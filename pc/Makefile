ROOT := .
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c

PC_DIR := pc
BUILD_DIR := build/pc
OBJ_DIR := $(BUILD_DIR)/obj
GAME_OBJ_DIR := $(OBJ_DIR)/game
PLATFORM_OBJ_DIR := $(OBJ_DIR)/platform
DATA_OBJ_DIR := $(OBJ_DIR)/data
SONG_OBJ_DIR := $(OBJ_DIR)/songs
WIN_BUILD_DIR := $(BUILD_DIR)/windows
WIN_OBJ_DIR := $(WIN_BUILD_DIR)/obj
WIN_GAME_OBJ_DIR := $(WIN_OBJ_DIR)/game
WIN_PLATFORM_OBJ_DIR := $(WIN_OBJ_DIR)/platform
WIN_DATA_OBJ_DIR := $(WIN_OBJ_DIR)/data
WIN_SONG_OBJ_DIR := $(WIN_OBJ_DIR)/songs

FRONTEND_CC ?= cc
PC_AS ?= as
PC_LD ?= ld
ZIG ?= zig
UV ?= uv --cache-dir $(abspath $(BUILD_DIR)/uv-cache)
ZIG_CACHE := $(BUILD_DIR)/zig-cache
CORE_CC = env ZIG_GLOBAL_CACHE_DIR=$(abspath $(ZIG_CACHE)/global) ZIG_LOCAL_CACHE_DIR=$(abspath $(ZIG_CACHE)/local) $(ZIG) cc -target x86-linux-musl
WIN_FRONTEND_CC = env ZIG_GLOBAL_CACHE_DIR=$(abspath $(ZIG_CACHE)/global) ZIG_LOCAL_CACHE_DIR=$(abspath $(ZIG_CACHE)/local) $(ZIG) cc -target x86_64-windows-gnu
WIN_CORE_CC = env ZIG_GLOBAL_CACHE_DIR=$(abspath $(ZIG_CACHE)/global) ZIG_LOCAL_CACHE_DIR=$(abspath $(ZIG_CACHE)/local) $(ZIG) cc -target x86-windows-gnu

PREPROC := tools/preproc/preproc
ELF2COFF := $(PC_DIR)/tools/elf2coff.py
PC_CPPFLAGS := -DPLATFORM_PC=1 -DMODERN=1 \
	-I$(PC_DIR)/include -iquote include -iquote .
PC_CFLAGS := -std=gnu11 -O1 -g -fcommon -fno-pic -fno-pie \
	-Wall -Wextra -Wno-address-of-packed-member -Wno-constant-logical-operand \
	-Wno-gnu-alignof-expression -Wno-ignored-attributes -Wno-implicit-function-declaration \
	-Wno-incompatible-pointer-types-discards-qualifiers -Wno-pointer-to-int-cast \
	-Wno-tautological-constant-out-of-range-compare -Wno-unknown-attributes \
	-Wno-unused-function -Wno-unused-variable
WIN_PC_CFLAGS := $(filter-out -fno-pic -fno-pie,$(PC_CFLAGS)) -mno-ms-bitfields
PC_HEADERS := $(wildcard $(PC_DIR)/include/*.h)
GAME_HEADERS := $(shell find include -type f -name '*.h') $(wildcard *.h)

SDL_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null || printf '%s' '-Wl,-l:libSDL2-2.0.so.0')

FRONTEND := $(BUILD_DIR)/pokeemerald-pc
CORE := $(BUILD_DIR)/pokeemerald-core
WIN_FRONTEND := $(WIN_BUILD_DIR)/pokeemerald-pc.exe
WIN_CORE := $(WIN_BUILD_DIR)/pokeemerald-core.exe
WIN_ASM_ELF := $(WIN_OBJ_DIR)/pokeemerald-asm.elf.o
WIN_ASM_OBJ := $(WIN_OBJ_DIR)/pokeemerald-asm.o

SDL2_VERSION := 2.32.10
SDL2_ARCHIVE := $(BUILD_DIR)/deps/SDL2-devel-$(SDL2_VERSION)-mingw.tar.gz
SDL2_SHA256 := 83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2
SDL2_URL := https://github.com/libsdl-org/SDL/releases/download/release-$(SDL2_VERSION)/SDL2-devel-$(SDL2_VERSION)-mingw.tar.gz
SDL2_EXTRACT_DIR := $(BUILD_DIR)/deps/SDL2-mingw
SDL2_STAMP := $(SDL2_EXTRACT_DIR)/.extracted-$(SDL2_VERSION)
SDL2_WIN64_DIR := $(SDL2_EXTRACT_DIR)/SDL2-$(SDL2_VERSION)/x86_64-w64-mingw32
WIN_SDL_DLL := $(WIN_BUILD_DIR)/SDL2.dll
WIN_SDL_LICENSE := $(WIN_BUILD_DIR)/SDL2-LICENSE.txt

GAME_C_SRCS_ALL := $(wildcard src/*.c)
GAME_C_SRCS_EXCLUDED := \
	src/agb_flash.c \
	src/agb_flash_1m.c \
	src/agb_flash_le.c \
	src/agb_flash_mx.c \
	src/libisagbprn.c \
	src/librfu_intr.c \
	src/multiboot.c \
	src/rom_header_gf.c \
	src/siirtc.c
GAME_C_SRCS := $(filter-out $(GAME_C_SRCS_EXCLUDED),$(GAME_C_SRCS_ALL))
GAME_C_OBJS := $(patsubst src/%.c,$(GAME_OBJ_DIR)/%.o,$(GAME_C_SRCS))
WIN_GAME_C_OBJS := $(patsubst src/%.c,$(WIN_GAME_OBJ_DIR)/%.o,$(GAME_C_SRCS))

PLATFORM_C_SRCS := \
	$(PC_DIR)/audio.c \
	$(PC_DIR)/bios.c \
	$(PC_DIR)/core_main.c \
	$(PC_DIR)/platform.c \
	$(PC_DIR)/ppu.c \
	$(PC_DIR)/services.c
PLATFORM_C_OBJS := $(patsubst $(PC_DIR)/%.c,$(PLATFORM_OBJ_DIR)/%.o,$(PLATFORM_C_SRCS))
WIN_PLATFORM_C_OBJS := $(patsubst $(PC_DIR)/%.c,$(WIN_PLATFORM_OBJ_DIR)/%.o,$(PLATFORM_C_SRCS))

DATA_S_SRCS := $(wildcard data/*.s)
DATA_S_OBJS := $(patsubst data/%.s,$(DATA_OBJ_DIR)/%.o,$(DATA_S_SRCS))
SONG_S_SRCS := $(wildcard sound/songs/midi/*.s)
SONG_S_OBJS := $(patsubst sound/songs/midi/%.s,$(SONG_OBJ_DIR)/%.o,$(SONG_S_SRCS))
.PHONY: all frontend core core-objects windows windows-core-objects clean check-assets

all: frontend core

frontend: $(FRONTEND)

core: $(CORE)

core-objects: check-assets $(PLATFORM_C_OBJS) $(GAME_C_OBJS) $(DATA_S_OBJS) $(SONG_S_OBJS)

windows: $(WIN_FRONTEND) $(WIN_CORE) $(WIN_SDL_DLL) $(WIN_SDL_LICENSE)

windows-core-objects: check-assets $(WIN_PLATFORM_C_OBJS) $(WIN_GAME_C_OBJS) $(WIN_ASM_OBJ)

$(FRONTEND): $(PC_DIR)/frontend.c $(PC_HEADERS)
	@mkdir -p $(@D)
	$(FRONTEND_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -I$(PC_DIR)/include $< $(SDL_LIBS) -o $@

$(PLATFORM_OBJ_DIR)/%.o: $(PC_DIR)/%.c $(PC_HEADERS) $(GAME_HEADERS)
	@mkdir -p $(@D) $(ZIG_CACHE)/global $(ZIG_CACHE)/local
	$(CORE_CC) $(PC_CPPFLAGS) $(PC_CFLAGS) -O3 -c $< -o $@

$(GAME_OBJ_DIR)/%.o: src/%.c $(GAME_HEADERS) $(PREPROC) | check-assets
	@mkdir -p $(@D) $(ZIG_CACHE)/global $(ZIG_CACHE)/local
	@echo "PC CC $<"
	@$(CORE_CC) $(PC_CPPFLAGS) -E $< \
		| $(PREPROC) -i -g build/assets $< charmap.txt \
		| $(CORE_CC) $(PC_CPPFLAGS) $(PC_CFLAGS) -x c -c -o $@ -

$(WIN_GAME_OBJ_DIR)/%.o: src/%.c $(GAME_HEADERS) $(PREPROC) | check-assets
	@mkdir -p $(@D) $(ZIG_CACHE)/global $(ZIG_CACHE)/local
	@echo "WIN PC CC $<"
	@$(WIN_CORE_CC) $(PC_CPPFLAGS) -E $< \
		| $(PREPROC) -i -g build/assets $< charmap.txt \
		| $(WIN_CORE_CC) $(PC_CPPFLAGS) $(WIN_PC_CFLAGS) -x c -c -o $@ -

$(DATA_OBJ_DIR)/%.o: data/%.s $(PREPROC)
	@mkdir -p $(@D) $(ZIG_CACHE)/global $(ZIG_CACHE)/local
	@echo "PC AS $<"
	@$(PREPROC) $< charmap.txt \
		| $(CORE_CC) -E -x assembler-with-cpp -Iinclude -I. - \
		| $(PREPROC) -ie $< charmap.txt \
		| $(PC_AS) --32 -I. -o $@ -

$(SONG_OBJ_DIR)/%.o: sound/songs/midi/%.s sound/MPlayDef.s $(PREPROC)
	@mkdir -p $(@D) $(ZIG_CACHE)/global $(ZIG_CACHE)/local
	@echo "PC AS $<"
	@cd sound && ../$(PREPROC) songs/midi/$*.s ../charmap.txt \
		| $(CORE_CC) -Dword=long -E -x assembler-with-cpp -I../include -I.. -I. - \
		| ../$(PREPROC) -ie songs/midi/$*.s ../charmap.txt \
		| $(PC_AS) --32 -I. -o ../$@ -

$(WIN_PLATFORM_OBJ_DIR)/%.o: $(PC_DIR)/%.c $(PC_HEADERS) $(GAME_HEADERS)
	@mkdir -p $(@D) $(ZIG_CACHE)/global $(ZIG_CACHE)/local
	$(WIN_CORE_CC) $(PC_CPPFLAGS) $(WIN_PC_CFLAGS) -O3 -c $< -o $@

$(WIN_ASM_ELF): $(DATA_S_OBJS) $(SONG_S_OBJS)
	@mkdir -p $(@D) $(ZIG_CACHE)/global $(ZIG_CACHE)/local
	@echo "WIN PC LD $@"
	@$(PC_LD) -m elf_i386 -r -z noexecstack $^ -o $@

$(WIN_ASM_OBJ): $(WIN_ASM_ELF) $(ELF2COFF)
	@mkdir -p $(@D) $(ZIG_CACHE)/global $(ZIG_CACHE)/local
	@echo "WIN PC COFF $<"
	@$(UV) run python $(ELF2COFF) $< $@

$(PREPROC):
	$(MAKE) -f make_tools.mk

check-assets:
	@test -d build/assets || { \
		echo "PC assets are missing; build the ROM assets once with make modern" >&2; \
		exit 1; \
	}

$(CORE): $(PLATFORM_C_OBJS) $(GAME_C_OBJS) $(DATA_S_OBJS) $(SONG_S_OBJS) | check-assets
	@mkdir -p $(@D) $(ZIG_CACHE)/global $(ZIG_CACHE)/local
	$(CORE_CC) -static -fno-pie -no-pie \
		$(PLATFORM_C_OBJS) $(GAME_C_OBJS) $(DATA_S_OBJS) $(SONG_S_OBJS) \
		-lm -o $@

$(SDL2_ARCHIVE):
	@mkdir -p $(@D)
	curl -L $(SDL2_URL) -o $@.tmp
	@printf '%s  %s\n' '$(SDL2_SHA256)' '$@.tmp' | sha256sum -c -
	mv $@.tmp $@

$(SDL2_STAMP): $(SDL2_ARCHIVE)
	@mkdir -p $(SDL2_EXTRACT_DIR)
	tar -xf $< -C $(SDL2_EXTRACT_DIR)
	@touch $@

$(WIN_FRONTEND): $(PC_DIR)/frontend.c $(PC_HEADERS) $(SDL2_STAMP)
	@mkdir -p $(@D) $(ZIG_CACHE)/global $(ZIG_CACHE)/local
	$(WIN_FRONTEND_CC) -std=c11 -O2 -g -Wall -Wextra -Werror -I$(PC_DIR)/include \
		$< $(SDL2_WIN64_DIR)/lib/libSDL2.dll.a -o $@

$(WIN_CORE): $(WIN_PLATFORM_C_OBJS) $(WIN_GAME_C_OBJS) $(WIN_ASM_OBJ) | check-assets
	@mkdir -p $(@D) $(ZIG_CACHE)/global $(ZIG_CACHE)/local
	$(WIN_CORE_CC) -Wl,--image-base,0x10000000 -Wl,--stack,1048576 -Wl,--no-dynamicbase \
		$(WIN_PLATFORM_C_OBJS) $(WIN_GAME_C_OBJS) $(WIN_ASM_OBJ) \
		-lm -o $@

$(WIN_SDL_DLL): $(SDL2_STAMP)
	@mkdir -p $(@D)
	cp $(SDL2_WIN64_DIR)/bin/SDL2.dll $@

$(WIN_SDL_LICENSE): $(SDL2_STAMP)
	@mkdir -p $(@D)
	cp $(SDL2_EXTRACT_DIR)/SDL2-$(SDL2_VERSION)/LICENSE.txt $@

clean:
	rm -rf $(BUILD_DIR)
