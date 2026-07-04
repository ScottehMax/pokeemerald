# Rust port agent task template

Use this template when assigning a future agent a porting slice.

## Assignment

Port this C source to Rust:

```text
src/<file>.c
```

Matching Rust output path:

```text
rust/src/<file>.rs
```

## Constraints

1. Keep the ROM build mixed-language: replace only the assigned C object.
2. Preserve the C ABI exactly for exported symbols.
3. Use `#![no_std]` in ROM object code.
4. Add tests for every exported function or document why a symbol is data-only.
5. Update `PORTED_C_SRCS` in `rust_port.mk` only after the Rust object is intended to replace the C object.
6. Update `docs/rust-porting-progress.md` before finishing.

## Required analysis output

Before editing, identify:

```text
Exported symbols:
External functions used:
External globals used:
Types/constants needed:
Test fixtures needed:
Unsafe/FFI hazards:
```

## Required implementation output

1. Rust replacement source under `rust/src`.
2. Unit/integration tests under the Rust crate or a documented C/Rust equivalence harness.
3. `rust_port.mk` updated with the ported C source path.
4. Progress ledger row updated.

## Completion report

End with:

```text
Ported C source:
Rust source:
Tests added:
Validation run:
Known risks:
Next recommended slice:
```
