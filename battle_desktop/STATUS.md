# Battle Desktop Build Status

## Goal
Extract pokeemerald's battle engine into a standalone desktop C program.
Compile with: `make -f battle_desktop/Makefile`

## Current State
**NOT COMPILING** — Multiple header conflicts remain. All infrastructure files exist but need targeted fixes.

## Remaining Compilation Errors (from last build attempt)

```
battle_desktop/include/palette.h:25:18: error: static declaration of 'AllocSpritePalette' follows non-static declaration
battle_desktop/include/palette.h:26:18: error: static declaration of 'IndexOfSpritePaletteTag' follows non-static declaration
battle_desktop/include/palette.h:27:20: error: static declaration of 'FreeSpritePaletteByTag' follows non-static declaration
battle_desktop/include/battle_interface.h:12:34: error: conflicting types for 'gBattleSpritesDataPtr'
battle_desktop/include/battle_interface.h:17:20: error: static declaration of 'HideBattlerShadowSprite' follows non-static declaration
battle_desktop/include/battle_interface.h:24:20: error: static declaration of 'InitAndLaunchSpecialAnimation' follows non-static declaration
battle_desktop/include/battle_interface.h:26:20: error: static declaration of 'ClearTemporarySpeciesSpriteData' follows non-static declaration
battle_desktop/include/berry.h:4:8: error: redefinition of 'struct Berry'
battle_desktop/include/international_string_util.h:5:19: error: conflicting types for 'GetTrainerPartnerName'
battle_desktop/stubs/stubs.c:186:1: error: variable 'gMPlayInfo_BGM' has initializer but incomplete type
battle_desktop/stubs/stubs.c:201:21: error: 'ANIM_ARGS_COUNT' undeclared
battle_desktop/stubs/stubs.c:369+: error: redefinition of 'VarGet', 'VarSet', 'FlagGet', 'FlagSet', 'FlagClear'
battle_desktop/stubs/stubs.c: redefinition of UpdateRoamerHP, UpdateRoamerAfterBattle
battle_desktop/stubs/stubs.c: redefinition of GetSafariZoneBallCount, SetSafariZoneBallCount, IsPlayerInSafariZone
battle_desktop/stubs/stubs.c: redefinition of EvolutionScene, TradeEvolutionScene
battle_desktop/stubs/stubs.c: redefinition of TryPutLotteryPickOnAir, PutFanClubLetter
battle_desktop/stubs/stubs.c: redefinition of IsFrontierTrainer, GetFrontierTrainerMonIconSpecies, SetFrontierBattlePartyIds, HandleTrainerBattleLost
battle_desktop/stubs/stubs.c: redefinition of RecordedBattle_Init, RecordedBattle_SaveParties, RecordedBattle_ClearFrontierPassFlag, RecordedBattle_CanStopPlayback, GetBattleSceneInRecordedBattle, RecordedBattle_SetActionByBattler, RecordedBattle_RecordAllBattlerData, RecordedBattle_IsRecordingBattle
battle_desktop/stubs/stubs.c: redefinition of IsTrainerHillBattle, TrainerHill_SetMonMoves
battle_desktop/stubs/stubs.c: redefinition of OpenLink, CloseLink, IsLinkMaster, GetLinkPlayerCount, IsLinkRecovery, SetSuppressLinkErrorMessage, IsLinkConnectionEstablished, SetWirelessCommType1
battle_desktop/stubs/stubs.c: redefinition of GetBerryInfo, GetBerryPower
battle_desktop/stubs/stubs.c: redefinition of IsStringSmaller, GetTrainerNameFormatted
```

## Root Cause Analysis

### Why "static follows non-static"
`include/battle.h` (NOT shadowed) does `#include "battle_gfx_sfx_util.h"` at line 11.
GCC resolves this from the SAME directory as battle.h (`include/`), finding the ORIGINAL
non-static declarations. Later, our shadow headers (`palette.h`, `battle_interface.h`)
redeclare the same functions as `static inline` → conflict.

### Why stubs.c has redefinitions
stubs.c includes shadow headers (event_data.h, roamer.h, safari_zone.h, etc.) which provide
`static inline` implementations. Then stubs.c also defines those same functions → redefinition.

### Why gBattleSpritesDataPtr conflicts
Original `include/battle.h` declares: `extern struct BattleSpriteData *gBattleSpritesDataPtr`
Our shadow `battle_interface.h` declares: `extern struct BattleSpritesData *gBattleSpritesDataPtr`
Different struct names. Must use `struct BattleSpriteData` (the original).

### Why struct Berry conflicts
`struct Berry` is defined in `include/global.berry.h`, included via `include/global.h`.
Our shadow `global.h` chains to original via `#include_next "global.h"`, so Berry is
already defined. Our shadow `berry.h` redefines it → conflict.

### Why MPlayInfo_t is incomplete
`include/m4a.h` uses `struct MusicPlayerInfo`, NOT `MPlayInfo_t`. stubs.c uses the wrong name.

### Why ANIM_ARGS_COUNT is undeclared in stubs.c
stubs.c doesn't include `battle_anim.h`. `ANIM_ARGS_COUNT` (= 8) is defined there.

### Why GetTrainerPartnerName conflicts
`include/pokemon.h` declares: `const u8 *GetTrainerPartnerName(void)` (defined in pokemon.c)
Our shadow `international_string_util.h` declares: `static inline u8 *GetTrainerPartnerName(u8 *dst)`
Wrong return type and wrong parameter. Remove from shadow.

## Fixes Required (in order)

### Fix 1: `battle_desktop/include/palette.h`
Remove lines 25-27 (the 3 static inline palette functions):
```c
// REMOVE these 3 lines:
static inline u8 AllocSpritePalette(u16 tag) { return 0; }
static inline u8 IndexOfSpritePaletteTag(u16 tag) { return 0xFF; }
static inline void FreeSpritePaletteByTag(u16 tag) {}
```
These are declared non-statically in original `include/sprite.h`. Add real implementations to stubs.c.
NOTE: shadow `battle_desktop/include/sprite.h` line 140 also declares FreeSpritePaletteByTag non-statically.

### Fix 2: `battle_desktop/include/battle_interface.h`
Remove:
- The `struct BattleSpritesData` definition (use original `struct BattleSpriteData` from battle.h)
- `extern struct BattleSpritesData *gBattleSpritesDataPtr` (already in original battle.h)
- `static inline void HideBattlerShadowSprite(...)` (in original battle_gfx_sfx_util.h)
- `static inline void InitAndLaunchSpecialAnimation(...)` (in original battle_gfx_sfx_util.h)
- `static inline void ClearTemporarySpeciesSpriteData(...)` (in original battle_gfx_sfx_util.h)
- `void AllocateBattleSpritesData(void)` and `void FreeBattleSpritesData(void)` declarations
  (already in battle_gfx_sfx_util.h shadow)

Keep: CreateHealthboxSprite, UpdateHealthboxAttribute, SetHealthboxSpriteVisible,
SetHealthboxSpriteInvisible, UpdateHpTextInHealthbox, MoveBattleBar, TryShinyAnimation,
IsBattlerSpritePresent, BattleArena_DeductSkillPoints, BattleArena_AllocateBattleArenaData,
BattleArena_FreeBattleArenaData.
Also keep: `extern u8 gHealthboxSpriteIds[]` and `extern u8 gBattlerSpriteIds[]`.

### Fix 3: `battle_desktop/include/berry.h`
Remove the `struct Berry { ... }` definition (it comes from global.berry.h already).
Keep GetBerryInfo and GetBerryPower as static inline (remove their definitions from stubs.c).

### Fix 4: `battle_desktop/include/international_string_util.h`
Remove entirely: GetTrainerPartnerName (declared in pokemon.h with incompatible sig)
Remove entirely: StringAppendN, StringCompare, StringLength (in string_util.h/c)
Remove entirely: GetTrainerNameFormatted, IsStringSmaller (not used in battle code)
Final file should be nearly empty or just include guards.

### Fix 5: `battle_desktop/stubs/stubs.c`

**A. Fix MPlayInfo_t → struct MusicPlayerInfo** (lines 186-189):
```c
// Change:
MPlayInfo_t gMPlayInfo_BGM = {0};
// To:
struct MusicPlayerInfo gMPlayInfo_BGM = {0};
// Same for SE1/SE2/SE3
```

**B. Fix ANIM_ARGS_COUNT** — add `#include "battle_anim.h"` to includes section.

**C. Fix gBattleSpritesDataPtr type** (lines 181-183):
The original `struct BattleSpriteData` has 4 fields (battlerData, healthBoxesData, animationData, battleBars), all pointers to other structs. Must allocate all 4.
```c
// Replace static struct BattleSpritesData with:
static struct BattleSpriteInfo      sBattlerData[MAX_BATTLERS_COUNT];
static struct BattleHealthboxInfo   sHealthBoxesData[MAX_BATTLERS_COUNT];
static struct BattleAnimationInfo   sAnimationData[MAX_BATTLERS_COUNT];
static struct BattleBarInfo         sBattleBars[MAX_BATTLERS_COUNT];
static struct BattleSpriteData      sBattleSpritesData = {
    .battlerData    = sBattlerData,
    .healthBoxesData = sHealthBoxesData,
    .animationData  = sAnimationData,
    .battleBars     = sBattleBars,
};
struct BattleSpriteData *gBattleSpritesDataPtr = &sBattleSpritesData;
```

**D. Fix AllocateBattleSpritesData** to use real struct:
```c
void AllocateBattleSpritesData(void) {
    memset(sBattlerData, 0, sizeof(sBattlerData));
    memset(sHealthBoxesData, 0, sizeof(sHealthBoxesData));
    memset(sAnimationData, 0, sizeof(sAnimationData));
    memset(sBattleBars, 0, sizeof(sBattleBars));
    gBattleSpritesDataPtr = &sBattleSpritesData;
    gBattleSpritesDataPtr->battlerData = sBattlerData;
    gBattleSpritesDataPtr->healthBoxesData = sHealthBoxesData;
    gBattleSpritesDataPtr->animationData = sAnimationData;
    gBattleSpritesDataPtr->battleBars = sBattleBars;
}
```

**E. Remove all redefined functions from stubs.c** — these are already static inline in shadow headers:
- VarGet, VarSet, FlagGet, FlagSet, FlagClear (in event_data.h shadow)
- UpdateRoamerHP, UpdateRoamerAfterBattle (in roamer.h shadow)
- GetSafariZoneBallCount, SetSafariZoneBallCount, IsPlayerInSafariZone (in safari_zone.h shadow)
- EvolutionScene, TradeEvolutionScene (in evolution_scene.h shadow)
- TryPutLotteryPickOnAir, PutFanClubLetter (in tv.h shadow)
- IsFrontierTrainer, GetFrontierTrainerMonIconSpecies, SetFrontierBattlePartyIds, HandleTrainerBattleLost (in frontier_util.h shadow)
- RecordedBattle_Init, RecordedBattle_SaveParties, RecordedBattle_ClearFrontierPassFlag, RecordedBattle_CanStopPlayback, GetBattleSceneInRecordedBattle, RecordedBattle_SetActionByBattler, RecordedBattle_RecordAllBattlerData, RecordedBattle_IsRecordingBattle (in recorded_battle.h shadow)
- IsTrainerHillBattle, TrainerHill_SetMonMoves (in trainer_hill.h shadow)
- OpenLink, CloseLink, IsLinkMaster, GetLinkPlayerCount, IsLinkRecovery, SetSuppressLinkErrorMessage, IsLinkConnectionEstablished, SetWirelessCommType1 (in link.h shadow)
- GetBerryInfo, GetBerryPower (in berry.h shadow — after Fix 3 above they stay as static inline)
- IsStringSmaller, GetTrainerNameFormatted (not needed at all)

**F. Add missing implementations to stubs.c** (removed from palette.h):
```c
u8 AllocSpritePalette(u16 tag) { return 0; }
u8 IndexOfSpritePaletteTag(u16 tag) { return 0xFF; }
// FreeSpritePaletteByTag already in stubs.c? Check — add if not present
void FreeSpritePaletteByTag(u16 tag) {}
```

Also add `extern struct BattleControllerOpponentHealthboxData` and any other missing externs
if battle.h line 714-715 declares them.

## Key Architecture Facts

### Shadow Header System
- `battle_desktop/include/` takes priority over `include/` via `-I` ordering
- BUT: headers in `include/` that `#include "other.h"` use QUOTED includes, which
  first search the current file's directory → bypass our shadows
- `include/battle.h` is NOT shadowed, so it pulls in `include/battle_gfx_sfx_util.h`
  (original, non-static) when you'd expect our shadow version

### Struct Names
- Use `struct BattleSpriteData` (from original battle.h) — has battlerData, healthBoxesData, animationData, battleBars
- Do NOT use `struct BattleSpritesData` (was our shadow — wrong name)
- `struct BattleHealthboxInfo` (original) — different from `struct BattleHealthboxAnimData` (our wrong name)

### Audio
- `struct MusicPlayerInfo` (not `MPlayInfo_t`) from `include/m4a.h`

### Battle source files being compiled
```
src/battle_main.c src/battle_script_commands.c src/battle_util.c
src/battle_util2.c src/battle_ai_script_commands.c
src/battle_ai_switch_items.c src/battle_controllers.c
src/battle_message.c src/pokemon.c src/task.c src/random.c
src/string_util.c src/international_string_util.c src/strings.c src/util.c
```

### Desktop source files
```
battle_desktop/main.c
battle_desktop/console_controller.c
battle_desktop/data_transfer.c
battle_desktop/stubs/stubs.c
```

## Build Command
```
make -f battle_desktop/Makefile
```
Or for verbose output:
```
make -f battle_desktop/Makefile 2>&1 | head -60
```

## Files Created (all in battle_desktop/)
- Makefile
- main.c
- console_controller.c
- data_transfer.c
- stubs/stubs.c
- include/global.h (shadow with #include_next)
- include/battle_anim.h (shadow)
- include/battle_arena.h (shadow)
- include/battle_bg.h (shadow)
- include/battle_gfx_sfx_util.h (shadow)
- include/battle_interface.h (shadow)
- include/battle_pike.h (shadow)
- include/battle_pyramid.h (shadow)
- include/battle_setup.h (shadow)
- include/battle_tower.h (shadow)
- include/berry.h (shadow)
- include/event_data.h (shadow)
- include/evolution_scene.h (shadow)
- include/field_specials.h (shadow)
- include/field_weather.h (shadow)
- include/frontier_util.h (shadow)
- include/international_string_util.h (shadow — needs fix)
- include/link.h (shadow)
- include/mail.h (shadow)
- include/main.h (shadow)
- include/map_groups.h (empty stub)
- include/menu.h (shadow)
- include/money.h (shadow)
- include/overworld.h (shadow)
- include/palette.h (shadow — needs fix)
- include/pokedex.h (shadow)
- include/pokemon_animation.h (shadow)
- include/pokemon_icon.h (shadow)
- include/pokemon_storage_system.h (shadow)
- include/pokemon_summary_screen.h (shadow)
- include/pokeblock.h (shadow)
- include/recorded_battle.h (shadow)
- include/reshow_battle_screen.h (shadow)
- include/roamer.h (shadow)
- include/rtc.h (shadow)
- include/safari_zone.h (shadow)
- include/scanline_effect.h (shadow)
- include/sprite.h (shadow)
- include/trainer_hill.h (shadow)
- include/tv.h (shadow)
- include/window.h (shadow)
- include/gba/ (GBA type stubs: types.h, defines.h, io_reg.h, macro.h, multiboot.h, syscall.h)
