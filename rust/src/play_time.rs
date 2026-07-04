#![cfg_attr(all(not(test), target_arch = "arm"), no_std)]

type U8 = u8;
type U16 = u16;

const STOPPED: U8 = 0;
const RUNNING: U8 = 1;
const MAXED_OUT: U8 = 2;

const MAX_HOURS: U16 = 999;
const MAX_MINUTES: U8 = 59;
const MAX_SECONDS: U8 = 59;
const MAX_VBLANKS: U8 = 59;
const VBLANKS_PER_SECOND: U8 = 60;
const SECONDS_PER_MINUTE: U8 = 60;
const MINUTES_PER_HOUR: U8 = 60;

#[repr(C)]
pub struct SaveBlock2 {
    player_name: [U8; 8],
    player_gender: U8,
    special_save_warp_flags: U8,
    player_trainer_id: [U8; 4],
    pub play_time_hours: U16,
    pub play_time_minutes: U8,
    pub play_time_seconds: U8,
    pub play_time_vblanks: U8,
}

static mut S_PLAY_TIME_COUNTER_STATE: U8 = STOPPED;

#[cfg(not(test))]
extern "C" {
    static mut gSaveBlock2Ptr: *mut SaveBlock2;
}

#[cfg(test)]
#[no_mangle]
pub static mut gSaveBlock2Ptr: *mut SaveBlock2 = core::ptr::null_mut();

#[inline]
unsafe fn save_block_2_mut() -> *mut SaveBlock2 {
    // SAFETY: Reading the external mutable pointer mirrors the original C
    // global. Callers must ensure it points to the active SaveBlock2.
    unsafe { gSaveBlock2Ptr }
}

#[no_mangle]
pub extern "C" fn PlayTimeCounter_Reset() {
    // SAFETY: The game initializes gSaveBlock2Ptr before these routines are
    // called. Field writes match the original C SaveBlock2 layout prefix.
    unsafe {
        S_PLAY_TIME_COUNTER_STATE = STOPPED;

        let save = save_block_2_mut();
        (*save).play_time_hours = 0;
        (*save).play_time_minutes = 0;
        (*save).play_time_seconds = 0;
        (*save).play_time_vblanks = 0;
    }
}

#[no_mangle]
pub extern "C" fn PlayTimeCounter_Start() {
    // SAFETY: Accesses the same global counter state and SaveBlock2 pointer as
    // the C implementation under the game's single-threaded execution model.
    unsafe {
        S_PLAY_TIME_COUNTER_STATE = RUNNING;

        if (*save_block_2_mut()).play_time_hours > MAX_HOURS {
            PlayTimeCounter_SetToMax();
        }
    }
}

#[no_mangle]
pub extern "C" fn PlayTimeCounter_Stop() {
    // SAFETY: This is the direct Rust equivalent of assigning the private C
    // static counter state.
    unsafe {
        S_PLAY_TIME_COUNTER_STATE = STOPPED;
    }
}

#[no_mangle]
pub extern "C" fn PlayTimeCounter_Update() {
    // SAFETY: Mutates the active SaveBlock2 play-time fields through the same
    // global pointer used by the original C code.
    unsafe {
        if S_PLAY_TIME_COUNTER_STATE != RUNNING {
            return;
        }

        let save = save_block_2_mut();
        (*save).play_time_vblanks = (*save).play_time_vblanks.wrapping_add(1);

        if (*save).play_time_vblanks < VBLANKS_PER_SECOND {
            return;
        }

        (*save).play_time_vblanks = 0;
        (*save).play_time_seconds = (*save).play_time_seconds.wrapping_add(1);

        if (*save).play_time_seconds < SECONDS_PER_MINUTE {
            return;
        }

        (*save).play_time_seconds = 0;
        (*save).play_time_minutes = (*save).play_time_minutes.wrapping_add(1);

        if (*save).play_time_minutes < MINUTES_PER_HOUR {
            return;
        }

        (*save).play_time_minutes = 0;
        (*save).play_time_hours = (*save).play_time_hours.wrapping_add(1);

        if (*save).play_time_hours > MAX_HOURS {
            PlayTimeCounter_SetToMax();
        }
    }
}

#[no_mangle]
pub extern "C" fn PlayTimeCounter_SetToMax() {
    // SAFETY: The writes target the active SaveBlock2 play-time fields and
    // preserve the C function's saturated max values.
    unsafe {
        S_PLAY_TIME_COUNTER_STATE = MAXED_OUT;

        let save = save_block_2_mut();
        (*save).play_time_hours = MAX_HOURS;
        (*save).play_time_minutes = MAX_MINUTES;
        (*save).play_time_seconds = MAX_SECONDS;
        (*save).play_time_vblanks = MAX_VBLANKS;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::Mutex;

    static TEST_LOCK: Mutex<()> = Mutex::new(());

    fn blank_save_block() -> SaveBlock2 {
        SaveBlock2 {
            player_name: [0; 8],
            player_gender: 0,
            special_save_warp_flags: 0,
            player_trainer_id: [0; 4],
            play_time_hours: 0,
            play_time_minutes: 0,
            play_time_seconds: 0,
            play_time_vblanks: 0,
        }
    }

    unsafe fn use_save_block(save: &mut SaveBlock2) {
        // SAFETY: Tests serialize through TEST_LOCK and keep this stack value
        // alive for the duration of each call sequence.
        unsafe {
            gSaveBlock2Ptr = save as *mut SaveBlock2;
            S_PLAY_TIME_COUNTER_STATE = STOPPED;
        }
    }

    fn snapshot(save: &SaveBlock2) -> (U16, U8, U8, U8) {
        (
            save.play_time_hours,
            save.play_time_minutes,
            save.play_time_seconds,
            save.play_time_vblanks,
        )
    }

    #[test]
    fn reset_stops_counter_and_clears_time_fields() {
        let _guard = TEST_LOCK.lock().unwrap();
        let mut save = blank_save_block();
        save.play_time_hours = 12;
        save.play_time_minutes = 34;
        save.play_time_seconds = 56;
        save.play_time_vblanks = 58;

        // SAFETY: The test-owned save block remains valid while the exported
        // functions use gSaveBlock2Ptr.
        unsafe { use_save_block(&mut save) };
        PlayTimeCounter_Start();
        PlayTimeCounter_Reset();
        PlayTimeCounter_Update();

        assert_eq!(snapshot(&save), (0, 0, 0, 0));
    }

    #[test]
    fn start_clamps_hours_above_max() {
        let _guard = TEST_LOCK.lock().unwrap();
        let mut save = blank_save_block();
        save.play_time_hours = 1000;

        // SAFETY: The test-owned save block remains valid for the call.
        unsafe { use_save_block(&mut save) };
        PlayTimeCounter_Start();

        assert_eq!(snapshot(&save), (999, 59, 59, 59));
    }

    #[test]
    fn stopped_counter_update_is_no_op() {
        let _guard = TEST_LOCK.lock().unwrap();
        let mut save = blank_save_block();
        save.play_time_hours = 1;
        save.play_time_minutes = 2;
        save.play_time_seconds = 3;
        save.play_time_vblanks = 4;

        // SAFETY: The test-owned save block remains valid while stopped update
        // reads the private state.
        unsafe { use_save_block(&mut save) };
        PlayTimeCounter_Start();
        PlayTimeCounter_Stop();
        PlayTimeCounter_Update();

        assert_eq!(snapshot(&save), (1, 2, 3, 4));
    }

    #[test]
    fn vblank_rollover_advances_one_second() {
        let _guard = TEST_LOCK.lock().unwrap();
        let mut save = blank_save_block();
        save.play_time_vblanks = 59;

        // SAFETY: The test-owned save block remains valid while the counter
        // advances.
        unsafe { use_save_block(&mut save) };
        PlayTimeCounter_Start();
        PlayTimeCounter_Update();

        assert_eq!(snapshot(&save), (0, 0, 1, 0));
    }

    #[test]
    fn second_and_minute_rollovers_advance_minute_and_hour() {
        let _guard = TEST_LOCK.lock().unwrap();
        let mut save = blank_save_block();
        save.play_time_hours = 9;
        save.play_time_minutes = 59;
        save.play_time_seconds = 59;
        save.play_time_vblanks = 59;

        // SAFETY: The test-owned save block remains valid while the counter
        // advances across all rollover fields.
        unsafe { use_save_block(&mut save) };
        PlayTimeCounter_Start();
        PlayTimeCounter_Update();

        assert_eq!(snapshot(&save), (10, 0, 0, 0));
    }

    #[test]
    fn minute_rollover_without_hour_increment() {
        let _guard = TEST_LOCK.lock().unwrap();
        let mut save = blank_save_block();
        save.play_time_minutes = 7;
        save.play_time_seconds = 59;
        save.play_time_vblanks = 59;

        // SAFETY: The test-owned save block remains valid while the counter
        // advances across second rollover.
        unsafe { use_save_block(&mut save) };
        PlayTimeCounter_Start();
        PlayTimeCounter_Update();

        assert_eq!(snapshot(&save), (0, 8, 0, 0));
    }

    #[test]
    fn maxed_out_counter_update_is_no_op() {
        let _guard = TEST_LOCK.lock().unwrap();
        let mut save = blank_save_block();

        // SAFETY: The test-owned save block remains valid while max state is
        // set and then checked by Update.
        unsafe { use_save_block(&mut save) };
        PlayTimeCounter_SetToMax();
        PlayTimeCounter_Update();

        assert_eq!(snapshot(&save), (999, 59, 59, 59));
    }
}
