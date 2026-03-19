# Makefile for the pokeemerald desktop battle engine
#
# Compiles the authentic Gen III battle system into a standalone
# console program. Shadow headers in battle_desktop/include/ override
# GBA-specific headers; all graphics/sound are no-ops.
#
# Usage:
#   make          - build the executable
#   make clean    - remove build artifacts
#   make run      - build and run

# ---------------------------------------------------------------------------
# Cross-platform helpers
# MSYSTEM is set by MSYS2; its absence on Windows means native cmd.exe make.
# ---------------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
  ifndef MSYSTEM
    # Native Windows: force cmd.exe so our rd/md commands work even when sh.exe
    # (e.g. from Git for Windows) is on PATH and would otherwise be picked up.
    SHELL   = cmd.exe
    # md creates nested dirs; 2>nul suppresses "already exists" error; & exit 0 forces success.
    MKDIR   = md $(subst /,\,$1) 2>nul & exit 0
    RM_DIR  = rd /s /q $(subst /,\,$1) 2>nul & exit 0
    RM_FILE = del /f /q $(subst /,\,$1) 2>nul & exit 0
  else
    # MSYS2 — bash available
    MKDIR   = mkdir -p $1
    RM_DIR  = rm -rf $1
    RM_FILE = rm -f $1
  endif
else
  # Linux / macOS
  MKDIR   = mkdir -p $1
  RM_DIR  = rm -rf $1
  RM_FILE = rm -f $1
endif

CC      := gcc
CFLAGS  := -std=c11 -DMODERN=1 -DENGLISH -DNDEBUG \
            -Ibattle_desktop/include \
            -Iinclude \
            -I. \
            -Wall -Wno-unused-function -Wno-unused-variable \
            -Wno-missing-braces \
            -O2 -g
LDFLAGS := -lm

# Output (.exe suffix on Windows — MinGW gcc always appends it when linking)
ifeq ($(OS),Windows_NT)
  EXE := .exe
  DLL := .dll
else
  EXE :=
  DLL := .so
endif
TARGET  := battle_desktop/battle_desktop$(EXE)
LIB_TARGET := battle_desktop/battle_desktop$(DLL)

# Build directory
OBJDIR  := battle_desktop/build

# -----------------------------------------------------------------------
# Desktop-specific source files
# -----------------------------------------------------------------------
DESKTOP_SRCS := \
    battle_desktop/main.c \
    battle_desktop/console_controller.c \
    battle_desktop/data_transfer.c \
    battle_desktop/team_parser.c \
    battle_desktop/stubs/stubs.c \
    battle_desktop/stubs/data_stubs.c \
    battle_desktop/stubs/text_data.c \
    battle_desktop/generated/battle_scripts.c \
    battle_desktop/generated/name_tables.c \
    battle_desktop/generated/item_hold_effects.c

# Additional sources for the shared library build (ML API)
LIB_SRCS := \
    battle_desktop/battle_api.c \
    battle_desktop/programmatic_controller.c \
    battle_desktop/team_randomiser.c \
    battle_desktop/console_controller.c \
    battle_desktop/data_transfer.c \
    battle_desktop/team_parser.c \
    battle_desktop/stubs/stubs.c \
    battle_desktop/stubs/data_stubs.c \
    battle_desktop/stubs/text_data.c \
    battle_desktop/generated/battle_scripts.c \
    battle_desktop/generated/name_tables.c \
    battle_desktop/generated/item_hold_effects.c

# -----------------------------------------------------------------------
# Core battle engine source files from src/
# -----------------------------------------------------------------------
BATTLE_SRCS := \
    src/battle_main.c \
    src/battle_script_commands.c \
    src/battle_util.c \
    src/battle_util2.c \
    src/battle_ai_script_commands.c \
    src/battle_ai_switch_items.c \
    src/battle_controllers.c \
    src/battle_message.c

# -----------------------------------------------------------------------
# Pokémon data and utility files
# -----------------------------------------------------------------------
UTIL_SRCS := \
    src/pokemon.c \
    src/task.c \
    src/random.c \
    src/string_util.c \
    src/international_string_util.c \
    src/strings.c \
    src/util.c

ALL_SRCS := $(DESKTOP_SRCS) $(BATTLE_SRCS) $(UTIL_SRCS)

# Generate object file names
OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(ALL_SRCS))

# Library object files (in a separate directory to avoid CFLAGS conflicts)
LIB_OBJDIR := $(OBJDIR)/lib
LIB_ALL_SRCS := $(LIB_SRCS) $(BATTLE_SRCS) $(UTIL_SRCS)
LIB_OBJS := $(patsubst %.c,$(LIB_OBJDIR)/%.o,$(LIB_ALL_SRCS))

# -----------------------------------------------------------------------
# Build rules
# -----------------------------------------------------------------------

.PHONY: all clean run lib

all: $(TARGET)

lib: $(LIB_TARGET)

$(TARGET): $(OBJS)
	@echo "Linking $@..."
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	@echo "Build complete: $@"

$(LIB_TARGET): $(LIB_OBJS)
	@echo "Linking shared library $@..."
	$(CC) -shared $(LIB_OBJS) $(LDFLAGS) -o $@
	@echo "Library build complete: $@"

# Files that contain _("...{B_...}...") placeholders must go through preproc
# to convert GF charmap + placeholder tokens into the 0xFD byte sequences that
# BattleStringExpandPlaceholders expects at runtime.
#
# We write preproc output to an intermediate .i file instead of piping it
# directly to gcc.  This ensures that preproc failures are always caught:
# on cmd.exe, a pipe only reports the last command's exit code (gcc), so a
# silent preproc failure would produce an empty .o and corrupt the build.
# With separate recipe lines, make checks each exit code independently.
# -----------------------------------------------------------------------
# preproc tool — built from source with static linking so it runs on
# native Windows cmd.exe without requiring MSYS2 DLLs in PATH.
# -----------------------------------------------------------------------
CHARMAP      := charmap.txt
PREPROC_EXE  := tools/preproc/preproc_native.exe
PREPROC_SRCS := tools/preproc/asm_file.cpp tools/preproc/c_file.cpp \
                tools/preproc/charmap.cpp  tools/preproc/preproc.cpp \
                tools/preproc/string_parser.cpp tools/preproc/utf8.cpp \
                tools/preproc/io.cpp
PREPROC_HDRS := tools/preproc/asm_file.h tools/preproc/c_file.h \
                tools/preproc/char_util.h tools/preproc/charmap.h \
                tools/preproc/preproc.h   tools/preproc/string_parser.h \
                tools/preproc/utf8.h      tools/preproc/io.h

$(PREPROC_EXE): $(PREPROC_SRCS) $(PREPROC_HDRS)
	@echo "Building preproc..."
	g++ -std=c++11 -O2 -Wall -Wno-switch \
	    -static -static-libgcc -static-libstdc++ \
	    $(PREPROC_SRCS) -o $@

# On native Windows cmd.exe, forward slashes in an executable path are parsed
# as flags; use backslashes for the actual invocation.
ifeq ($(OS),Windows_NT)
  ifndef MSYSTEM
    PREPROC := tools\preproc\preproc_native.exe
  else
    PREPROC := $(PREPROC_EXE)
  endif
else
  PREPROC := $(PREPROC_EXE)
endif

$(OBJDIR)/src/battle_message.o: src/battle_message.c $(PREPROC_EXE)
	@$(call MKDIR,$(dir $@))
	@echo "Preproc+compiling $<..."
	$(PREPROC) $< $(CHARMAP) > $(@:.o=.i)
	$(CC) $(CFLAGS) -x c $(@:.o=.i) -c -o $@

$(OBJDIR)/src/strings.o: src/strings.c $(PREPROC_EXE)
	@$(call MKDIR,$(dir $@))
	@echo "Preproc+compiling $<..."
	$(PREPROC) $< $(CHARMAP) > $(@:.o=.i)
	$(CC) $(CFLAGS) -x c $(@:.o=.i) -c -o $@

# Compile each .c file into the build directory
$(OBJDIR)/%.o: %.c
	@$(call MKDIR,$(dir $@))
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# -----------------------------------------------------------------------
# Library build rules (separate object directory, with DLL export flag)
# -----------------------------------------------------------------------
LIB_CFLAGS := $(CFLAGS) -DBATTLE_API_BUILD

$(LIB_OBJDIR)/src/battle_message.o: src/battle_message.c $(PREPROC_EXE)
	@$(call MKDIR,$(dir $@))
	@echo "[lib] Preproc+compiling $<..."
	$(PREPROC) $< $(CHARMAP) > $(@:.o=.i)
	$(CC) $(LIB_CFLAGS) -x c $(@:.o=.i) -c -o $@

$(LIB_OBJDIR)/src/strings.o: src/strings.c $(PREPROC_EXE)
	@$(call MKDIR,$(dir $@))
	@echo "[lib] Preproc+compiling $<..."
	$(PREPROC) $< $(CHARMAP) > $(@:.o=.i)
	$(CC) $(LIB_CFLAGS) -x c $(@:.o=.i) -c -o $@

$(LIB_OBJDIR)/%.o: %.c
	@$(call MKDIR,$(dir $@))
	@echo "[lib] Compiling $<..."
	$(CC) $(LIB_CFLAGS) -c $< -o $@

clean:
	$(call RM_DIR,$(OBJDIR))
	$(call RM_FILE,$(TARGET))
	$(call RM_FILE,$(LIB_TARGET))
	$(call RM_FILE,$(PREPROC_EXE))

run: all
	$(TARGET)
