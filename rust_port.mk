# Optional Rust object integration for piecemeal ports.
#
# Usage for a ported C file:
#   1. Add the original C source path to PORTED_C_SRCS below.
#   2. Add a Rust source at rust/src/<same-relative-path>.rs.
#      Example: src/decompress.c -> rust/src/decompress.rs
#      Example: src/foo/bar.c -> rust/src/foo/bar.rs
#   3. Export the same C ABI symbols from Rust with #[no_mangle] extern "C".
#
# The normal linker still links C, asm, data, and Rust .o files together. This
# lets the ROM build with any mixture of original C and ported Rust objects.

RUSTC ?= rustc
RUST_TARGET ?= thumbv4t-none-eabi
RUST_SUBDIR := rust/src
RUST_BUILDDIR := $(OBJ_DIR)/rust

# Future porting tasks append source files here as they are replaced by Rust.
# Keep paths relative to the repository root.
PORTED_C_SRCS :=

RUST_SRCS := $(patsubst src/%.c,$(RUST_SUBDIR)/%.rs,$(PORTED_C_SRCS))
RUST_OBJS := $(patsubst $(RUST_SUBDIR)/%.rs,$(RUST_BUILDDIR)/%.o,$(RUST_SRCS))

C_SRCS := $(filter-out $(PORTED_C_SRCS),$(C_SRCS))
C_OBJS := $(patsubst $(C_SUBDIR)/%.c,$(C_BUILDDIR)/%.o,$(C_SRCS))

OBJS += $(RUST_OBJS)
OBJS_REL := $(patsubst $(OBJ_DIR)/%,%,$(OBJS))
RUST_OBJ_DIRS := $(sort $(dir $(RUST_OBJS)))
SUBDIRS += $(RUST_OBJ_DIRS)
ifneq ($(strip $(RUST_OBJ_DIRS)),)
$(shell mkdir -p $(RUST_OBJ_DIRS))
endif

# Keep this intentionally close to the C target: bare-metal ARMv4T Thumb code,
# no standard library, C ABI exports, and no unwinding.
RUST_PORT_FLAGS ?= \
	--edition=2021 \
	--target $(RUST_TARGET) \
	--crate-type lib \
	--emit=obj \
	-C opt-level=$(O_LEVEL) \
	-C panic=abort \
	-C relocation-model=static

$(RUST_BUILDDIR)/%.o: $(RUST_SUBDIR)/%.rs
	@echo "$(RUSTC) <rust-port-flags> -o $@ $<"
	@mkdir -p $(dir $@)
	$(RUSTC) $(RUST_PORT_FLAGS) -o $@ $<
