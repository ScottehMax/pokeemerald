#ifndef GUARD_CONSTANTS_LINK_VIEWER_H
#define GUARD_CONSTANTS_LINK_VIEWER_H

// Link Battle Viewer protocol.
//
// The viewer ROM puts the serial port in JOY Bus mode and receives a byte
// stream from the host, 4 bytes per JOY Bus write (command 0x15). In mGBA the
// host is whatever answers "Connect to Dolphin..." (see tools/link_viewer).
//
// Flow control: the host polls (command 0x00) and only writes a word when
// JOYSTAT has LV_JOYSTAT_READY set and LV_JOYSTAT_RECV clear. The ROM leaves
// JOY_RECV unread while its receive buffer is full, which stalls the host.
//
// A JOY Bus reset (command 0xFF) empties the receive buffer and clears
// LV_JOYSTAT_NEED_SYNC. The ROM sets that flag when it boots, so a host that
// sees it knows the ROM has lost the stream and must reset and resend the
// battle from the start of a message.
//
// The stream is a sequence of messages: [length][type][payload...], where
// length counts the type byte and the payload. A length byte of 0 is
// padding and is skipped. Multi-byte fields are little-endian. Sides are
// viewer sides: 0 is the player (bottom), 1 is the opponent (top).
//
// tools/link_viewer/lvproto.py reads the defines below; keep them simple
// "#define NAME value" lines.

#define LV_PROTOCOL_VERSION 1

// JOY_TRANS (read with command 0x14) is LV_MAGIC in the high half and, in
// the low half, how many stream bytes the viewer has taken from its buffer
// since the last reset (mod 0x10000). The host uses it to keep only a little
// of the stream in flight, so its idea of the battle stays close to what is
// on screen.
#define LV_MAGIC 0x4C56 // "LV"

#define LV_JOYSTAT_RECV  0x02 // set by the host's write, cleared when the ROM reads it
#define LV_JOYSTAT_READY     0x10 // the viewer is receiving
#define LV_JOYSTAT_NEED_SYNC 0x20 // the viewer booted and hasn't been reset by the host since

// Messages
#define LVMSG_RESET       0x01 // (): leave any battle, clear both teams
#define LVMSG_TRAINER     0x02 // side, gender, class (0-4, opponent only), name (GBA charset, EOS-terminated)
#define LVMSG_MON         0x03 // side, slot, species:16, level, hp:16, maxHP:16, status, flags, nickname (EOS-terminated; empty = species name)
#define LVMSG_START       0x04 // bgm:16 (0 = default), environment (0xFF = default), textDelay (0 = default)
#define LVMSG_TEXT        0x05 // text (GBA charset with LV_PH_* placeholders, EOS-terminated)
#define LVMSG_MOVE        0x06 // attackerSide, targetSide, move:16, animTurn
#define LVMSG_HIT         0x07 // side, effectiveness (LV_EFFECT_*)
#define LVMSG_HP          0x08 // side, hp:16
#define LVMSG_SWITCH_OUT  0x09 // side
#define LVMSG_SWITCH_IN   0x0A // side, slot
#define LVMSG_FAINT       0x0B // side
#define LVMSG_STATUS      0x0C // side, status (LV_STATUS_*), playAnim
#define LVMSG_STAT_CHANGE 0x0D // side, stat (STAT_ATK..STAT_EVASION), delta (signed)
#define LVMSG_ANIM        0x0E // side, animId (B_ANIM_* general animation), arg:16
#define LVMSG_END         0x0F // winner side (2 = tie)
#define LVMSG_WAIT        0x10 // frames:16
#define LVMSG_WEATHER     0x11 // weather (LV_WEATHER_*), playAnim
#define LVMSG_SE          0x12 // songId:16
#define LVMSG_STATUS_ANIM 0x13 // side, which (LV_VOLATILE_*): confusion/attract/curse/nightmare animation
#define LVMSG_CRY         0x14 // side

// LVMSG_MON flags
#define LV_MONFLAG_MALE   0x01
#define LV_MONFLAG_FEMALE 0x02
#define LV_MONFLAG_SHINY  0x04

// LVMSG_HIT effectiveness
#define LV_EFFECT_NORMAL     0
#define LV_EFFECT_SUPER      1
#define LV_EFFECT_NOT_VERY   2
#define LV_EFFECT_SILENT     3

// Major status
#define LV_STATUS_NONE 0
#define LV_STATUS_SLP  1
#define LV_STATUS_PSN  2
#define LV_STATUS_BRN  3
#define LV_STATUS_FRZ  4
#define LV_STATUS_PAR  5
#define LV_STATUS_TOX  6

#define LV_WEATHER_NONE 0
#define LV_WEATHER_RAIN 1
#define LV_WEATHER_SUN  2
#define LV_WEATHER_SAND 3
#define LV_WEATHER_HAIL 4

#define LV_VOLATILE_CONFUSION 0
#define LV_VOLATILE_ATTRACT   1
#define LV_VOLATILE_CURSE     2
#define LV_VOLATILE_NIGHTMARE 3

// LVMSG_TEXT placeholders: PLACEHOLDER_BEGIN (0xFD) followed by one of these
#define LV_PH_MOVE    0x80 // move:16, the move's name
#define LV_PH_ITEM    0x81 // item:16, the item's name
#define LV_PH_ABILITY 0x82 // ability, the ability's name
#define LV_PH_SPECIES 0x83 // species:16, the species' name

#endif // GUARD_CONSTANTS_LINK_VIEWER_H
