#!/usr/bin/env python3
"""
compile_battle_scripts.py - Option A: delta-offset pointer encoding for desktop

Converts pokeemerald battle scripts (and AI scripts) from ARM ELF into a flat
C byte array suitable for 64-bit desktop use.

Both battle_scripts_*.s and battle_ai_scripts.s are assembled and merged into
a single gBattleScriptData[] blob.  All internal script/AI pointers are stored
as 32-bit LE offsets from gBattleScriptData[0]; C variable references are
encoded as 0x80000000 | (varIndex << 16) | addend.

Runtime decoding (T1_READ_PTR / T2_READ_PTR override in battle_desktop/include/global.h):
  v = read_u32_le(ptr)
  if v & 0x80000000:
      idx    = (v >> 16) & 0x7FFF
      addend = v & 0xFFFF
      return (u8*)gBattleVarAddresses[idx] + addend
  else:
      return gBattleScriptBase + v

Usage: python3 battle_desktop/tools/compile_battle_scripts.py
(run from pokeemerald root directory)

Outputs:
  battle_desktop/generated/battle_scripts.c
  battle_desktop/generated/battle_scripts.h
"""

import subprocess
import struct
import os
import sys

REPO_ROOT = os.getcwd()
PREPROC   = os.path.join(REPO_ROOT, 'tools', 'preproc', 'preproc_native.exe')
AS        = 'arm-none-eabi-as'
AS_FLAGS  = ['-mcpu=arm7tdmi', '-mthumb-interwork', '-I', '.']
NM        = 'arm-none-eabi-nm'
READELF   = 'arm-none-eabi-readelf'
OBJCOPY   = 'arm-none-eabi-objcopy'

SCRIPT_FILES = [
    'data/battle_scripts_1.s',
    'data/battle_scripts_2.s',
]

AI_SCRIPT_FILE = 'data/battle_ai_scripts.s'

OUT_C = 'battle_desktop/generated/battle_scripts.c'
OUT_H = 'battle_desktop/generated/battle_scripts.h'

# ---------------------------------------------------------------------------
def assemble_script(src_path):
    """Preprocess + assemble a .s file, return path to object file."""
    obj = f'/tmp/{os.path.basename(src_path)}.o'
    charmap = os.path.join(REPO_ROOT, 'charmap.txt')
    p1 = subprocess.Popen([PREPROC, src_path, charmap],
                          stdout=subprocess.PIPE, cwd=REPO_ROOT)
    p2 = subprocess.Popen(['gcc', '-E', '-Iinclude', '-x', 'assembler-with-cpp', '-'],
                          stdin=p1.stdout, stdout=subprocess.PIPE, cwd=REPO_ROOT)
    p1.stdout.close()
    p3 = subprocess.Popen([PREPROC, '-ie', src_path, charmap],
                          stdin=p2.stdout, stdout=subprocess.PIPE, cwd=REPO_ROOT)
    p2.stdout.close()
    p4 = subprocess.Popen([AS] + AS_FLAGS + ['-o', obj, '-'],
                          stdin=p3.stdout, cwd=REPO_ROOT)
    p3.stdout.close()
    p4.wait()
    if p4.returncode != 0:
        sys.exit(f'Assembler failed for {src_path}')
    return obj


def read_section_binary(obj_path, section='script_data'):
    """Extract raw bytes of a named section from an ELF object."""
    bin_path = obj_path + '.bin'
    subprocess.run([OBJCOPY, '-O', 'binary', '--only-section=' + section,
                    obj_path, bin_path], check=True)
    with open(bin_path, 'rb') as f:
        data = bytearray(f.read())
    os.unlink(bin_path)
    return data


def read_symbols(obj_path):
    """Return dict: symbol_name -> local_offset for defined symbols in script_data."""
    out = subprocess.check_output([NM, '--numeric-sort', obj_path]).decode()
    syms = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) < 3 or parts[0] == '':
            continue
        try:
            offset = int(parts[0], 16)
        except ValueError:
            continue
        typ  = parts[1]
        name = parts[2]
        # D/d = initialized data, T/t = text, R/r = read-only data.
        # Exclude A/a (absolute equates like B_BATTLER_0, BG_PLTT) — those
        # are GAS .equiv/.set constants, not actual labels in the section.
        if typ in ('D', 'd', 'T', 't', 'R', 'r'):
            syms[name] = offset
    return syms


def read_relocations(obj_path, section='script_data'):
    """Return list of (offset_in_section, sym_name) for R_ARM_ABS32 relocs."""
    out = subprocess.check_output([READELF, '-r', '-W', obj_path]).decode()
    relocs = []
    in_section = False
    for line in out.splitlines():
        # Detect section header e.g. "Relocation section '.relscript_data' ..."
        if "'.rel" + section + "'" in line or f"[.rel{section}]" in line:
            in_section = True
            continue
        if in_section:
            # Stop at next "Relocation section" for a different section
            if line.startswith('Relocation section') and section not in line:
                in_section = False
                continue
            parts = line.split()
            if not parts or parts[0].startswith('Off') or not parts[0][0].isdigit():
                continue
            if len(parts) >= 5 and 'ABS32' in parts[2]:
                try:
                    offset   = int(parts[0], 16)
                    sym_name = parts[4]
                    relocs.append((offset, sym_name))
                except (ValueError, IndexError):
                    continue
    return relocs


# ---------------------------------------------------------------------------
def main():
    os.makedirs(os.path.dirname(OUT_C), exist_ok=True)

    # ---- Pass 1a: assemble battle script files, collect symbol tables -----
    sections = []   # [(bytearray, local_syms, relocs, base_offset)]
    all_script_syms = {}   # name -> absolute offset in merged data

    base_offset = 0
    for src in SCRIPT_FILES:
        print(f'  Assembling {src}...')
        obj  = assemble_script(src)
        data = read_section_binary(obj)
        syms = read_symbols(obj)
        relocs = read_relocations(obj)
        # Adjust to absolute offsets in the merged array
        for name, off in syms.items():
            all_script_syms[name] = base_offset + off
        sections.append((data, syms, relocs, base_offset))
        base_offset += len(data)

    battle_script_data_size = base_offset  # where AI data begins

    # ---- Pass 1b: assemble AI scripts -------------------------------------
    print(f'  Assembling {AI_SCRIPT_FILE}...')
    ai_obj    = assemble_script(AI_SCRIPT_FILE)
    ai_data   = read_section_binary(ai_obj)
    ai_syms   = read_symbols(ai_obj)
    ai_relocs = read_relocations(ai_obj)
    ai_base   = base_offset  # absolute offset where AI data starts in merged blob
    # Merge AI symbols into the unified namespace
    for name, off in ai_syms.items():
        all_script_syms[name] = ai_base + off
    base_offset += len(ai_data)

    # Everything in all_script_syms is a script-section symbol -> delta-encode
    # 'script_data' is the section itself -> also delta-encode
    def is_script_sym(name):
        return name in all_script_syms or name == 'script_data'

    # Collect all C variable symbols from both battle and AI relocs
    all_var_syms = set()
    for _, _, relocs, _ in sections:
        for _, sym_name in relocs:
            if not is_script_sym(sym_name):
                all_var_syms.add(sym_name)
    for _, sym_name in ai_relocs:
        if not is_script_sym(sym_name):
            all_var_syms.add(sym_name)

    var_list  = sorted(all_var_syms)
    var_index = {name: i for i, name in enumerate(var_list)}
    print(f'  {len(all_script_syms)} script/AI labels, {len(var_list)} C variable symbols')

    # ---- Pass 2a: patch battle script relocations -------------------------
    full_data = bytearray()
    for data, _, relocs, base in sections:
        for rel_off, sym_name in relocs:
            # Read inline addend (REL format, LE 32-bit)
            addend = struct.unpack_from('<I', data, rel_off)[0]
            if is_script_sym(sym_name):
                # Script pointer: store absolute offset in merged data
                if sym_name == 'script_data':
                    # Self-referential: addend is local offset within this section
                    target = base + addend
                else:
                    target = all_script_syms[sym_name] + addend
                struct.pack_into('<I', data, rel_off, target)
            else:
                # C variable pointer: encode as 0x80000000 | (idx<<16) | addend
                idx     = var_index[sym_name]
                encoded = 0x80000000 | (idx << 16) | (addend & 0xFFFF)
                struct.pack_into('<I', data, rel_off, encoded)
        full_data.extend(data)

    # ---- Pass 2b: patch AI script relocations -----------------------------
    for rel_off, sym_name in ai_relocs:
        addend = struct.unpack_from('<I', ai_data, rel_off)[0]
        if is_script_sym(sym_name):
            if sym_name == 'script_data':
                # Self-referential within AI data: addend is local offset
                target = ai_base + addend
            else:
                target = all_script_syms[sym_name] + addend
            struct.pack_into('<I', ai_data, rel_off, target)
        else:
            idx     = var_index[sym_name]
            encoded = 0x80000000 | (idx << 16) | (addend & 0xFFFF)
            struct.pack_into('<I', ai_data, rel_off, encoded)
    full_data.extend(ai_data)

    # ---- Extract gBattleScriptsForMoveEffects table -----------------------
    # The table lives at offset 0 in battle_scripts_1.s script_data; each
    # entry is a 4-byte delta to a BattleScript_Effect* label.
    effects_relocs = []  # (entry_index, label_name) ordered by entry index
    for _, _, relocs, base in sections[:1]:  # only file 1 has the table at offset 0
        for rel_off, sym_name in relocs:
            if rel_off < all_script_syms.get('BattleScript_EffectHit', 0xFFFFFFFF) - sections[0][3]:
                # This reloc is within the table
                entry_idx = rel_off // 4
                effects_relocs.append((entry_idx, sym_name))
    effects_relocs.sort(key=lambda x: x[0])
    num_effects = len(effects_relocs)

    # ---- Extract gBattleAI_ScriptsTable -----------------------------------
    # The AI scripts table lives at offset 0 in battle_ai_scripts.s.
    # It contains one .4byte pointer per AI script index.
    # Determine table size: bytes from gBattleAI_ScriptsTable to first script label.
    ai_table_local_start = ai_syms.get('gBattleAI_ScriptsTable', 0)
    # Find the smallest local offset that is > ai_table_local_start
    # (i.e., the first actual script following the table).
    next_label_local = min(
        (off for name, off in ai_syms.items() if off > ai_table_local_start),
        default=ai_table_local_start + 32 * 4
    )
    num_ai_entries = (next_label_local - ai_table_local_start) // 4

    # Read the patched table entries from the merged full_data
    ai_table_abs_start = ai_base + ai_table_local_start
    ai_table_entries = []
    for i in range(num_ai_entries):
        entry_offset = struct.unpack_from('<I', full_data, ai_table_abs_start + i * 4)[0]
        ai_table_entries.append(entry_offset)

    # ---- Generate header --------------------------------------------------
    with open(OUT_H, 'w') as f:
        f.write('/* AUTO-GENERATED by compile_battle_scripts.py - do not edit */\n')
        f.write('#pragma once\n')
        f.write('#ifdef __cplusplus\nextern "C" {\n#endif\n\n')
        f.write('#include "global.h"\n\n')
        f.write('/* Raw unified script+AI data blob */\n')
        f.write(f'extern const u8 gBattleScriptData[];\n')
        f.write('extern const u8 * const gBattleScriptBase;\n\n')
        f.write('/* C variable address table (shared by battle scripts and AI scripts) */\n')
        f.write(f'#define BATTLE_SCRIPT_NUM_VARS {len(var_list)}\n')
        f.write('extern void *gBattleVarAddresses[BATTLE_SCRIPT_NUM_VARS];\n')
        f.write('void InitBattleScriptVarTable(void);\n\n')
        f.write('/* AI scripts table */\n')
        f.write(f'extern const u8 *const gBattleAI_ScriptsTable[{num_ai_entries}];\n\n')
        f.write('/* Variable index constants */\n')
        for i, name in enumerate(var_list):
            f.write(f'#define BSVAR_{name.upper()} {i}\n')
        # Labels to skip — either declared elsewhere (battle.h) or emitted as
        # proper C arrays rather than macros.
        SKIP_LABEL_DECLS = {
            'gBattleScriptsForMoveEffects',
            'gBattlescriptsForBallThrow',
            'gBattlescriptsForUsingItem',
            'gBattlescriptsForRunningByItem',
            'gBattlescriptsForSafariActions',
            'gBattleAI_ScriptsTable',
        }
        # Emit battle script labels as macros expanding to &gBattleScriptData[offset].
        # AI script labels (AI_CheckBadMove, etc.) are internal implementation detail
        # and not exported — only gBattleAI_ScriptsTable needs external visibility.
        f.write('\n/* Battle script label macros — expand to &gBattleScriptData[offset] */\n')
        for name in sorted(all_script_syms):
            if name in SKIP_LABEL_DECLS:
                continue
            # Skip AI-internal labels (not needed outside the AI engine)
            if name.startswith('AI_') or name.startswith('Score_') or name.startswith('CheckIf'):
                continue
            off = all_script_syms[name]
            f.write(f'#define {name} (&gBattleScriptData[{off}])\n')
        f.write('\n#ifdef __cplusplus\n}\n#endif\n')

    # ---- Generate C source ------------------------------------------------
    with open(OUT_C, 'w') as f:
        f.write('/* AUTO-GENERATED by compile_battle_scripts.py - do not edit */\n')
        f.write('#include "global.h"\n')
        f.write('#include "battle.h"\n')
        f.write('#include "battle_message.h"\n')
        f.write('#include "battle_setup.h"\n')
        f.write('#include "safari_zone.h"\n')
        f.write('#include "battle_desktop/generated/battle_scripts.h"\n\n')

        # Extern declarations for variables referenced by battle/AI scripts that are
        # NOT declared in the included headers (battle.h / battle_message.h).
        BATTLE_H_VARS = {
            'gBattleCommunication', 'gBattleMoveDamage', 'gBattleOutcome',
            'gBattleScripting', 'gBattleTextBuff1', 'gBattleTypeFlags',
            'gBattleWeather', 'gBattlerAttacker', 'gBattlerByTurnOrder',
            'gBattlerFainted', 'gBattlerTarget', 'gBattlersCount',
            'gChosenMove', 'gCritMultiplier', 'gCurrentMove',
            'gDynamicBasePower', 'gEffectBattler', 'gHitMarker',
            'gHpDealt', 'gLastUsedItem', 'gMoveResultFlags',
            'gTrainerBattleOpponent_A',  # battle_setup.h
            'gNumSafariBalls',           # safari_zone.h
            'gMissStringIds',  # declared in battle_message.h
        }
        missing_externs = [n for n in var_list if n not in BATTLE_H_VARS]
        if missing_externs:
            f.write('/* Extern declarations for arrays with no header declaration */\n')
            for name in missing_externs:
                f.write(f'extern const u16 {name}[];\n')
            f.write('\n')

        # Raw byte array (battle scripts + AI scripts merged)
        f.write(f'const u8 gBattleScriptData[{len(full_data)}] = {{\n')
        for i, b in enumerate(full_data):
            if i % 16 == 0:
                f.write('    ')
            f.write(f'0x{b:02X},')
            if i % 16 == 15:
                f.write('\n')
            else:
                f.write(' ')
        if len(full_data) % 16 != 0:
            f.write('\n')
        f.write('};\n\n')

        f.write('const u8 * const gBattleScriptBase = gBattleScriptData;\n')
        f.write('const uint32_t gBattleScriptDataSize = sizeof(gBattleScriptData);\n\n')

        # gBattleScriptsForMoveEffects - real pointer array so engine can index directly.
        f.write(f'/* Real effect->script lookup (overrides data_stubs.c stub) */\n')
        f.write(f'const u8 *const gBattleScriptsForMoveEffects[{num_effects}] = {{\n')
        for idx, label in effects_relocs:
            off = all_script_syms.get(label, 0)
            f.write(f'    /* [{idx}] {label} */ &gBattleScriptData[{off}],\n')
        f.write('};\n\n')

        # gBattleAI_ScriptsTable - real pointer array built from the patched AI data.
        # Each entry is &gBattleScriptData[absolute_offset] pointing into the AI blob.
        f.write(f'/* Real AI scripts table (overrides data_stubs.c stub) */\n')
        f.write(f'const u8 *const gBattleAI_ScriptsTable[{num_ai_entries}] = {{\n')
        for i, offset in enumerate(ai_table_entries):
            f.write(f'    /* [{i}] */ &gBattleScriptData[{offset}],\n')
        f.write('};\n\n')

        # Variable address table
        f.write(f'void *gBattleVarAddresses[BATTLE_SCRIPT_NUM_VARS];\n\n')
        f.write('void InitBattleScriptVarTable(void) {\n')
        for i, name in enumerate(var_list):
            f.write(f'    gBattleVarAddresses[{i}] = (void *)&{name}; /* BSVAR_{name.upper()} */\n')
        f.write('}\n')

    total_kb = len(full_data) / 1024
    battle_kb = battle_script_data_size / 1024
    ai_kb = len(ai_data) / 1024
    print(f'  Generated {OUT_C} ({total_kb:.1f} KB total: {battle_kb:.1f} KB battle + {ai_kb:.1f} KB AI)')
    print(f'    {num_effects} move effects, {num_ai_entries} AI script entries, {len(all_script_syms)} labels')
    print(f'  Generated {OUT_H}')


if __name__ == '__main__':
    print('Compiling battle scripts + AI scripts (Option A: delta-offset encoding)...')
    main()
    print('Done.')
