# Rust porting progress

Progress is measured by C objects replaced with Rust objects while the ROM remains buildable.

| C source | Rust source | Status | Tests | Validation | Risks/notes |
| --- | --- | --- | --- | --- | --- |
| src/math_util.c | rust/src/math_util.rs | tested | Rust host unit tests cover all nine exported functions, zero-divisor guarded paths, signed truncation, shift variants, and wide intermediates. | `docker compose run --rm builder sh -lc 'export PATH=/usr/local/cargo/bin:$PATH; cd rust && cargo test'`; `docker compose run --rm builder`; `arm-none-eabi-nm` confirmed MathUtil exports in `build/emerald/src/math_util.o` and `pokeemerald.elf`. | Pure arithmetic port; inverse functions intentionally preserve C divide-by-zero precondition. Rust ROM code calls agbcc/libgcc helper symbols for signed division/multiplication to avoid unsupported `__aeabi_*` helpers and panic paths. |
