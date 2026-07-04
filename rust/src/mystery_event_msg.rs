#![cfg_attr(all(not(test), target_arch = "arm"), no_std)]
#![allow(non_upper_case_globals)]

type U8 = u8;

const CHAR_SPACE: U8 = 0x00;
const CHAR_e_ACUTE: U8 = 0x1B;
const CHAR_APOSTROPHE: U8 = 0xB4;
const PLACEHOLDER_BEGIN: U8 = 0xFD;
const CHAR_NEWLINE: U8 = 0xFE;
const EOS: U8 = 0xFF;

const PLACEHOLDER_ID_STRING_VAR_1: U8 = 0x02;
const PLACEHOLDER_ID_STRING_VAR_2: U8 = 0x03;

const fn gba_char(byte: U8) -> U8 {
    match byte {
        b' ' => CHAR_SPACE,
        b'0'..=b'9' => 0xA1 + (byte - b'0'),
        b'!' => 0xAB,
        b'.' => 0xAD,
        b'\'' => CHAR_APOSTROPHE,
        b'A'..=b'Z' => 0xBB + (byte - b'A'),
        b'a'..=b'z' => 0xD5 + (byte - b'a'),
        b'\n' => CHAR_NEWLINE,
        _ => panic!("unsupported mystery-event text character"),
    }
}

const fn placeholder_id(bytes: &[U8], i: usize) -> (U8, usize) {
    if bytes[i] == b'{'
        && bytes[i + 1] == b'S'
        && bytes[i + 2] == b'T'
        && bytes[i + 3] == b'R'
        && bytes[i + 4] == b'_'
        && bytes[i + 5] == b'V'
        && bytes[i + 6] == b'A'
        && bytes[i + 7] == b'R'
        && bytes[i + 8] == b'_'
        && bytes[i + 10] == b'}'
    {
        match bytes[i + 9] {
            b'1' => (PLACEHOLDER_ID_STRING_VAR_1, i + 11),
            b'2' => (PLACEHOLDER_ID_STRING_VAR_2, i + 11),
            _ => panic!("unsupported mystery-event placeholder"),
        }
    } else {
        panic!("unsupported mystery-event placeholder")
    }
}

const fn gba_text<const OUT: usize>(text: &str) -> [U8; OUT] {
    let bytes = text.as_bytes();
    let mut out = [EOS; OUT];
    let mut i = 0;
    let mut j = 0;

    while i < bytes.len() {
        if bytes[i] == b'{' {
            let placeholder = placeholder_id(bytes, i);
            out[j] = PLACEHOLDER_BEGIN;
            out[j + 1] = placeholder.0;
            j += 2;
            i = placeholder.1;
        } else if bytes[i] == 0xC3 && i + 1 < bytes.len() && bytes[i + 1] == 0xA9 {
            out[j] = CHAR_e_ACUTE;
            j += 1;
            i += 2;
        } else {
            out[j] = gba_char(bytes[i]);
            j += 1;
            i += 1;
        }
    }

    out[j] = EOS;
    out
}

#[no_mangle]
#[link_section = ".rodata"]
pub static gText_MysteryEventBerry: [U8; 50] =
    gba_text("Obtained a {STR_VAR_2} BERRY!\nDad has it at PETALBURG GYM.");

#[no_mangle]
#[link_section = ".rodata"]
pub static gText_MysteryEventBerryTransform: [U8; 44] =
    gba_text("The {STR_VAR_1} BERRY transformed into\none {STR_VAR_2} BERRY.");

#[no_mangle]
#[link_section = ".rodata"]
pub static gText_MysteryEventBerryObtained: [U8; 40] =
    gba_text("The {STR_VAR_1} BERRY has already been\nobtained.");

#[no_mangle]
#[link_section = ".rodata"]
pub static gText_MysteryEventSpecialRibbon: [U8; 52] =
    gba_text("A special RIBBON was awarded to\nyour party POKéMON.");

#[no_mangle]
#[link_section = ".rodata"]
pub static gText_MysteryEventNationalDex: [U8; 54] =
    gba_text("The POKéDEX has been upgraded\nwith the NATIONAL MODE.");

#[no_mangle]
#[link_section = ".rodata"]
pub static gText_MysteryEventRareWord: [U8; 28] =
    gba_text("A rare word has been added.");

#[no_mangle]
#[link_section = ".rodata"]
pub static gText_MysteryEventSentOver: [U8; 18] = gba_text("{STR_VAR_1} was sent over!");

#[no_mangle]
#[link_section = ".rodata"]
pub static gText_MysteryEventFullParty: [U8; 47] =
    gba_text("Your party is full.\n{STR_VAR_1} could not be sent over.");

#[no_mangle]
#[link_section = ".rodata"]
pub static gText_MysteryEventNewTrainer: [U8; 36] =
    gba_text("A new TRAINER has arrived in\nHOENN.");

#[no_mangle]
#[link_section = ".rodata"]
pub static gText_MysteryEventNewAdversaryInBattleTower: [U8; 49] =
    gba_text("A new adversary has arrived in the\nBATTLE TOWER.");

#[no_mangle]
#[link_section = ".rodata"]
pub static gText_MysteryEventCantBeUsed: [U8; 41] =
    gba_text("This data can't be used in\nthis version.");

#[cfg(test)]
mod tests {
    use super::*;

    const ALL_TEXTS: [&[U8]; 11] = [
        &gText_MysteryEventBerry,
        &gText_MysteryEventBerryTransform,
        &gText_MysteryEventBerryObtained,
        &gText_MysteryEventSpecialRibbon,
        &gText_MysteryEventNationalDex,
        &gText_MysteryEventRareWord,
        &gText_MysteryEventSentOver,
        &gText_MysteryEventFullParty,
        &gText_MysteryEventNewTrainer,
        &gText_MysteryEventNewAdversaryInBattleTower,
        &gText_MysteryEventCantBeUsed,
    ];

    #[test]
    fn all_texts_are_eos_terminated() {
        for text in ALL_TEXTS {
            assert_eq!(text.last(), Some(&EOS));
        }
    }

    #[test]
    fn readable_encoder_matches_representative_c_object_bytes() {
        assert_eq!(
            &gText_MysteryEventSentOver,
            &[
                0xFD, 0x02, 0x00, 0xEB, 0xD5, 0xE7, 0x00, 0xE7, 0xD9, 0xE2, 0xE8,
                0x00, 0xE3, 0xEA, 0xD9, 0xE6, 0xAB, 0xFF,
            ]
        );
        assert_eq!(
            &gText_MysteryEventRareWord,
            &[
                0xBB, 0x00, 0xE6, 0xD5, 0xE6, 0xD9, 0x00, 0xEB, 0xE3, 0xE6, 0xD8,
                0x00, 0xDC, 0xD5, 0xE7, 0x00, 0xD6, 0xD9, 0xD9, 0xE2, 0x00, 0xD5,
                0xD8, 0xD8, 0xD9, 0xD8, 0xAD, 0xFF,
            ]
        );
    }

    #[test]
    fn placeholder_and_newline_tokens_match_charmap_ids() {
        assert!(gText_MysteryEventBerry
            .windows(2)
            .any(|w| w == [PLACEHOLDER_BEGIN, PLACEHOLDER_ID_STRING_VAR_2]));
        assert!(gText_MysteryEventFullParty
            .windows(2)
            .any(|w| w == [PLACEHOLDER_BEGIN, PLACEHOLDER_ID_STRING_VAR_1]));
        assert!(gText_MysteryEventNationalDex.contains(&CHAR_NEWLINE));
    }

    #[test]
    fn exported_lengths_match_c_object_symbols() {
        assert_eq!(gText_MysteryEventBerry.len(), 0x32);
        assert_eq!(gText_MysteryEventBerryTransform.len(), 0x2C);
        assert_eq!(gText_MysteryEventBerryObtained.len(), 0x28);
        assert_eq!(gText_MysteryEventSpecialRibbon.len(), 0x34);
        assert_eq!(gText_MysteryEventNationalDex.len(), 0x36);
        assert_eq!(gText_MysteryEventRareWord.len(), 0x1C);
        assert_eq!(gText_MysteryEventSentOver.len(), 0x12);
        assert_eq!(gText_MysteryEventFullParty.len(), 0x2F);
        assert_eq!(gText_MysteryEventNewTrainer.len(), 0x24);
        assert_eq!(gText_MysteryEventNewAdversaryInBattleTower.len(), 0x31);
        assert_eq!(gText_MysteryEventCantBeUsed.len(), 0x29);
    }
}
