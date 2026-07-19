#!/usr/bin/env python3
"""Convert the ELF32/i386 data objects used by the PC port to i386 COFF.

GNU objcopy does not translate i386 relocation numbers or section-symbol
indices correctly when changing these objects from ELF to COFF.  The source
objects use a deliberately small subset of ELF, so handling it directly keeps
the cross build deterministic and makes unsupported input fail loudly.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import struct
import sys


ELF_HEADER = struct.Struct("<16sHHIIIIIHHHHHH")
ELF_SECTION = struct.Struct("<IIIIIIIIII")
ELF_SYMBOL = struct.Struct("<IIIBBH")
ELF_RELOCATION = struct.Struct("<II")

COFF_HEADER = struct.Struct("<HHIIIHH")
COFF_SECTION = struct.Struct("<8sIIIIIIHHI")
COFF_SYMBOL = struct.Struct("<8sIhHBB")
COFF_RELOCATION = struct.Struct("<IIH")

ET_REL = 1
EM_386 = 3
ELFCLASS32 = 1
ELFDATA2LSB = 1

SHT_PROGBITS = 1
SHT_SYMTAB = 2
SHT_NOBITS = 8
SHT_REL = 9
SHF_WRITE = 0x1
SHF_ALLOC = 0x2
SHF_EXECINSTR = 0x4

SHN_UNDEF = 0
SHN_ABS = 0xFFF1
SHN_COMMON = 0xFFF2
STB_LOCAL = 0
STB_GLOBAL = 1
STT_SECTION = 3

IMAGE_FILE_MACHINE_I386 = 0x14C
IMAGE_SYM_UNDEFINED = 0
IMAGE_SYM_ABSOLUTE = -1
IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_SYM_CLASS_STATIC = 3

IMAGE_SCN_CNT_CODE = 0x00000020
IMAGE_SCN_CNT_INITIALIZED_DATA = 0x00000040
IMAGE_SCN_CNT_UNINITIALIZED_DATA = 0x00000080
IMAGE_SCN_MEM_EXECUTE = 0x20000000
IMAGE_SCN_MEM_READ = 0x40000000
IMAGE_SCN_MEM_WRITE = 0x80000000

ELF_TO_COFF_RELOCATION = {
    1: 0x0006,   # R_386_32 -> IMAGE_REL_I386_DIR32
    2: 0x0014,   # R_386_PC32 -> IMAGE_REL_I386_REL32
    20: 0x0001,  # R_386_16 -> IMAGE_REL_I386_DIR16
}


class FormatError(Exception):
    pass


def align(value: int, alignment: int) -> int:
    return (value + alignment - 1) & -alignment


def get_c_string(data: bytes, offset: int, context: str) -> str:
    if offset < 0 or offset >= len(data):
        raise FormatError(f"invalid {context} string offset {offset}")
    end = data.find(b"\0", offset)
    if end < 0:
        raise FormatError(f"unterminated {context} string at offset {offset}")
    try:
        return data[offset:end].decode("ascii")
    except UnicodeDecodeError as error:
        raise FormatError(f"non-ASCII {context} string at offset {offset}") from error


def read_struct(fmt: struct.Struct, data: bytes, offset: int, context: str) -> tuple:
    if offset < 0 or offset + fmt.size > len(data):
        raise FormatError(f"truncated {context}")
    return fmt.unpack_from(data, offset)


class StringTable:
    def __init__(self) -> None:
        self.data = bytearray(b"\0\0\0\0")
        self.offsets: dict[bytes, int] = {}

    def add(self, name: str) -> int:
        encoded = name.encode("ascii")
        existing = self.offsets.get(encoded)
        if existing is not None:
            return existing
        offset = len(self.data)
        self.data.extend(encoded)
        self.data.append(0)
        self.offsets[encoded] = offset
        return offset

    def symbol_name(self, name: str) -> bytes:
        encoded = name.encode("ascii")
        if len(encoded) <= 8:
            return encoded.ljust(8, b"\0")
        return struct.pack("<II", 0, self.add(name))

    def section_name(self, name: str) -> bytes:
        encoded = name.encode("ascii")
        if len(encoded) <= 8:
            return encoded.ljust(8, b"\0")
        reference = f"/{self.add(name)}".encode("ascii")
        if len(reference) > 8:
            raise FormatError(f"COFF string-table offset is too large for section {name!r}")
        return reference.ljust(8, b"\0")

    def finish(self) -> bytes:
        struct.pack_into("<I", self.data, 0, len(self.data))
        return bytes(self.data)


def section_characteristics(section: dict) -> int:
    alignment = section["alignment"] or 1
    if alignment & (alignment - 1) or alignment > 8192:
        raise FormatError(
            f"unsupported alignment {alignment} for section {section['name']!r}"
        )
    alignment_flag = alignment.bit_length() << 20
    flags = section["flags"]
    if section["type"] == SHT_NOBITS:
        characteristics = IMAGE_SCN_CNT_UNINITIALIZED_DATA
    elif flags & SHF_EXECINSTR:
        characteristics = IMAGE_SCN_CNT_CODE
    else:
        characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA
    characteristics |= IMAGE_SCN_MEM_READ | alignment_flag
    if flags & SHF_WRITE:
        characteristics |= IMAGE_SCN_MEM_WRITE
    if flags & SHF_EXECINSTR:
        characteristics |= IMAGE_SCN_MEM_EXECUTE
    return characteristics


def convert(input_path: Path, output_path: Path) -> None:
    data = bytearray(input_path.read_bytes())
    header = read_struct(ELF_HEADER, data, 0, "ELF header")
    ident = header[0]
    if ident[:4] != b"\x7fELF" or ident[4] != ELFCLASS32 or ident[5] != ELFDATA2LSB:
        raise FormatError("input is not a little-endian ELF32 object")
    if header[1] != ET_REL or header[2] != EM_386:
        raise FormatError("input is not an ELF32/i386 relocatable object")

    section_offset = header[6]
    section_entry_size = header[11]
    section_count = header[12]
    section_name_index = header[13]
    if section_entry_size != ELF_SECTION.size or not section_count:
        raise FormatError("unsupported ELF section table")
    if section_name_index >= section_count:
        raise FormatError("invalid ELF section-name table index")

    sections: list[dict] = []
    for index in range(section_count):
        raw = read_struct(
            ELF_SECTION,
            data,
            section_offset + index * section_entry_size,
            f"ELF section header {index}",
        )
        sections.append(
            {
                "name_offset": raw[0],
                "type": raw[1],
                "flags": raw[2],
                "offset": raw[4],
                "size": raw[5],
                "link": raw[6],
                "info": raw[7],
                "alignment": raw[8],
                "entry_size": raw[9],
            }
        )

    name_section = sections[section_name_index]
    name_data = data[name_section["offset"] : name_section["offset"] + name_section["size"]]
    if len(name_data) != name_section["size"]:
        raise FormatError("truncated ELF section-name table")
    for section in sections:
        section["name"] = get_c_string(name_data, section["name_offset"], "section")

    symbol_tables = [i for i, section in enumerate(sections) if section["type"] == SHT_SYMTAB]
    if len(symbol_tables) != 1:
        raise FormatError(f"expected one ELF symbol table, found {len(symbol_tables)}")
    symbol_table_index = symbol_tables[0]
    symbol_section = sections[symbol_table_index]
    if symbol_section["entry_size"] != ELF_SYMBOL.size:
        raise FormatError("unsupported ELF symbol-table entry size")
    if symbol_section["link"] >= section_count:
        raise FormatError("invalid ELF symbol string-table index")
    symbol_strings_section = sections[symbol_section["link"]]
    symbol_strings = data[
        symbol_strings_section["offset"] :
        symbol_strings_section["offset"] + symbol_strings_section["size"]
    ]
    if len(symbol_strings) != symbol_strings_section["size"]:
        raise FormatError("truncated ELF symbol string table")

    symbols: list[dict] = []
    symbol_count = symbol_section["size"] // ELF_SYMBOL.size
    for index in range(symbol_count):
        raw = read_struct(
            ELF_SYMBOL,
            data,
            symbol_section["offset"] + index * ELF_SYMBOL.size,
            f"ELF symbol {index}",
        )
        symbols.append(
            {
                "name": get_c_string(symbol_strings, raw[0], "symbol"),
                "value": raw[1],
                "size": raw[2],
                "binding": raw[3] >> 4,
                "type": raw[3] & 0xF,
                "section": raw[5],
            }
        )

    included = {
        index
        for index, section in enumerate(sections)
        if index and section["flags"] & SHF_ALLOC and section["size"]
    }
    for symbol in symbols:
        if (
            symbol["binding"] == STB_GLOBAL
            and symbol["section"] not in (SHN_UNDEF, SHN_ABS, SHN_COMMON)
        ):
            if symbol["section"] >= section_count:
                raise FormatError(f"symbol {symbol['name']!r} has an invalid section")
            included.add(symbol["section"])

    relocations: dict[int, list[tuple[int, int, int]]] = {index: [] for index in included}
    used_symbols: set[int] = set()
    for section in sections:
        if section["type"] != SHT_REL or section["info"] not in included:
            continue
        if section["link"] != symbol_table_index or section["entry_size"] != ELF_RELOCATION.size:
            raise FormatError(f"unsupported relocation section {section['name']!r}")
        count = section["size"] // ELF_RELOCATION.size
        for index in range(count):
            offset, info = read_struct(
                ELF_RELOCATION,
                data,
                section["offset"] + index * ELF_RELOCATION.size,
                f"ELF relocation {index} in {section['name']}",
            )
            symbol_index = info >> 8
            relocation_type = info & 0xFF
            if symbol_index >= symbol_count:
                raise FormatError(f"relocation in {section['name']!r} has an invalid symbol")
            if relocation_type not in ELF_TO_COFF_RELOCATION:
                raise FormatError(
                    f"unsupported i386 relocation {relocation_type} in {section['name']!r}"
                )
            symbol = symbols[symbol_index]
            if relocation_type == 20:
                if symbol["section"] != SHN_ABS:
                    raise FormatError(
                        f"16-bit relocation in {section['name']!r} targets unresolved "
                        f"symbol {symbol['name']!r}"
                    )
                target = sections[section["info"]]
                location = target["offset"] + offset
                addend = read_struct(struct.Struct("<H"), data, location, "16-bit addend")[0]
                relocated = symbol["value"] + addend
                if relocated > 0xFFFF:
                    raise FormatError(
                        f"16-bit relocation for {symbol['name']!r} overflows: {relocated}"
                    )
                struct.pack_into("<H", data, location, relocated)
                continue
            relocations[section["info"]].append((offset, symbol_index, relocation_type))
            used_symbols.add(symbol_index)

    included_sections = sorted(included)
    section_numbers = {elf_index: coff_index + 1 for coff_index, elf_index in enumerate(included_sections)}

    kept_symbol_indices: list[int] = []
    for index, symbol in enumerate(symbols):
        is_definition = symbol["section"] != SHN_UNDEF
        if index in used_symbols or (symbol["binding"] == STB_GLOBAL and is_definition):
            kept_symbol_indices.append(index)

    coff_symbol_indices = {elf_index: coff_index for coff_index, elf_index in enumerate(kept_symbol_indices)}
    missing_symbols = used_symbols.difference(coff_symbol_indices)
    if missing_symbols:
        raise FormatError(f"relocations reference omitted symbols: {sorted(missing_symbols)}")

    coff_symbols: list[tuple[str, int, int, int]] = []
    for elf_index in kept_symbol_indices:
        symbol = symbols[elf_index]
        elf_section = symbol["section"]
        if symbol["type"] == STT_SECTION:
            if elf_section not in section_numbers:
                raise FormatError("relocation references an omitted ELF section")
            name = sections[elf_section]["name"]
        else:
            name = "_" + symbol["name"]
        if not name:
            name = f".Lsym{elf_index}"

        if elf_section == SHN_UNDEF:
            section_number = IMAGE_SYM_UNDEFINED
            value = symbol["size"] if elf_section == SHN_COMMON else symbol["value"]
        elif elf_section == SHN_ABS:
            section_number = IMAGE_SYM_ABSOLUTE
            value = symbol["value"]
        elif elf_section == SHN_COMMON:
            section_number = IMAGE_SYM_UNDEFINED
            value = symbol["size"]
        else:
            if elf_section not in section_numbers:
                raise FormatError(f"kept symbol {symbol['name']!r} is in an omitted section")
            section_number = section_numbers[elf_section]
            value = symbol["value"]
        storage_class = (
            IMAGE_SYM_CLASS_STATIC if symbol["binding"] == STB_LOCAL
            else IMAGE_SYM_CLASS_EXTERNAL
        )
        coff_symbols.append((name, value, section_number, storage_class))

    header_size = COFF_HEADER.size + len(included_sections) * COFF_SECTION.size
    cursor = align(header_size, 4)
    section_layouts: list[dict] = []
    for elf_index in included_sections:
        section = sections[elf_index]
        if section["type"] == SHT_NOBITS:
            raw_data = b""
            raw_pointer = 0
            raw_size = section["size"]
        elif section["type"] == SHT_PROGBITS:
            raw_data = data[section["offset"] : section["offset"] + section["size"]]
            if len(raw_data) != section["size"]:
                raise FormatError(f"truncated section {section['name']!r}")
            raw_pointer = cursor
            raw_size = len(raw_data)
            cursor = align(cursor + raw_size, 4)
        else:
            raise FormatError(f"unsupported allocated section type for {section['name']!r}")

        section_relocations = relocations.get(elf_index, [])
        if len(section_relocations) > 0xFFFF:
            raise FormatError(f"too many relocations in section {section['name']!r}")
        relocation_pointer = cursor if section_relocations else 0
        cursor = align(cursor + len(section_relocations) * COFF_RELOCATION.size, 4)
        section_layouts.append(
            {
                "elf_index": elf_index,
                "raw_data": raw_data,
                "raw_pointer": raw_pointer,
                "raw_size": raw_size,
                "relocations": section_relocations,
                "relocation_pointer": relocation_pointer,
            }
        )

    symbol_table_pointer = cursor
    cursor += len(coff_symbols) * COFF_SYMBOL.size
    strings = StringTable()

    output = bytearray(cursor)
    COFF_HEADER.pack_into(
        output,
        0,
        IMAGE_FILE_MACHINE_I386,
        len(included_sections),
        0,
        symbol_table_pointer,
        len(coff_symbols),
        0,
        0,
    )

    for index, layout in enumerate(section_layouts):
        section = sections[layout["elf_index"]]
        COFF_SECTION.pack_into(
            output,
            COFF_HEADER.size + index * COFF_SECTION.size,
            strings.section_name(section["name"]),
            0,
            0,
            layout["raw_size"],
            layout["raw_pointer"],
            layout["relocation_pointer"],
            0,
            len(layout["relocations"]),
            0,
            section_characteristics(section),
        )
        if layout["raw_data"]:
            start = layout["raw_pointer"]
            output[start : start + len(layout["raw_data"])] = layout["raw_data"]
        for relocation_index, (offset, elf_symbol_index, elf_type) in enumerate(
            layout["relocations"]
        ):
            COFF_RELOCATION.pack_into(
                output,
                layout["relocation_pointer"] + relocation_index * COFF_RELOCATION.size,
                offset,
                coff_symbol_indices[elf_symbol_index],
                ELF_TO_COFF_RELOCATION[elf_type],
            )

    for index, (name, value, section_number, storage_class) in enumerate(coff_symbols):
        COFF_SYMBOL.pack_into(
            output,
            symbol_table_pointer + index * COFF_SYMBOL.size,
            strings.symbol_name(name),
            value,
            section_number,
            0,
            storage_class,
            0,
        )

    output.extend(strings.finish())
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = output_path.with_name(output_path.name + ".tmp")
    temporary.write_bytes(output)
    os.replace(temporary, output_path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()
    try:
        convert(arguments.input, arguments.output)
    except (FormatError, OSError) as error:
        print(f"elf2coff: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
