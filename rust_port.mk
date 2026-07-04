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
CARGO ?= cargo
RUST_TARGET ?= thumbv4t-none-eabi
RUST_SUBDIR := rust/src
RUST_BUILDDIR := $(OBJ_DIR)/rust
RUST_CRATE := rust
RUST_CRATE_ROOT := $(RUST_CRATE)/src/lib.rs
RUST_CARGO_MANIFEST := $(RUST_CRATE)/Cargo.toml
RUST_CORE_STAMP := $(RUST_CRATE)/target/$(RUST_TARGET)/release/.rust-core-built

# Future porting tasks append source files here as they are replaced by Rust.
# Keep paths relative to the repository root.
PORTED_C_SRCS := src/math_util.c src/random.c src/heal_location.c src/blit.c src/dynamic_placeholder_text_util.c src/play_time.c

RUST_SRCS := $(patsubst src/%.c,$(RUST_SUBDIR)/%.rs,$(PORTED_C_SRCS))
RUST_OBJS := $(patsubst src/%.c,$(C_BUILDDIR)/%.o,$(PORTED_C_SRCS))

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
# no standard library, C ABI exports, and no unwinding. Cargo builds core from
# rust-src because thumbv4t-none-eabi has no prebuilt core artifact.
RUST_PORT_FLAGS ?= \
	--edition=2021 \
	--target $(RUST_TARGET) \
	--crate-type lib \
	--emit=obj \
	-C opt-level=$(O_LEVEL) \
	-C panic=abort \
	-C relocation-model=static

$(RUST_CORE_STAMP): $(RUST_CRATE_ROOT) $(RUST_SRCS) $(RUST_CARGO_MANIFEST)
	@echo "$(CARGO) build <rust-core-flags>"
	cd $(RUST_CRATE) && PATH=/usr/local/cargo/bin:$$PATH RUSTC_BOOTSTRAP=1 $(CARGO) build -Z build-std=core --target $(RUST_TARGET) --release --lib
	@touch $@

$(RUST_OBJS): $(C_BUILDDIR)/%.o: $(RUST_SUBDIR)/%.rs $(RUST_CORE_STAMP)
	@echo "$(RUSTC) <rust-port-flags> -o $@ $<"
	@mkdir -p $(dir $@)
	CORE_RLIB=$$(ls $(RUST_CRATE)/target/$(RUST_TARGET)/release/deps/libcore-*.rlib | head -n 1); \
	COMPILER_BUILTINS_RLIB=$$(ls $(RUST_CRATE)/target/$(RUST_TARGET)/release/deps/libcompiler_builtins-*.rlib | head -n 1); \
	PATH=/usr/local/cargo/bin:$$PATH $(RUSTC) $(RUST_PORT_FLAGS) -L dependency=$(RUST_CRATE)/target/$(RUST_TARGET)/release/deps --extern core=$$CORE_RLIB --extern compiler_builtins=$$COMPILER_BUILTINS_RLIB -o $@ $<
