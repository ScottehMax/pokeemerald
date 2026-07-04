#![cfg_attr(all(not(test), target_arch = "arm"), no_std)]

type U8 = u8;

const NUM_PLACEHOLDERS: usize = 8;
const CHAR_DYNAMIC: U8 = 0xF7;
const EOS: U8 = 0xFF;

#[link_section = "ewram_data"]
static mut S_STRING_POINTERS: [*const U8; NUM_PLACEHOLDERS] =
    [core::ptr::null(); NUM_PLACEHOLDERS];

#[cfg(not(test))]
extern "C" {
    fn StringCopy(dest: *mut U8, src: *const U8) -> *mut U8;
}

#[cfg(not(test))]
unsafe fn string_copy(dest: *mut U8, src: *const U8) -> *mut U8 {
    // SAFETY: The C routine has the same contract as this module: callers pass
    // valid EOS-terminated source and destination buffers.
    unsafe { StringCopy(dest, src) }
}

#[cfg(test)]
unsafe fn string_copy(mut dest: *mut U8, mut src: *const U8) -> *mut U8 {
    loop {
        let c = unsafe { *src };
        if c == EOS {
            unsafe {
                *dest = EOS;
            }
            return dest;
        }

        unsafe {
            *dest = c;
            dest = dest.add(1);
            src = src.add(1);
        }
    }
}

#[no_mangle]
pub extern "C" fn DynamicPlaceholderTextUtil_Reset() {
    unsafe {
        let table = core::ptr::addr_of_mut!(S_STRING_POINTERS) as *mut *const U8;
        for i in 0..NUM_PLACEHOLDERS {
            *table.add(i) = core::ptr::null();
        }
    }
}

#[no_mangle]
pub extern "C" fn DynamicPlaceholderTextUtil_SetPlaceholderPtr(idx: U8, ptr: *const U8) {
    let idx = idx as usize;
    if idx < NUM_PLACEHOLDERS {
        unsafe {
            let table = core::ptr::addr_of_mut!(S_STRING_POINTERS) as *mut *const U8;
            *table.add(idx) = ptr;
        }
    }
}

#[no_mangle]
pub extern "C" fn DynamicPlaceholderTextUtil_ExpandPlaceholders(
    mut dest: *mut U8,
    mut src: *const U8,
) -> *mut U8 {
    unsafe {
        while *src != EOS {
            if *src != CHAR_DYNAMIC {
                *dest = *src;
                dest = dest.add(1);
                src = src.add(1);
            } else {
                src = src.add(1);

                let table = core::ptr::addr_of!(S_STRING_POINTERS) as *const *const U8;
                let placeholder = *table.add(*src as usize);
                if !placeholder.is_null() {
                    dest = string_copy(dest, placeholder);
                }

                src = src.add(1);
            }
        }

        *dest = EOS;
        dest
    }
}

#[no_mangle]
pub extern "C" fn DynamicPlaceholderTextUtil_GetPlaceholderPtr(idx: U8) -> *const U8 {
    unsafe {
        let table = core::ptr::addr_of!(S_STRING_POINTERS) as *const *const U8;
        *table.add(idx as usize)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::Mutex;

    static TEST_LOCK: Mutex<()> = Mutex::new(());

    fn eos_slice(bytes: &[U8]) -> &[U8] {
        assert_eq!(bytes.last(), Some(&EOS));
        bytes
    }

    #[test]
    fn reset_clears_all_placeholder_pointers() {
        let _guard = TEST_LOCK.lock().unwrap();
        static VALUE: [U8; 2] = [1, EOS];

        for i in 0..NUM_PLACEHOLDERS {
            DynamicPlaceholderTextUtil_SetPlaceholderPtr(i as U8, VALUE.as_ptr());
        }

        DynamicPlaceholderTextUtil_Reset();

        for i in 0..NUM_PLACEHOLDERS {
            assert!(DynamicPlaceholderTextUtil_GetPlaceholderPtr(i as U8).is_null());
        }
    }

    #[test]
    fn set_and_get_respect_set_bounds() {
        let _guard = TEST_LOCK.lock().unwrap();
        static ZERO: [U8; 2] = [0, EOS];
        static SEVEN: [U8; 2] = [7, EOS];
        static OUT_OF_RANGE: [U8; 2] = [9, EOS];

        DynamicPlaceholderTextUtil_Reset();
        DynamicPlaceholderTextUtil_SetPlaceholderPtr(0, ZERO.as_ptr());
        DynamicPlaceholderTextUtil_SetPlaceholderPtr(7, SEVEN.as_ptr());
        DynamicPlaceholderTextUtil_SetPlaceholderPtr(8, OUT_OF_RANGE.as_ptr());

        assert_eq!(DynamicPlaceholderTextUtil_GetPlaceholderPtr(0), ZERO.as_ptr());
        assert_eq!(DynamicPlaceholderTextUtil_GetPlaceholderPtr(7), SEVEN.as_ptr());
    }

    #[test]
    fn expand_copies_literal_text() {
        let _guard = TEST_LOCK.lock().unwrap();
        let src = eos_slice(&[0x12, 0x34, 0x56, EOS]);
        let mut dest = [0; 8];

        DynamicPlaceholderTextUtil_Reset();
        let end =
            DynamicPlaceholderTextUtil_ExpandPlaceholders(dest.as_mut_ptr(), src.as_ptr());

        assert_eq!(&dest[..4], src);
        assert_eq!(end, unsafe { dest.as_mut_ptr().add(3) });
    }

    #[test]
    fn expand_replaces_dynamic_placeholders() {
        let _guard = TEST_LOCK.lock().unwrap();
        static NAME: [U8; 4] = [0xAA, 0xBB, 0xCC, EOS];
        let src = eos_slice(&[0x10, CHAR_DYNAMIC, 2, 0x20, EOS]);
        let mut dest = [0; 8];

        DynamicPlaceholderTextUtil_Reset();
        DynamicPlaceholderTextUtil_SetPlaceholderPtr(2, NAME.as_ptr());
        let end =
            DynamicPlaceholderTextUtil_ExpandPlaceholders(dest.as_mut_ptr(), src.as_ptr());

        assert_eq!(&dest[..6], &[0x10, 0xAA, 0xBB, 0xCC, 0x20, EOS]);
        assert_eq!(end, unsafe { dest.as_mut_ptr().add(5) });
    }

    #[test]
    fn expand_skips_null_placeholders() {
        let _guard = TEST_LOCK.lock().unwrap();
        let src = eos_slice(&[0x10, CHAR_DYNAMIC, 4, 0x20, EOS]);
        let mut dest = [0; 8];

        DynamicPlaceholderTextUtil_Reset();
        let end =
            DynamicPlaceholderTextUtil_ExpandPlaceholders(dest.as_mut_ptr(), src.as_ptr());

        assert_eq!(&dest[..3], &[0x10, 0x20, EOS]);
        assert_eq!(end, unsafe { dest.as_mut_ptr().add(2) });
    }

    #[test]
    fn expand_writes_eos_after_placeholder_content() {
        let _guard = TEST_LOCK.lock().unwrap();
        static VALUE: [U8; 2] = [0x77, EOS];
        let src = eos_slice(&[CHAR_DYNAMIC, 1, EOS]);
        let mut dest = [0xEE; 4];

        DynamicPlaceholderTextUtil_Reset();
        DynamicPlaceholderTextUtil_SetPlaceholderPtr(1, VALUE.as_ptr());
        let end =
            DynamicPlaceholderTextUtil_ExpandPlaceholders(dest.as_mut_ptr(), src.as_ptr());

        assert_eq!(&dest[..3], &[0x77, EOS, 0xEE]);
        assert_eq!(end, unsafe { dest.as_mut_ptr().add(1) });
    }
}
