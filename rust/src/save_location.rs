#![cfg_attr(all(not(test), target_arch = "arm"), no_std)]

type I8 = i8;
type I16 = i16;
type U8 = u8;
type U16 = u16;
type U32 = u32;

const LIST_END: U16 = 0xFFFF;

const POKECENTER_SAVEWARP: U8 = 1 << 1;
const LOBBY_SAVEWARP: U8 = 1 << 2;
const UNK_SPECIAL_SAVE_WARP_FLAG_3: U8 = 1 << 3;
const CHAMPION_SAVEWARP: U8 = 1 << 7;

const UNLOCKED_POKEDEX_FLAGS: U32 =
    (1 << 15) | (1 << 0) | (1 << 1) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 3);

const MAP_BATTLE_FRONTIER_BATTLE_TOWER_LOBBY: U16 = 0x1A05;

#[repr(C)]
pub struct WarpData {
    pub map_group: I8,
    pub map_num: I8,
    warp_id: I8,
    padding: U8,
    x: I16,
    y: I16,
}

#[repr(C)]
pub struct SaveBlock1 {
    filler_00: [U8; 0x04],
    pub location: WarpData,
}

#[repr(C)]
pub struct SaveBlock2 {
    filler_00: [U8; 0x09],
    pub special_save_warp_flags: U8,
    filler_0a: [U8; 0x9e],
    pub gcn_link_flags: U32,
}

#[cfg(not(test))]
extern "C" {
    static mut gSaveBlock1Ptr: *mut SaveBlock1;
    static mut gSaveBlock2Ptr: *mut SaveBlock2;
}

#[cfg(test)]
#[no_mangle]
pub static mut gSaveBlock1Ptr: *mut SaveBlock1 = core::ptr::null_mut();

#[cfg(test)]
extern "C" {
    static mut gSaveBlock2Ptr: *mut SaveBlock2;
}

#[link_section = ".rodata"]
static S_SAVE_LOCATION_POKE_CENTER_LIST: [U16; 39] = [
    0x0202, 0x0203, 0x0301, 0x0302, 0x0405, 0x0406, 0x0504, 0x0505, 0x0604, 0x0605,
    0x0700, 0x0701, 0x0804, 0x0805, 0x090B, 0x090C, 0x0A05, 0x0A06, 0x0B05, 0x0B06,
    0x0C02, 0x0C03, 0x0D06, 0x0D07, 0x0E03, 0x0E04, 0x0F02, 0x0F03, 0x100C, 0x100D,
    0x100A, 0x100E, 0x1A35, 0x1A36, 0x1918, 0x1919, 0x191A, 0x191B, LIST_END,
];

#[link_section = ".rodata"]
static S_SAVE_LOCATION_RELOAD_LOC_LIST: [U16; 2] = [
    MAP_BATTLE_FRONTIER_BATTLE_TOWER_LOBBY,
    LIST_END,
];

#[link_section = ".rodata"]
static S_EMPTY_MAP_LIST: [U16; 1] = [LIST_END];

#[inline]
unsafe fn save_block_1() -> *mut SaveBlock1 {
    unsafe { gSaveBlock1Ptr }
}

#[inline]
unsafe fn save_block_2() -> *mut SaveBlock2 {
    unsafe { gSaveBlock2Ptr }
}

fn is_cur_map_in_location_list(list: &[U16]) -> bool {
    unsafe {
        let location = &(*save_block_1()).location;
        let map = ((location.map_group as U8 as U16) << 8) + location.map_num as U8 as U16;

        for &entry in list {
            if entry == LIST_END {
                break;
            }
            if entry == map {
                return true;
            }
        }
    }

    false
}

fn is_cur_map_poke_center() -> bool {
    is_cur_map_in_location_list(&S_SAVE_LOCATION_POKE_CENTER_LIST)
}

fn is_cur_map_reload_location() -> bool {
    is_cur_map_in_location_list(&S_SAVE_LOCATION_RELOAD_LOC_LIST)
}

fn is_cur_map_in_empty_list() -> bool {
    is_cur_map_in_location_list(&S_EMPTY_MAP_LIST)
}

fn try_set_poke_center_warp_status() {
    unsafe {
        if is_cur_map_poke_center() {
            (*save_block_2()).special_save_warp_flags |= POKECENTER_SAVEWARP;
        } else {
            (*save_block_2()).special_save_warp_flags &= !POKECENTER_SAVEWARP;
        }
    }
}

fn try_set_reload_warp_status() {
    unsafe {
        if is_cur_map_reload_location() {
            (*save_block_2()).special_save_warp_flags |= LOBBY_SAVEWARP;
        } else {
            (*save_block_2()).special_save_warp_flags &= !LOBBY_SAVEWARP;
        }
    }
}

fn try_set_unknown_warp_status() {
    unsafe {
        if is_cur_map_in_empty_list() {
            (*save_block_2()).special_save_warp_flags |= UNK_SPECIAL_SAVE_WARP_FLAG_3;
        } else {
            (*save_block_2()).special_save_warp_flags &= !UNK_SPECIAL_SAVE_WARP_FLAG_3;
        }
    }
}

#[no_mangle]
pub extern "C" fn TrySetMapSaveWarpStatus() {
    try_set_poke_center_warp_status();
    try_set_reload_warp_status();
    try_set_unknown_warp_status();
}

#[no_mangle]
pub extern "C" fn SetUnlockedPokedexFlags() {
    unsafe {
        (*save_block_2()).gcn_link_flags |= UNLOCKED_POKEDEX_FLAGS;
    }
}

#[no_mangle]
pub extern "C" fn SetChampionSaveWarp() {
    unsafe {
        (*save_block_2()).special_save_warp_flags |= CHAMPION_SAVEWARP;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::mem::{align_of, offset_of, size_of};
    use std::sync::Mutex;

    static TEST_LOCK: Mutex<()> = Mutex::new(());

    fn blank_warp() -> WarpData {
        WarpData {
            map_group: 0,
            map_num: 0,
            warp_id: 0,
            padding: 0,
            x: 0,
            y: 0,
        }
    }

    fn blank_save_block_1() -> SaveBlock1 {
        SaveBlock1 {
            filler_00: [0; 0x04],
            location: blank_warp(),
        }
    }

    fn blank_save_block_2() -> SaveBlock2 {
        SaveBlock2 {
            filler_00: [0; 0x09],
            special_save_warp_flags: 0,
            filler_0a: [0; 0x9e],
            gcn_link_flags: 0,
        }
    }

    unsafe fn use_save_blocks(save1: &mut SaveBlock1, save2: &mut SaveBlock2) {
        unsafe {
            gSaveBlock1Ptr = save1 as *mut SaveBlock1;
            gSaveBlock2Ptr = save2 as *mut SaveBlock2;
        }
    }

    fn set_map(save: &mut SaveBlock1, map: U16) {
        save.location.map_group = (map >> 8) as U8 as I8;
        save.location.map_num = (map & 0xFF) as U8 as I8;
    }

    #[test]
    fn save_block_layout_matches_c_offsets() {
        assert_eq!(size_of::<WarpData>(), 8);
        assert_eq!(align_of::<WarpData>(), 2);
        assert_eq!(offset_of!(SaveBlock1, location), 0x04);
        assert_eq!(offset_of!(SaveBlock2, special_save_warp_flags), 0x09);
        assert_eq!(offset_of!(SaveBlock2, gcn_link_flags), 0xA8);
    }

    #[test]
    fn every_poke_center_map_sets_poke_center_flag() {
        let _guard = TEST_LOCK.lock().unwrap();
        let poke_center_maps =
            &S_SAVE_LOCATION_POKE_CENTER_LIST[..S_SAVE_LOCATION_POKE_CENTER_LIST.len() - 1];

        for &map in poke_center_maps {
            let mut save1 = blank_save_block_1();
            let mut save2 = blank_save_block_2();
            save2.special_save_warp_flags = 0xF0;
            set_map(&mut save1, map);

            unsafe { use_save_blocks(&mut save1, &mut save2) };
            TrySetMapSaveWarpStatus();

            assert_eq!(
                save2.special_save_warp_flags & POKECENTER_SAVEWARP,
                POKECENTER_SAVEWARP
            );
            assert_eq!(save2.special_save_warp_flags & LOBBY_SAVEWARP, 0);
            assert_eq!(save2.special_save_warp_flags & UNK_SPECIAL_SAVE_WARP_FLAG_3, 0);
            assert_eq!(save2.special_save_warp_flags & 0xF0, 0xF0);
        }
    }

    #[test]
    fn non_listed_map_clears_managed_warp_flags_and_preserves_others() {
        let _guard = TEST_LOCK.lock().unwrap();
        let mut save1 = blank_save_block_1();
        let mut save2 = blank_save_block_2();
        save2.special_save_warp_flags = 0xFF;
        set_map(&mut save1, 0x0101);

        unsafe { use_save_blocks(&mut save1, &mut save2) };
        TrySetMapSaveWarpStatus();

        assert_eq!(save2.special_save_warp_flags & POKECENTER_SAVEWARP, 0);
        assert_eq!(save2.special_save_warp_flags & LOBBY_SAVEWARP, 0);
        assert_eq!(save2.special_save_warp_flags & UNK_SPECIAL_SAVE_WARP_FLAG_3, 0);
        assert_eq!(save2.special_save_warp_flags & 0xF1, 0xF1);
    }

    #[test]
    fn battle_tower_lobby_sets_lobby_flag() {
        let _guard = TEST_LOCK.lock().unwrap();
        let mut save1 = blank_save_block_1();
        let mut save2 = blank_save_block_2();
        set_map(&mut save1, MAP_BATTLE_FRONTIER_BATTLE_TOWER_LOBBY);

        unsafe { use_save_blocks(&mut save1, &mut save2) };
        TrySetMapSaveWarpStatus();

        assert_eq!(save2.special_save_warp_flags & POKECENTER_SAVEWARP, 0);
        assert_eq!(save2.special_save_warp_flags & LOBBY_SAVEWARP, LOBBY_SAVEWARP);
        assert_eq!(save2.special_save_warp_flags & UNK_SPECIAL_SAVE_WARP_FLAG_3, 0);
    }

    #[test]
    fn set_unlocked_pokedex_flags_ors_expected_bits() {
        let _guard = TEST_LOCK.lock().unwrap();
        let mut save1 = blank_save_block_1();
        let mut save2 = blank_save_block_2();
        save2.gcn_link_flags = 0x4000_0000;

        unsafe { use_save_blocks(&mut save1, &mut save2) };
        SetUnlockedPokedexFlags();

        assert_eq!(save2.gcn_link_flags, 0x4000_803F);
    }

    #[test]
    fn set_champion_save_warp_ors_champion_bit() {
        let _guard = TEST_LOCK.lock().unwrap();
        let mut save1 = blank_save_block_1();
        let mut save2 = blank_save_block_2();
        save2.special_save_warp_flags = 0x05;

        unsafe { use_save_blocks(&mut save1, &mut save2) };
        SetChampionSaveWarp();

        assert_eq!(save2.special_save_warp_flags, 0x85);
    }
}
