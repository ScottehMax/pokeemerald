/*
 * battle_scripts.h - Desktop shadow header
 *
 * Redirects all BattleScript_* label declarations to the generated
 * battle_scripts.h, which declares them as "const u8 * const" (pointers
 * into gBattleScriptData) instead of "const u8[]" (C arrays).
 *
 * On GBA, BattleScript_X is a section label — using it as an expression
 * gives the address of the label (i.e., a pointer to the bytecode).
 * On desktop with Option A, BattleScript_X is a const u8 * const pointer
 * variable whose VALUE is &gBattleScriptData[offset].  If the original
 * "extern const u8 BattleScript_X[]" declaration is used, the compiler
 * treats BattleScript_X as an array and loads the address of the pointer
 * variable instead of its value — causing a segfault.
 */
#ifndef GUARD_BATTLE_SCRIPTS_H
#define GUARD_BATTLE_SCRIPTS_H

#include "battle_desktop/generated/battle_scripts.h"

#endif /* GUARD_BATTLE_SCRIPTS_H */
