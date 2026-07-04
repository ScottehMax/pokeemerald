#![cfg_attr(not(test), no_std)]
#![allow(unused_attributes)]

// Shared Rust port support goes here. Individual ROM objects are compiled from
// rust/src/<module>.rs by rust_port.mk so each port can replace one C object at a time.

pub mod math_util;
pub mod blit;
pub mod dynamic_placeholder_text_util;
pub mod heal_location;
pub mod play_time;
pub mod random;

#[cfg(test)]
mod tests {
    #[test]
    fn rust_test_harness_is_available() {
        assert_eq!(2 + 2, 4);
    }
}
