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

CC      := gcc
CFLAGS  := -std=c11 -DMODERN=1 -DENGLISH -DNDEBUG \
            -Ibattle_desktop/include \
            -Iinclude \
            -I. \
            -Wall -Wno-unused-function -Wno-unused-variable \
            -Wno-missing-braces \
            -O2 -g
LDFLAGS := -lm

# Output
TARGET  := battle_desktop/battle_desktop

# Build directory
OBJDIR  := battle_desktop/build

# -----------------------------------------------------------------------
# Desktop-specific source files
# -----------------------------------------------------------------------
DESKTOP_SRCS := \
    battle_desktop/main.c \
    battle_desktop/console_controller.c \
    battle_desktop/data_transfer.c \
    battle_desktop/stubs/stubs.c \
    battle_desktop/stubs/data_stubs.c \
    battle_desktop/generated/battle_scripts.c

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

# -----------------------------------------------------------------------
# Build rules
# -----------------------------------------------------------------------

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "Linking $@..."
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	@echo "Build complete: $@"

# Compile each .c file into the build directory
$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)

run: all
	$(TARGET)
