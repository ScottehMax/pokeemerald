# Rust porting pipeline for future agents

This repository is a C/ASM decompilation of Pokemon Emerald. The migration goal is not a rewrite in one pass. The goal is to replace one C object at a time with a Rust object while keeping the ROM linkable after every accepted change.

## Build model

The normal build collects C sources from `src/**/*.c`, assembles them into objects under `build/<variant>/src`, then links those objects with existing ASM/data objects.

`rust_port.mk` adds an optional replacement layer:

1. Add a C file to `PORTED_C_SRCS` in `rust_port.mk`.
2. Add a Rust file at the matching path under `rust/src`.
3. The original C file is removed from `C_SRCS`.
4. The Rust file is compiled to an object under `build/<variant>/rust`.
5. The final ELF/ROM links C, ASM, data, and Rust objects together.

Example mapping:

```text
src/decompress.c       -> rust/src/decompress.rs
src/foo/bar.c          -> rust/src/foo/bar.rs
```

The Rust object must export the same ABI-visible symbols that the removed C object provided.

## Required Makefile hook

The top-level `Makefile` must include `rust_port.mk` after the initial C object list is computed and before final `OBJS_REL`, `SUBDIRS`, dependency includes, and link rules are used.

Expected location:

```make
C_OBJS := $(patsubst $(C_SUBDIR)/%.c,$(C_BUILDDIR)/%.o,$(C_SRCS))

-include rust_port.mk
```

This makes the Rust pipeline inert until `PORTED_C_SRCS` contains at least one source.

## Agent work-unit contract

Each future agent gets exactly one work unit unless explicitly assigned more. A work unit is usually one C file, but can be smaller if the file is large or tightly coupled to global state.

For each work unit, the agent must:

1. Identify all public symbols defined by the C object.
2. Identify all external globals, functions, and constants used by the C object.
3. Create or update the matching Rust source under `rust/src`.
4. Export replacement functions and globals with `#[no_mangle] extern "C"` and C-compatible types.
5. Add the C source path to `PORTED_C_SRCS` only when the Rust object is ready to link.
6. Add tests that compare behavior against the C implementation or a captured fixture.
7. Update the progress ledger in `docs/rust-porting-progress.md`.

## Porting standards

Use Rust as a C-compatible systems language first. Avoid broad redesigns until the entire dependency island is in Rust.

Required standards:

1. `#![no_std]` for ROM object files.
2. `#[repr(C)]` on every struct, enum wrapper, and union equivalent crossing the C ABI.
3. Fixed-width integer aliases matching the decomp conventions: `u8`, `s8`, `u16`, `s16`, `u32`, `s32`.
4. Explicit volatile access for MMIO and shared hardware registers.
5. No heap allocation in ROM code unless the original code used an equivalent allocator.
6. No panics across FFI. ROM objects are compiled with `panic=abort`.
7. No Rust references for aliased C memory. Use raw pointers at FFI boundaries and convert to safe helpers only inside tightly proven scopes.
8. Preserve symbol names, calling convention, global mutability, data layout, integer overflow behavior, and signedness behavior.

## Test strategy

Every port must have tests. Prefer fast host tests, then add integration/ROM-level tests only when host tests cannot prove behavior.

Test layers:

1. Host unit tests in the Rust crate for pure logic and data conversion.
2. C/Rust equivalence harnesses for functions whose behavior can be invoked on host with fixtures.
3. Golden-output fixture tests for table transforms, compression, decompression, string conversion, RNG-like behavior, and state machines.
4. ROM build checks after linking the Rust object.
5. ROM equivalence checks when feasible: symbol presence, map diff, and checksum/fixture output for deterministic subsystems.

Minimum accepted tests for a work unit:

1. At least one direct test for every exported function, unless the export is only a data symbol.
2. Boundary-case tests for integer widths, null/empty inputs, table ends, and overflow-prone arithmetic.
3. Fixture tests for behavior depending on static data.
4. A note in the progress ledger for any behavior that cannot be host-tested yet.

## Recommended agent workflow

1. Read the assigned C file and its header once.
2. Inspect only the directly referenced types/constants needed to compile the port.
3. List the C object API: exported functions, exported data, external dependencies.
4. Write tests before or alongside Rust code using fixtures small enough to review.
5. Implement Rust with C ABI exports and explicit raw-pointer boundaries.
6. Add the source path to `PORTED_C_SRCS`.
7. Run targeted tests and a ROM build after every code or build-system change. Prefer `docker compose run --rm builder` so C/Rust coexistence is checked in the pinned toolchain container.
8. Update `docs/rust-porting-progress.md` with status, tests, and known risks.

## Choosing good first ports

Start with leaf modules that are mostly pure logic and have small C-visible APIs. Avoid battle core, task scheduler, save I/O, graphics hardware paths, and link/RFU until the Rust ABI patterns are proven.

Good candidates usually have these traits:

1. Few external mutable globals.
2. Inputs and outputs are arrays, scalars, or simple structs.
3. Deterministic behavior with fixtureable results.
4. No inline assembly and minimal direct hardware access.
5. Limited dependency on generated asset data.

Poor first candidates:

1. Files with interrupt handlers or timing-sensitive hardware access.
2. Files that define large global state shared across many systems.
3. Files where behavior is mostly side effects through callbacks or scripts.
4. Files that rely on compiler-specific layout quirks that are not yet documented.

## Definition of done for a work unit

A Rust port work unit is done when:

1. The mixed C/Rust build links with the original C object removed.
2. The Rust object exports the required symbols and does not introduce duplicate symbols.
3. Unit/integration tests cover the exported behavior and important edge cases.
4. Any unsafe block has a local safety comment explaining the required invariants.
5. The progress ledger records the C file, Rust file, tests, validation performed, and remaining risks.
6. `docker compose run --rm builder` succeeds after the change.

## Progress metric

Track progress by C object replacement count and estimated source line replacement.

Recommended ledger fields:

```text
C source | Rust source | Status | Tests | Validation | Risks/notes
```

Statuses:

```text
unassigned | in-progress | links | tested | blocked
```

## Context-reset handoff notes

These notes are required reading for the next agent after a context reset.

Current validated baseline:

1. The active branch is intended to support a piecemeal Rust port while retaining the original C/ASM build.
2. `rust_port.mk` is inert while `PORTED_C_SRCS` is empty.
3. `docker compose run --rm builder` has successfully built the current all-C ROM and generated `pokeemerald.gba`.
4. Host builds may fail if `arm-none-eabi-as` is not installed locally. The Docker build is the canonical validation path.

Required validation after every change:

1. Run the narrowest relevant tests for the changed Rust or harness code.
2. Run `docker compose run --rm builder` after every code, build-system, Docker, or port-ledger change.
3. Record validation in `docs/rust-porting-progress.md` for port work units.
4. If validation fails, fix the failure before moving to another work unit unless the user explicitly accepts the broken state.

Required commit discipline:

1. Make a commit after each completed change set.
2. Do not batch unrelated porting work into one commit.
3. Recommended commit message format: `rust-port: <short description>`.
4. Commit only the files relevant to the completed change set.
5. If the working tree already contains unrelated user changes, leave them unstaged and ask before touching them.
6. Never rewrite history or amend a commit unless explicitly requested.

Docker build details:

1. The Docker setup is adapted from the `overworld-battle` branch.
2. The image builds pinned `agbcc` from commit `da598c1d918402c42c0c0d7128ba14567f3175e9`.
3. The image installs `binutils-arm-none-eabi`, which provides the assembler/linker missing on some hosts.
4. The image is Rust-capable, but `thumbv4t-none-eabi` has no prebuilt standard/core artifacts from rustup.
5. `rust-src` is installed so future Rust object compilation can use a build-std flow when needed.

Important Rust target caveat:

`rustup target add thumbv4t-none-eabi` fails because Rust does not ship prebuilt artifacts for that target. Do not reintroduce that command. Future Rust ROM-object work must either:

1. Compile no-core/no-std object code that does not require prebuilt target libraries.
2. Use nightly `-Z build-std=core` or an equivalent custom target/core build strategy.
3. Document the exact command in this file and validate it in Docker before assigning broad porting work.

Build-system seam summary:

1. `Makefile` computes `C_SRCS` and `C_OBJS` as usual.
2. `Makefile` includes `rust_port.mk` immediately after `C_OBJS` is computed.
3. `rust_port.mk` removes paths listed in `PORTED_C_SRCS` from `C_SRCS`.
4. `rust_port.mk` maps each removed `src/<path>.c` to `rust/src/<path>.rs` and produces `build/<variant>/rust/<path>.o`.
5. Final `OBJS` includes the remaining C objects plus any Rust objects.
6. The original C file should remain in `src/` until the Rust replacement has proven equivalent; it is excluded from the build only by `PORTED_C_SRCS`.

Recommended first real port:

Choose a small deterministic leaf module. Avoid hardware, interrupts, link/RFU, battle core, save I/O, task scheduler, and graphics paths until the Rust object command is proven with a trivial symbol replacement.

Before porting a real game module, create a tiny proof-of-link work unit if one does not already exist:

1. Pick or add a trivial exported function with no external dependencies.
2. Replace only that function/object with Rust.
3. Confirm the Rust object links into the ROM through `docker compose run --rm builder`.
4. Remove or keep the proof only if it represents a legitimate port target; do not leave throwaway game-code changes in the tree.

Agent behavior constraints:

1. Prefer small, reviewable slices.
2. Preserve ABI and behavior over idiomatic Rust design.
3. Treat unsafe Rust as expected at FFI boundaries, but require local safety comments.
4. Do not change generated assets or unrelated C behavior while porting.
5. Do not run cleanup commands that remove user work.
6. If build output changes unexpectedly, stop and explain what changed before proceeding.
