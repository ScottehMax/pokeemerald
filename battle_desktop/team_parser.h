#ifndef GUARD_BATTLE_DESKTOP_TEAM_PARSER_H
#define GUARD_BATTLE_DESKTOP_TEAM_PARSER_H

/*
 * team_parser.h — parse a Pokémon Showdown exportable team file into a party.
 *
 * The file format is standard Showdown export text, e.g.:
 *
 *   Kyogre @ Leftovers
 *   Ability: Drizzle
 *   Level: 50
 *   EVs: 252 HP / 252 SpA / 4 SpD
 *   Modest Nature
 *   - Surf
 *   - Thunder
 *   - Ice Beam
 *   - Calm Mind
 *
 *   Skarmory @ Shed Shell
 *   Ability: Sturdy
 *   Level: 50
 *   EVs: 252 HP / 252 Def / 4 SpD
 *   Impish Nature
 *   IVs: 0 SpA
 *   - Spikes
 *   - Whirlwind
 *   - Steel Wing
 *   - Roost
 *
 * Pokémon sets are separated by blank lines.
 * Up to 6 Pokémon are loaded per file; extras are silently ignored.
 *
 * Supported fields:
 *   Species / Nickname (Species) — first line, optional " @ Item" suffix
 *   Ability:    — accepted but ignored (Gen III can't force ability)
 *   Level:      — defaults to 100
 *   EVs:        — defaults to 0 in each stat
 *   IVs:        — only non-31 values need to be listed; defaults to 31
 *   Nature      — e.g. "Jolly Nature"; sets personality % 25
 *   Happiness:  — defaults to 255
 *   - Move      — up to 4 moves; Hidden Power [Type] supported
 *
 * Unsupported / ignored:
 *   Shiny, Tera Type, Gigantamax, Dynamax Level, Pokeball
 *   (none of these exist in Gen III)
 */

#include "global.h"
#include "pokemon.h"

/*
 * ParseTeamFile — load a Showdown team file into *party.
 *
 *   path       — path to the team file
 *   party      — destination party array (gPlayerParty or gEnemyParty)
 *   otIdType   — OT_ID_PLAYER_ID for the player side,
 *                OT_ID_RANDOM_NO_SHINY for the opponent side
 *
 * Returns TRUE on success (at least one mon was loaded).
 * Prints warnings to stderr for unrecognised species/move/item names.
 */
bool8 ParseTeamFile(const char *path, struct Pokemon *party, u8 otIdType);

#endif /* GUARD_BATTLE_DESKTOP_TEAM_PARSER_H */
