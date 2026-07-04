#![cfg_attr(all(not(test), target_arch = "arm"), no_std)]

type U8 = u8;
type U16 = u16;
type U32 = u32;

const ISO_RANDOM_MULTIPLIER: U32 = 1_103_515_245;
const ISO_RANDOM_INCREMENT: U32 = 24_691;

#[repr(C)]
struct RandomEwram {
    unknown: U8,
    _padding: [U8; 3],
    rand_count: U32,
}

#[link_section = "ewram_data"]
static mut S_RANDOM_EWRAM: RandomEwram = RandomEwram {
    unknown: 0,
    _padding: [0; 3],
    rand_count: 0,
};

#[no_mangle]
#[link_section = "common_data"]
pub static mut gRngValue: U32 = 0;

#[no_mangle]
#[link_section = "common_data"]
pub static mut gRng2Value: U32 = 0;

#[inline]
pub fn iso_randomize1(value: U32) -> U32 {
    value
        .wrapping_mul(ISO_RANDOM_MULTIPLIER)
        .wrapping_add(ISO_RANDOM_INCREMENT)
}

#[no_mangle]
pub extern "C" fn Random() -> U16 {
    // SAFETY: These globals mirror the original single-threaded GBA mutable
    // state. Callers use the C ABI and expect direct global mutation.
    unsafe {
        gRngValue = iso_randomize1(gRngValue);
        S_RANDOM_EWRAM.rand_count = S_RANDOM_EWRAM.rand_count.wrapping_add(1);
        (gRngValue >> 16) as U16
    }
}

#[no_mangle]
pub extern "C" fn SeedRng(seed: U16) {
    // SAFETY: Matches the original C global writes. The seed is intentionally
    // widened without preserving previous high bits.
    unsafe {
        gRngValue = seed as U32;
        core::ptr::write_volatile(core::ptr::addr_of_mut!(S_RANDOM_EWRAM.unknown), 0);
    }
}

#[no_mangle]
pub extern "C" fn SeedRng2(seed: U16) {
    // SAFETY: Matches the original C global write for the second RNG stream.
    unsafe {
        gRng2Value = seed as U32;
    }
}

#[no_mangle]
pub extern "C" fn Random2() -> U16 {
    // SAFETY: This mutable global mirrors the original single-threaded GBA RNG
    // state and is exported for C code.
    unsafe {
        gRng2Value = iso_randomize1(gRng2Value);
        (gRng2Value >> 16) as U16
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::Mutex;

    static TEST_LOCK: Mutex<()> = Mutex::new(());

    #[test]
    fn iso_randomize_matches_c_macro_with_wrapping() {
        assert_eq!(iso_randomize1(0), 24_691);
        assert_eq!(iso_randomize1(1), 1_103_539_936);
        assert_eq!(iso_randomize1(0xffff_ffff), 3_191_476_742);
    }

    #[test]
    fn seed_rng_truncates_to_u16_and_random_updates_primary_stream() {
        let _guard = TEST_LOCK.lock().unwrap();

        SeedRng(0x1234);

        // SAFETY: The test lock serializes access to the exported mutable RNG.
        unsafe {
            let rng_value = gRngValue;
            assert_eq!(rng_value, 0x1234);
        }

        assert_eq!(Random(), 0x4dcb);

        // SAFETY: The test lock serializes access to the exported mutable RNG.
        unsafe {
            let rng_value = gRngValue;
            assert_eq!(rng_value, 0x4dcb_f897);
        }
    }

    #[test]
    fn seed_rng2_and_random2_update_secondary_stream() {
        let _guard = TEST_LOCK.lock().unwrap();

        SeedRng2(0xbeef);

        // SAFETY: The test lock serializes access to the exported mutable RNG.
        unsafe {
            let rng2_value = gRng2Value;
            assert_eq!(rng2_value, 0xbeef);
        }

        assert_eq!(Random2(), 0x9658);

        // SAFETY: The test lock serializes access to the exported mutable RNG.
        unsafe {
            let rng2_value = gRng2Value;
            assert_eq!(rng2_value, 0x9658_7e36);
        }
    }

    #[test]
    fn random_streams_are_independent() {
        let _guard = TEST_LOCK.lock().unwrap();

        SeedRng(7);
        SeedRng2(9);

        assert_eq!(Random(), 0xcc6c);

        // SAFETY: The test lock serializes access to the exported mutable RNGs.
        unsafe {
            let rng2_value = gRng2Value;
            assert_eq!(rng2_value, 9);
        }

        assert_eq!(Random2(), 0x4ff9);

        // SAFETY: The test lock serializes access to the exported mutable RNGs.
        unsafe {
            let rng_value = gRngValue;
            assert_eq!(rng_value, 0xcc6c_856e);
        }
    }
}
