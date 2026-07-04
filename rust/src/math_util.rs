type U8 = u8;
type S16 = i16;
type S32 = i32;
type S64 = i64;

#[cfg(target_arch = "arm")]
extern "C" {
    fn __divsi3(a: S32, b: S32) -> S32;
    fn __divdi3(a: S64, b: S64) -> S64;
    fn __muldi3(a: S64, b: S64) -> S64;
}

#[inline]
fn trunc_s32_to_s16(value: S32) -> S16 {
    value as S16
}

#[inline]
fn div_s32(a: S32, b: S32) -> S32 {
    #[cfg(target_arch = "arm")]
    {
        // SAFETY: agbcc/libgcc provides __divsi3 with the same signed division
        // preconditions as the original C code. Callers preserve any C guards.
        unsafe { __divsi3(a, b) }
    }

    #[cfg(not(target_arch = "arm"))]
    {
        a / b
    }
}

#[inline]
fn div_s64(a: S64, b: S64) -> S64 {
    #[cfg(target_arch = "arm")]
    {
        // SAFETY: agbcc/libgcc provides __divdi3; callers preserve the C
        // divide-by-zero preconditions from the original implementation.
        unsafe { __divdi3(a, b) }
    }

    #[cfg(not(target_arch = "arm"))]
    {
        a / b
    }
}

#[inline]
fn mul_s64(a: S64, b: S64) -> S64 {
    #[cfg(target_arch = "arm")]
    {
        // SAFETY: agbcc/libgcc provides __muldi3 for 64-bit signed products.
        unsafe { __muldi3(a, b) }
    }

    #[cfg(not(target_arch = "arm"))]
    {
        a * b
    }
}

#[inline]
pub fn math_util_mul16(x: S16, y: S16) -> S16 {
    trunc_s32_to_s16(div_s32(x as S32 * y as S32, 256))
}

#[inline]
pub fn math_util_mul16_shift(s: U8, x: S16, y: S16) -> S16 {
    trunc_s32_to_s16(div_s32(x as S32 * y as S32, 1_i32.wrapping_shl(s as u32)))
}

#[inline]
pub fn math_util_mul32(x: S32, y: S32) -> S32 {
    div_s64(mul_s64(x as S64, y as S64), 256) as S32
}

#[inline]
pub fn math_util_div16(x: S16, y: S16) -> S16 {
    if y == 0 {
        0
    } else {
        trunc_s32_to_s16(div_s32((x as S32) << 8, y as S32))
    }
}

#[inline]
pub fn math_util_div16_shift(s: U8, x: S16, y: S16) -> S16 {
    if y == 0 {
        0
    } else {
        trunc_s32_to_s16(div_s32((x as S32).wrapping_shl(s as u32), y as S32))
    }
}

#[inline]
pub fn math_util_div32(x: S32, y: S32) -> S32 {
    if y == 0 {
        0
    } else {
        div_s64(x as S64 * 256, y as S64) as S32
    }
}

#[inline]
pub fn math_util_inv16(y: S16) -> S16 {
    trunc_s32_to_s16(div_s32(0x10000, y as S32))
}

#[inline]
pub fn math_util_inv16_shift(s: U8, y: S16) -> S16 {
    trunc_s32_to_s16(div_s32(0x100_i32.wrapping_shl(s as u32), y as S32))
}

#[inline]
pub fn math_util_inv32(y: S32) -> S32 {
    div_s64(0x10000_i64, y as S64) as S32
}

#[no_mangle]
pub extern "C" fn MathUtil_Mul16(x: S16, y: S16) -> S16 {
    math_util_mul16(x, y)
}

#[no_mangle]
pub extern "C" fn MathUtil_Mul16Shift(s: U8, x: S16, y: S16) -> S16 {
    math_util_mul16_shift(s, x, y)
}

#[no_mangle]
pub extern "C" fn MathUtil_Mul32(x: S32, y: S32) -> S32 {
    math_util_mul32(x, y)
}

#[no_mangle]
pub extern "C" fn MathUtil_Div16(x: S16, y: S16) -> S16 {
    math_util_div16(x, y)
}

#[no_mangle]
pub extern "C" fn MathUtil_Div16Shift(s: U8, x: S16, y: S16) -> S16 {
    math_util_div16_shift(s, x, y)
}

#[no_mangle]
pub extern "C" fn MathUtil_Div32(x: S32, y: S32) -> S32 {
    math_util_div32(x, y)
}

#[no_mangle]
pub extern "C" fn MathUtil_Inv16(y: S16) -> S16 {
    math_util_inv16(y)
}

#[no_mangle]
pub extern "C" fn MathUtil_Inv16Shift(s: U8, y: S16) -> S16 {
    math_util_inv16_shift(s, y)
}

#[no_mangle]
pub extern "C" fn MathUtil_Inv32(y: S32) -> S32 {
    math_util_inv32(y)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn mul16_uses_fixed_point_scale_and_truncates() {
        assert_eq!(math_util_mul16(256, 2), 2);
        assert_eq!(math_util_mul16(-257, 3), -3);
        assert_eq!(math_util_mul16(255, 255), 254);
    }

    #[test]
    fn mul16_shift_uses_requested_shift() {
        assert_eq!(math_util_mul16_shift(4, 32, 8), 16);
        assert_eq!(math_util_mul16_shift(3, -17, 5), -10);
    }

    #[test]
    fn mul32_uses_i64_intermediate() {
        assert_eq!(math_util_mul32(65_536, 65_536), 16_777_216);
        assert_eq!(math_util_mul32(-65_536, 65_536), -16_777_216);
    }

    #[test]
    fn div16_returns_zero_for_zero_divisor() {
        assert_eq!(math_util_div16(123, 0), 0);
        assert_eq!(math_util_div16_shift(4, 123, 0), 0);
    }

    #[test]
    fn div16_uses_fixed_point_scale_and_truncates() {
        assert_eq!(math_util_div16(2, 3), 170);
        assert_eq!(math_util_div16(-2, 3), -170);
        assert_eq!(math_util_div16_shift(5, 7, 2), 112);
    }

    #[test]
    fn div32_returns_zero_for_zero_divisor() {
        assert_eq!(math_util_div32(123_456, 0), 0);
    }

    #[test]
    fn div32_uses_i64_intermediate() {
        assert_eq!(math_util_div32(65_536, 2), 8_388_608);
        assert_eq!(math_util_div32(-65_536, 2), -8_388_608);
    }

    #[test]
    fn inverse_helpers_match_c_scaling() {
        assert_eq!(math_util_inv16(2), -32768);
        assert_eq!(math_util_inv16(-3), -21845);
        assert_eq!(math_util_inv16_shift(4, 2), 2048);
        assert_eq!(math_util_inv32(2), 32768);
        assert_eq!(math_util_inv32(-3), -21845);
    }
}
