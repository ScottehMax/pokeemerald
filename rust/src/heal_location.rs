#![cfg_attr(all(not(test), target_arch = "arm"), no_std)]

use core::ptr;

type S8 = i8;
type U16 = u16;
type U32 = u32;

const HEAL_LOCATION_NONE: U32 = 0;

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct HealLocation {
    pub map_group: S8,
    pub map_num: S8,
    pub x: U16,
    pub y: U16,
    _padding: [u8; 2],
}

const fn heal_location(map: U16, x: U16, y: U16) -> HealLocation {
    HealLocation {
        map_group: (map >> 8) as S8,
        map_num: (map & 0xff) as S8,
        x,
        y,
        _padding: [0; 2],
    }
}

const MAP_PETALBURG_CITY: U16 = 0 | (0 << 8);
const MAP_SLATEPORT_CITY: U16 = 1 | (0 << 8);
const MAP_MAUVILLE_CITY: U16 = 2 | (0 << 8);
const MAP_RUSTBORO_CITY: U16 = 3 | (0 << 8);
const MAP_FORTREE_CITY: U16 = 4 | (0 << 8);
const MAP_LILYCOVE_CITY: U16 = 5 | (0 << 8);
const MAP_MOSSDEEP_CITY: U16 = 6 | (0 << 8);
const MAP_SOOTOPOLIS_CITY: U16 = 7 | (0 << 8);
const MAP_EVER_GRANDE_CITY: U16 = 8 | (0 << 8);
const MAP_LITTLEROOT_TOWN: U16 = 9 | (0 << 8);
const MAP_OLDALE_TOWN: U16 = 10 | (0 << 8);
const MAP_DEWFORD_TOWN: U16 = 11 | (0 << 8);
const MAP_LAVARIDGE_TOWN: U16 = 12 | (0 << 8);
const MAP_FALLARBOR_TOWN: U16 = 13 | (0 << 8);
const MAP_VERDANTURF_TOWN: U16 = 14 | (0 << 8);
const MAP_PACIFIDLOG_TOWN: U16 = 15 | (0 << 8);
const MAP_LITTLEROOT_TOWN_BRENDANS_HOUSE_2F: U16 = 1 | (1 << 8);
const MAP_LITTLEROOT_TOWN_MAYS_HOUSE_2F: U16 = 3 | (1 << 8);
const MAP_SOUTHERN_ISLAND_EXTERIOR: U16 = 9 | (26 << 8);
const MAP_BATTLE_FRONTIER_OUTSIDE_EAST: U16 = 14 | (26 << 8);

#[link_section = ".rodata"]
static S_HEAL_LOCATIONS: [HealLocation; 22] = [
    heal_location(MAP_LITTLEROOT_TOWN_BRENDANS_HOUSE_2F, 4, 2),
    heal_location(MAP_LITTLEROOT_TOWN_MAYS_HOUSE_2F, 4, 2),
    heal_location(MAP_PETALBURG_CITY, 20, 17),
    heal_location(MAP_SLATEPORT_CITY, 19, 20),
    heal_location(MAP_MAUVILLE_CITY, 22, 6),
    heal_location(MAP_RUSTBORO_CITY, 16, 39),
    heal_location(MAP_FORTREE_CITY, 5, 7),
    heal_location(MAP_LILYCOVE_CITY, 24, 15),
    heal_location(MAP_MOSSDEEP_CITY, 28, 17),
    heal_location(MAP_SOOTOPOLIS_CITY, 43, 32),
    heal_location(MAP_EVER_GRANDE_CITY, 27, 49),
    heal_location(MAP_LITTLEROOT_TOWN, 5, 9),
    heal_location(MAP_LITTLEROOT_TOWN, 14, 9),
    heal_location(MAP_OLDALE_TOWN, 6, 17),
    heal_location(MAP_DEWFORD_TOWN, 2, 11),
    heal_location(MAP_LAVARIDGE_TOWN, 9, 7),
    heal_location(MAP_FALLARBOR_TOWN, 14, 8),
    heal_location(MAP_VERDANTURF_TOWN, 16, 4),
    heal_location(MAP_PACIFIDLOG_TOWN, 8, 16),
    heal_location(MAP_EVER_GRANDE_CITY, 18, 6),
    heal_location(MAP_SOUTHERN_ISLAND_EXTERIOR, 15, 20),
    heal_location(MAP_BATTLE_FRONTIER_OUTSIDE_EAST, 3, 52),
];

#[inline]
fn find_heal_location_index_by_map(map_group: U16, map_num: U16) -> U32 {
    for (index, heal_location) in S_HEAL_LOCATIONS.iter().enumerate() {
        if heal_location.map_group as U16 == map_group && heal_location.map_num as U16 == map_num {
            return index as U32 + 1;
        }
    }

    HEAL_LOCATION_NONE
}

#[inline]
fn get_heal_location(index: U32) -> *const HealLocation {
    if index == HEAL_LOCATION_NONE || index as usize > S_HEAL_LOCATIONS.len() {
        ptr::null()
    } else {
        S_HEAL_LOCATIONS.as_ptr().wrapping_add(index as usize - 1)
    }
}

#[no_mangle]
pub extern "C" fn GetHealLocationIndexByMap(map_group: U16, map_num: U16) -> U32 {
    find_heal_location_index_by_map(map_group, map_num)
}

#[no_mangle]
pub extern "C" fn GetHealLocationByMap(map_group: U16, map_num: U16) -> *const HealLocation {
    get_heal_location(find_heal_location_index_by_map(map_group, map_num))
}

#[no_mangle]
pub extern "C" fn GetHealLocation(index: U32) -> *const HealLocation {
    get_heal_location(index)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn lookup_known_maps_returns_one_based_index() {
        assert_eq!(find_heal_location_index_by_map(0, 3), 6);
        assert_eq!(find_heal_location_index_by_map(26, 14), 22);
    }

    #[test]
    fn heal_location_layout_matches_c_object_stride() {
        assert_eq!(core::mem::size_of::<HealLocation>(), 8);
        assert_eq!(core::mem::align_of::<HealLocation>(), 2);

        let location = HealLocation {
            map_group: 1,
            map_num: 2,
            x: 0x3344,
            y: 0x5566,
            _padding: [0; 2],
        };
        let base = &location as *const HealLocation as usize;

        assert_eq!((&location.map_group as *const S8 as usize) - base, 0);
        assert_eq!((&location.map_num as *const S8 as usize) - base, 1);
        assert_eq!((&location.x as *const U16 as usize) - base, 2);
        assert_eq!((&location.y as *const U16 as usize) - base, 4);
    }

    #[test]
    fn duplicate_map_returns_first_match() {
        assert_eq!(find_heal_location_index_by_map(0, 9), 12);
    }

    #[test]
    fn unknown_map_returns_none() {
        assert_eq!(find_heal_location_index_by_map(99, 99), HEAL_LOCATION_NONE);
    }

    #[test]
    fn get_heal_location_rejects_none_and_out_of_range() {
        assert!(get_heal_location(HEAL_LOCATION_NONE).is_null());
        assert!(get_heal_location(23).is_null());
    }

    #[test]
    fn get_heal_location_returns_expected_fields() {
        let location = get_heal_location(6);
        assert!(!location.is_null());

        // SAFETY: Index 6 is in range and the returned pointer refers to the
        // immutable static heal-location table.
        let location = unsafe { *location };

        assert_eq!(
            location,
            HealLocation {
                map_group: 0,
                map_num: 3,
                x: 16,
                y: 39,
                _padding: [0; 2],
            }
        );
    }
}
