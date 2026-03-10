/*
 * data_stubs.c - Stub definitions for battle data tables, battle scripts,
 *                and functions from uncompiled source modules.
 *
 * These replace: battle_bg.c, battle_gfx_sfx_util.c, recorded_battle.c,
 *                link.c, link_rfu.c, sound.c, tv.c, frontier_util.c,
 *                trainer_hill.c, pokemon_storage_system.c, etc.
 */

#include "global.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_bg.h"
#include "battle_gfx_sfx_util.h"
#include "battle_interface.h"
#include "battle_main.h"
#include "battle_message.h"
#include "bg.h"
#include "data.h"
#include "decompress.h"
#include "evolution_scene.h"
#include "field_specials.h"
#include "frontier_util.h"
#include "item.h"
#include "link.h"
#include "menu.h"
#include "menu_specialized.h"
#include "overworld.h"
#include "palette.h"
#include "party_menu.h"
#include "pokeball.h"
#include "pokedex.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "random.h"
#include "recorded_battle.h"
#include "roamer.h"
#include "script_menu.h"
#include "sound.h"
#include "trainer_hill.h"
#include "tv.h"
#include "window.h"
#include "apprentice.h"
#include "international_string_util.h"
#include "constants/battle_move_effects.h"
#include "constants/battle_script_commands.h"

/* B_SCR_OP_END = 0x3D  (battle script terminator) */
#define BSCR_END B_SCR_OP_END

/* ===========================================================================
 * Core battle scripts (damage + faint)
 * =========================================================================== */

/*
 * BattleScript_EffectHit - the generic damage script.
 *
 * Instruction byte sizes (all pointer-free; safe on 64-bit):
 *   Single-byte ops (no args): ATTACKCANCELER, ATTACKSTRING, PPREDUCE,
 *     CRITCALC, DAMAGECALC, TYPECALC, ADJUSTNORMALDAMAGE, ATTACKANIMATION,
 *     WAITANIMATION, EFFECTIVENESSSOUND, CRITMESSAGE, RESULTMESSAGE,
 *     SETEFFECTWITHCHANCE, WAITSTATE, END
 *   Two-byte ops  (battler arg): HEALTHBARUPDATE, DATAHPUPDATE
 *   Three-byte ops (u16 timer):  WAITMESSAGE
 *   Seven-byte op: TRYFAINTMON battler + FALSE(0) + 4-byte NULL (never read
 *     when flag==FALSE; Cmd_tryfaintmon uses hardcoded C pointers instead)
 *   Three-byte op: MOVEEND endMode endState
 */
const u8 BattleScript_EffectHit[] = {
    B_SCR_OP_ATTACKCANCELER,
    B_SCR_OP_ATTACKSTRING,
    B_SCR_OP_PPREDUCE,
    B_SCR_OP_CRITCALC,
    B_SCR_OP_DAMAGECALC,
    B_SCR_OP_TYPECALC,
    B_SCR_OP_ADJUSTNORMALDAMAGE,
    B_SCR_OP_ATTACKANIMATION,
    B_SCR_OP_WAITANIMATION,
    B_SCR_OP_EFFECTIVENESSSOUND,
    B_SCR_OP_WAITSTATE,
    B_SCR_OP_HEALTHBARUPDATE, BS_TARGET,
    B_SCR_OP_DATAHPUPDATE,    BS_TARGET,
    B_SCR_OP_CRITMESSAGE,
    B_SCR_OP_WAITMESSAGE, 2, 0,      /* u16 LE timer = 2 frames */
    B_SCR_OP_RESULTMESSAGE,
    B_SCR_OP_WAITMESSAGE, 2, 0,
    B_SCR_OP_SETEFFECTWITHCHANCE,
    /* tryfaintmon BS_TARGET FALSE NULL:
     *   [0]=opcode [1]=BS_TARGET [2]=0(FALSE) [3..6]=unused 4-byte field */
    B_SCR_OP_TRYFAINTMON, BS_TARGET, 0, 0, 0, 0, 0,
    B_SCR_OP_MOVEEND, 0, 0,
    B_SCR_OP_END,
};

/*
 * BattleScript_FaintTarget / BattleScript_FaintAttacker
 *
 * Called (via BattleScriptPush/jump) from Cmd_tryfaintmon when a battler
 * reaches 0 HP. After animations, checks if all team HP = 0 and sets
 * gBattleOutcome, then end2 sets gCurrentActionFuncId = B_ACTION_TRY_FINISH.
 *
 * checkteamslost: 5 bytes total (opcode + 4-byte ptr).
 * In non-link battles Cmd_checkteamslost always does += 5 WITHOUT reading
 * the embedded pointer, so the four 0-bytes are safe on 64-bit.
 */
const u8 BattleScript_FaintTarget[] = {
    B_SCR_OP_DOFAINTANIMATION,    BS_TARGET,
    B_SCR_OP_CLEAREFFECTSONFAINT, BS_TARGET,
    B_SCR_OP_WAITMESSAGE, 2, 0,
    B_SCR_OP_CHECKTEAMSLOST, 0, 0, 0, 0,   /* 4-byte ptr, never read in non-link */
    B_SCR_OP_END2,
};

const u8 BattleScript_FaintAttacker[] = {
    B_SCR_OP_DOFAINTANIMATION,    BS_ATTACKER,
    B_SCR_OP_CLEAREFFECTSONFAINT, BS_ATTACKER,
    B_SCR_OP_WAITMESSAGE, 2, 0,
    B_SCR_OP_CHECKTEAMSLOST, 0, 0, 0, 0,
    B_SCR_OP_END2,
};

/* ===========================================================================
 * Battle script data labels
 * =========================================================================== */

const u8 BattleScript_ActionSelectionItemsCantBeUsed[] = {BSCR_END};
const u8 BattleScript_ActionSwitch[]                   = {BSCR_END};
const u8 BattleScript_AbilityCuredStatus[]             = {BSCR_END};
const u8 BattleScript_AbilityNoStatLoss[]              = {BSCR_END};
const u8 BattleScript_AbilityNoSpecificStatLoss[]      = {BSCR_END};
const u8 BattleScript_AllStatsUp[]                     = {BSCR_END};
const u8 BattleScript_AlreadyAtFullHp[]                = {BSCR_END};
const u8 BattleScript_ApplySecondaryEffect[]           = {BSCR_END};
const u8 BattleScript_ArenaDoJudgment[]                = {BSCR_END};
const u8 BattleScript_ArenaTurnBeginning[]             = {BSCR_END};
const u8 BattleScript_AskIfWantsToForfeitMatch[]       = {BSCR_END};
const u8 BattleScript_AtkDefDown[]                     = {BSCR_END};
const u8 BattleScript_BRNPrevention[]                  = {BSCR_END};
const u8 BattleScript_BerryConfuseHealEnd2[]           = {BSCR_END};
const u8 BattleScript_BerryCureBrnEnd2[]               = {BSCR_END};
const u8 BattleScript_BerryCureBrnRet[]                = {BSCR_END};
const u8 BattleScript_BerryCureChosenStatusEnd2[]      = {BSCR_END};
const u8 BattleScript_BerryCureChosenStatusRet[]       = {BSCR_END};
const u8 BattleScript_BerryCureConfusionEnd2[]         = {BSCR_END};
const u8 BattleScript_BerryCureConfusionRet[]          = {BSCR_END};
const u8 BattleScript_BerryCureFrzEnd2[]               = {BSCR_END};
const u8 BattleScript_BerryCureFrzRet[]                = {BSCR_END};
const u8 BattleScript_BerryCureParRet[]                = {BSCR_END};
const u8 BattleScript_BerryCurePrlzEnd2[]              = {BSCR_END};
const u8 BattleScript_BerryCurePsnEnd2[]               = {BSCR_END};
const u8 BattleScript_BerryCurePsnRet[]                = {BSCR_END};
const u8 BattleScript_BerryCureSlpEnd2[]               = {BSCR_END};
const u8 BattleScript_BerryCureSlpRet[]                = {BSCR_END};
const u8 BattleScript_BerryFocusEnergyEnd2[]           = {BSCR_END};
const u8 BattleScript_BerryPPHealEnd2[]                = {BSCR_END};
const u8 BattleScript_BerryStatRaiseEnd2[]             = {BSCR_END};
const u8 BattleScript_BideAttack[]                     = {BSCR_END};
const u8 BattleScript_BideNoEnergyToAttack[]           = {BSCR_END};
const u8 BattleScript_BideStoringEnergy[]              = {BSCR_END};
const u8 BattleScript_BurnTurnDmg[]                    = {BSCR_END};
const u8 BattleScript_ButItFailed[]                    = {BSCR_END};
const u8 BattleScript_CastformChange[]                 = {BSCR_END};
const u8 BattleScript_ColorChangeActivates[]           = {BSCR_END};
const u8 BattleScript_CurseTurnDmg[]                   = {BSCR_END};
const u8 BattleScript_CuteCharmActivates[]             = {BSCR_END};
const u8 BattleScript_DamagingWeatherContinues[]       = {BSCR_END};
const u8 BattleScript_DampStopsExplosion[]             = {BSCR_END};
const u8 BattleScript_DefrostedViaFireMove[]           = {BSCR_END};
const u8 BattleScript_DestinyBondTakesLife[]           = {BSCR_END};
const u8 BattleScript_DisabledNoMore[]                 = {BSCR_END};
const u8 BattleScript_DrizzleActivates[]               = {BSCR_END};
const u8 BattleScript_DroughtActivates[]               = {BSCR_END};
const u8 BattleScript_EncoredNoMore[]                  = {BSCR_END};
const u8 BattleScript_EnduredMsg[]                     = {BSCR_END};
/* BattleScript_FaintAttacker and BattleScript_FaintTarget defined above */
const u8 BattleScript_FlashFireBoost[]                 = {BSCR_END};
const u8 BattleScript_FlashFireBoost_PPLoss[]          = {BSCR_END};
const u8 BattleScript_FlinchPrevention[]               = {BSCR_END};
const u8 BattleScript_FlushMessageBox[]                = {BSCR_END};
const u8 BattleScript_FocusBandActivates[]             = {BSCR_END};
const u8 BattleScript_FocusPunchSetUp[]                = {BSCR_END};
const u8 BattleScript_FrontierLinkBattleLost[]         = {BSCR_END};
const u8 BattleScript_FrontierTrainerBattleWon[]       = {BSCR_END};
const u8 BattleScript_GiveExp[]                        = {B_SCR_OP_END2};
const u8 BattleScript_GotAwaySafely[]                  = {BSCR_END};
const u8 BattleScript_GrudgeTakesPp[]                  = {BSCR_END};
/* HandleFaintedMon: run checkteamslost (pointer-free in non-link: += 5), then end.
 * If a team has 0 total HP, checkteamslost sets gBattleOutcome and the battle exits.
 * For multi-mon teams we'd also need openpartyscreen / switch-in here, but that
 * requires pointer-embedding opcodes (64-bit unsafe); implement Option A when needed. */
const u8 BattleScript_HandleFaintedMon[]               = {
    B_SCR_OP_CHECKTEAMSLOST, 0, 0, 0, 0,
    B_SCR_OP_END2,
};
const u8 BattleScript_HitFromCritCalc[]                = {BSCR_END};
const u8 BattleScript_IgnoresAndFallsAsleep[]          = {BSCR_END};
const u8 BattleScript_IgnoresAndHitsItself[]           = {BSCR_END};
const u8 BattleScript_IgnoresAndUsesRandomMove[]       = {BSCR_END};
const u8 BattleScript_IgnoresWhileAsleep[]             = {BSCR_END};
const u8 BattleScript_IngrainTurnHeal[]                = {BSCR_END};
const u8 BattleScript_IntimidateActivates[]            = {BSCR_END};
const u8 BattleScript_IntimidateActivatesEnd3[]        = {BSCR_END};
const u8 BattleScript_ItemHealHP_End2[]                = {BSCR_END};
const u8 BattleScript_ItemHealHP_RemoveItem[]          = {BSCR_END};
const u8 BattleScript_ItemHealHP_Ret[]                 = {BSCR_END};
const u8 BattleScript_ItemSteal[]                      = {BSCR_END};
const u8 BattleScript_KnockedOff[]                     = {BSCR_END};
const u8 BattleScript_LeechSeedFree[]                  = {BSCR_END};
const u8 BattleScript_LeechSeedTurnDrain[]             = {BSCR_END};
const u8 BattleScript_LevelUp[]                        = {BSCR_END};
const u8 BattleScript_LinkBattleWonOrLost[]            = {BSCR_END};
const u8 BattleScript_LocalBattleLost[]                = {BSCR_END};
const u8 BattleScript_LocalTrainerBattleWon[]          = {BSCR_END};
const u8 BattleScript_MagicCoatBounce[]                = {BSCR_END};
const u8 BattleScript_MistProtected[]                  = {BSCR_END};
const u8 BattleScript_MonMadeMoveUseless[]             = {BSCR_END};
const u8 BattleScript_MonMadeMoveUseless_PPLoss[]      = {BSCR_END};
const u8 BattleScript_MonTookFutureAttack[]            = {BSCR_END};
const u8 BattleScript_MonWokeUpInUproar[]              = {BSCR_END};
const u8 BattleScript_MoveEffectBurn[]                 = {BSCR_END};
const u8 BattleScript_MoveEffectConfusion[]            = {BSCR_END};
const u8 BattleScript_MoveEffectFreeze[]               = {BSCR_END};
const u8 BattleScript_MoveEffectParalysis[]            = {BSCR_END};
const u8 BattleScript_MoveEffectPayDay[]               = {BSCR_END};
const u8 BattleScript_MoveEffectPoison[]               = {BSCR_END};
const u8 BattleScript_MoveEffectRecoil[]               = {BSCR_END};
const u8 BattleScript_MoveEffectSleep[]                = {BSCR_END};
const u8 BattleScript_MoveEffectToxic[]                = {BSCR_END};
const u8 BattleScript_MoveEffectUproar[]               = {BSCR_END};
const u8 BattleScript_MoveEffectWrap[]                 = {BSCR_END};
const u8 BattleScript_MoveEnd[]                        = {BSCR_END};
const u8 BattleScript_MoveHPDrain[]                    = {BSCR_END};
const u8 BattleScript_MoveHPDrain_PPLoss[]             = {BSCR_END};
const u8 BattleScript_MoveMissedPause[]                = {BSCR_END};
const u8 BattleScript_MoveUsedFlinched[]               = {BSCR_END};
const u8 BattleScript_MoveUsedIsAsleep[]               = {BSCR_END};
const u8 BattleScript_MoveUsedIsConfused[]             = {BSCR_END};
const u8 BattleScript_MoveUsedIsConfusedNoMore[]       = {BSCR_END};
const u8 BattleScript_MoveUsedIsDisabled[]             = {BSCR_END};
const u8 BattleScript_MoveUsedIsFrozen[]               = {BSCR_END};
const u8 BattleScript_MoveUsedIsImprisoned[]           = {BSCR_END};
const u8 BattleScript_MoveUsedIsInLove[]               = {BSCR_END};
const u8 BattleScript_MoveUsedIsInLoveCantAttack[]     = {BSCR_END};
const u8 BattleScript_MoveUsedIsParalyzed[]            = {BSCR_END};
const u8 BattleScript_MoveUsedIsTaunted[]              = {BSCR_END};
const u8 BattleScript_MoveUsedLoafingAround[]          = {BSCR_END};
const u8 BattleScript_MoveUsedMustRecharge[]           = {BSCR_END};
const u8 BattleScript_MoveUsedUnfroze[]                = {BSCR_END};
const u8 BattleScript_MoveUsedWokeUp[]                 = {BSCR_END};
const u8 BattleScript_NightmareTurnDmg[]               = {BSCR_END};
const u8 BattleScript_NoItemSteal[]                    = {BSCR_END};
const u8 BattleScript_NoMovesLeft[]                    = {BSCR_END};
const u8 BattleScript_NoPPForMove[]                    = {BSCR_END};
const u8 BattleScript_ObliviousPreventsAttraction[]    = {BSCR_END};
const u8 BattleScript_OneHitKOMsg[]                    = {BSCR_END};
const u8 BattleScript_PRLZPrevention[]                 = {BSCR_END};
const u8 BattleScript_PSNPrevention[]                  = {BSCR_END};
const u8 BattleScript_PalacePrintFlavorText[]          = {BSCR_END};
/* B_SCR_OP_RETURN pops the script stack — used so Cmd_attackanimation's
 * "push return addr, jump here" pattern comes back to the right place. */
const u8 BattleScript_Pausex20[]                       = {B_SCR_OP_RETURN};
const u8 BattleScript_PayDayMoneyAndPickUpItems[]      = {BSCR_END};
const u8 BattleScript_PerishSongCountGoesDown[]        = {BSCR_END};
const u8 BattleScript_PerishSongTakesLife[]            = {BSCR_END};
const u8 BattleScript_PoisonTurnDmg[]                  = {BSCR_END};
const u8 BattleScript_PresentHealTarget[]              = {BSCR_END};
const u8 BattleScript_PrintCantEscapeFromBattle[]      = {BSCR_END};
const u8 BattleScript_PrintCantRunFromTrainer[]        = {BSCR_END};
const u8 BattleScript_PrintFailedToRunString[]         = {BSCR_END};
const u8 BattleScript_PrintFullBox[]                   = {BSCR_END};
const u8 BattleScript_PrintPayDayMoneyString[]         = {BSCR_END};
const u8 BattleScript_PrintPlayerForfeited[]           = {BSCR_END};
const u8 BattleScript_PrintPlayerForfeitedLinkBattle[] = {BSCR_END};
const u8 BattleScript_PrintUproarOverTurns[]           = {BSCR_END};
const u8 BattleScript_RageIsBuilding[]                 = {BSCR_END};
const u8 BattleScript_RainContinuesOrEnds[]            = {BSCR_END};
const u8 BattleScript_RainDishActivates[]              = {BSCR_END};
const u8 BattleScript_RanAwayUsingMonAbility[]         = {BSCR_END};
const u8 BattleScript_RapidSpinAway[]                  = {BSCR_END};
const u8 BattleScript_RoughSkinActivates[]             = {BSCR_END};
const u8 BattleScript_SAtkDown2[]                      = {BSCR_END};
const u8 BattleScript_SafeguardEnds[]                  = {BSCR_END};
const u8 BattleScript_SandStormHailEnds[]              = {BSCR_END};
const u8 BattleScript_SandstreamActivates[]            = {BSCR_END};
const u8 BattleScript_SelectingDisabledMove[]          = {BSCR_END};
const u8 BattleScript_SelectingDisabledMoveInPalace[]  = {BSCR_END};
const u8 BattleScript_SelectingImprisonedMove[]        = {BSCR_END};
const u8 BattleScript_SelectingImprisonedMoveInPalace[] = {BSCR_END};
const u8 BattleScript_SelectingMoveWithNoPP[]          = {BSCR_END};
const u8 BattleScript_SelectingNotAllowedMoveChoiceItem[] = {BSCR_END};
const u8 BattleScript_SelectingNotAllowedMoveTaunt[]   = {BSCR_END};
const u8 BattleScript_SelectingNotAllowedMoveTauntInPalace[] = {BSCR_END};
const u8 BattleScript_SelectingTormentedMove[]         = {BSCR_END};
const u8 BattleScript_SelectingTormentedMoveInPalace[] = {BSCR_END};
const u8 BattleScript_ShakeBallThrow[]                 = {BSCR_END};
const u8 BattleScript_ShedSkinActivates[]              = {BSCR_END};
const u8 BattleScript_SideStatusWoreOff[]              = {BSCR_END};
const u8 BattleScript_SmokeBallEscape[]                = {BSCR_END};
const u8 BattleScript_SnatchedMove[]                   = {BSCR_END};
const u8 BattleScript_SoundproofProtected[]            = {BSCR_END};
const u8 BattleScript_SpeedBoostActivates[]            = {BSCR_END};
const u8 BattleScript_SpikesFree[]                     = {BSCR_END};
const u8 BattleScript_SpikesOnAttacker[]               = {BSCR_END};
const u8 BattleScript_SpikesOnFaintedBattler[]         = {BSCR_END};
const u8 BattleScript_SpikesOnTarget[]                 = {BSCR_END};
const u8 BattleScript_StatDown[]                       = {BSCR_END};
const u8 BattleScript_StatUp[]                         = {BSCR_END};
const u8 BattleScript_StickyHoldActivates[]            = {BSCR_END};
const u8 BattleScript_SturdyPreventsOHKO[]             = {BSCR_END};
const u8 BattleScript_SubstituteFade[]                 = {BSCR_END};
const u8 BattleScript_SuccessBallThrow[]               = {BSCR_END};
const u8 BattleScript_SuccessForceOut[]                = {BSCR_END};
const u8 BattleScript_SunlightContinues[]              = {BSCR_END};
const u8 BattleScript_SunlightFaded[]                  = {BSCR_END};
const u8 BattleScript_SynchronizeActivates[]           = {BSCR_END};
const u8 BattleScript_TargetPRLZHeal[]                 = {BSCR_END};
const u8 BattleScript_ThrashConfuses[]                 = {BSCR_END};
const u8 BattleScript_TookAttack[]                     = {BSCR_END};
const u8 BattleScript_TraceActivates[]                 = {BSCR_END};
const u8 BattleScript_TrainerBallBlock[]               = {BSCR_END};
const u8 BattleScript_WallyBallThrow[]                 = {BSCR_END};
const u8 BattleScript_WhiteHerbEnd2[]                  = {BSCR_END};
const u8 BattleScript_WhiteHerbRet[]                   = {BSCR_END};
const u8 BattleScript_WildMonFled[]                    = {BSCR_END};
const u8 BattleScript_WishComesTrue[]                  = {BSCR_END};
const u8 BattleScript_WrapEnds[]                       = {BSCR_END};
const u8 BattleScript_WrapFree[]                       = {BSCR_END};
const u8 BattleScript_WrapTurnDmg[]                    = {BSCR_END};
const u8 BattleScript_YawnMakesAsleep[]                = {BSCR_END};
const u8 BattleScript_OverworldWeatherStarts[]         = {BSCR_END};

/* ===========================================================================
 * Battle script pointer tables (indexed by move effect / ball type / etc.)
 * =========================================================================== */

static const u8 sBattleScriptEnd[] = {BSCR_END};

/*
 * TRADE-OFF NOTE (Option B vs Option A):
 *
 * This table points all move effects at a hand-written C byte array
 * (BattleScript_EffectHit) rather than the real assembly scripts in
 * data/battle_scripts_1.s / battle_scripts_2.s.
 *
 * WHY NOT USE THE REAL .s FILES DIRECTLY (Option A):
 *   The GBA assembly scripts embed 4-byte absolute pointers via ".4byte Label".
 *   On 64-bit desktop, T1_READ_PTR/T2_READ_PTR read those 4 bytes and cast to
 *   a pointer — but 64-bit addresses don't fit in 4 bytes, so the pointer is
 *   truncated and the engine crashes whenever a pointer opcode fires
 *   (accuracycheck, goto, jumpif*, call, ...).
 *
 * HOW TO FIX (Option A — delta-offset approach):
 *   1. Create battle_desktop/asm/macros/battle_script.inc that changes every
 *      ".4byte \ptr" to ".4byte \ptr - gBattleScriptBase" (32-bit delta).
 *   2. Add a "gBattleScriptBase::" label at the top of battle_scripts_1.s
 *      (or via a thin wrapper .s).
 *   3. Override T1_READ_PTR / T2_READ_PTR in battle_desktop/include/global.h:
 *        extern const u8 gBattleScriptBase[];
 *        #define T1_READ_PTR(p) \
 *            (u8*)((uintptr_t)gBattleScriptBase + T1_READ_32(p))
 *   4. Add battle_scripts_1.s and battle_scripts_2.s to the Makefile and
 *      remove the duplicate symbol definitions from data_stubs.c.
 *
 * Until Option A is implemented, BattleScript_EffectHit covers all damage
 * moves (it skips accuracy check and uses FALSE-flag tryfaintmon so no
 * pointer fields are read). Status/non-damage moves silently deal 0 damage.
 */
const u8 *const gBattleScriptsForMoveEffects[NUM_BATTLE_MOVE_EFFECTS] = {
    [0 ... (NUM_BATTLE_MOVE_EFFECTS - 1)] = BattleScript_EffectHit,
};

const u8 *const gBattlescriptsForBallThrow[ITEMS_COUNT] = {
    [0 ... (ITEMS_COUNT - 1)] = sBattleScriptEnd,
};

const u8 *const gBattlescriptsForRunningByItem[1] = {sBattleScriptEnd};
const u8 *const gBattlescriptsForUsingItem[2]     = {sBattleScriptEnd, sBattleScriptEnd};
const u8 *const gBattlescriptsForSafariActions[4] = {
    sBattleScriptEnd, sBattleScriptEnd, sBattleScriptEnd, sBattleScriptEnd
};

/* AI scripts table (one entry per AI logic ID; use 32 slots for safety) */
const u8 *const gBattleAI_ScriptsTable[32] = {
    [0 ... 31] = sBattleScriptEnd,
};

/* ===========================================================================
 * Sprite / picture tables
 * =========================================================================== */

static const struct SpriteFrameImage sDummyFrameImage = {NULL, 0};

const struct SpriteFrameImage gBattlerPicTable_PlayerLeft[]   = {sDummyFrameImage};
const struct SpriteFrameImage gBattlerPicTable_OpponentLeft[] = {sDummyFrameImage};
const struct SpriteFrameImage gBattlerPicTable_PlayerRight[]  = {sDummyFrameImage};
const struct SpriteFrameImage gBattlerPicTable_OpponentRight[]= {sDummyFrameImage};

const struct SpriteFrameImage gTrainerBackPicTable_Brendan[]        = {sDummyFrameImage};
const struct SpriteFrameImage gTrainerBackPicTable_May[]             = {sDummyFrameImage};
const struct SpriteFrameImage gTrainerBackPicTable_Red[]             = {sDummyFrameImage};
const struct SpriteFrameImage gTrainerBackPicTable_Leaf[]            = {sDummyFrameImage};
const struct SpriteFrameImage gTrainerBackPicTable_RubySapphireBrendan[] = {sDummyFrameImage};
const struct SpriteFrameImage gTrainerBackPicTable_RubySapphireMay[] = {sDummyFrameImage};
const struct SpriteFrameImage gTrainerBackPicTable_Wally[]           = {sDummyFrameImage};
const struct SpriteFrameImage gTrainerBackPicTable_Steven[]          = {sDummyFrameImage};

static const union AffineAnimCmd sDummyAffineAnimEnd  = {.type = AFFINEANIMCMDTYPE_END};
static const union AffineAnimCmd *const sDummyAffineAnimSeq[] = {&sDummyAffineAnimEnd, NULL};

const union AffineAnimCmd *const gAffineAnims_BattleSpritePlayerSide[]   = {sDummyAffineAnimSeq[0], NULL};
const union AffineAnimCmd *const gAffineAnims_BattleSpriteOpponentSide[] = {sDummyAffineAnimSeq[0], NULL};

static const union AnimCmd sDummyAnimEnd = {.type = -1};
static const union AnimCmd *const sDummyAnimTable[] = {&sDummyAnimEnd, NULL};
const union AnimCmd *const gAnims_MonPic[] = {&sDummyAnimEnd};

/* Mon front picture coordinates */
const struct MonCoords gMonFrontPicCoords[NUM_SPECIES + 1] = {{0}};

/* Palette tables */
const struct CompressedSpritePalette gMonPaletteTable[NUM_SPECIES + 1]      = {{{NULL}, 0}};
const struct CompressedSpritePalette gMonShinyPaletteTable[NUM_SPECIES + 1] = {{{NULL}, 0}};

/* Animation tables */
const union AnimCmd *const *const gMonFrontAnimsPtrTable[NUM_SPECIES + 1]   = {NULL};
const union AnimCmd *const *const gTrainerFrontAnimsPtrTable[256]            = {NULL};
const union AnimCmd *const *const gTrainerBackAnimsPtrTable[256]             = {NULL};

/* Castform sprite coordinates */
const struct MonCoords gCastformFrontSpriteCoords[4] = {{0}};

/* Trainer back pic table (CompressedSpriteSheet, functionally unused) */
const struct CompressedSpriteSheet gTrainerBackPicTable[1] = {{{NULL}, 0, 0}};

/* ===========================================================================
 * Pokémon/trainer name tables
 * =========================================================================== */

/* EOS = 0xFF in GF string encoding */
#define GF_EOS 0xFF

const u8 gSpeciesNames[NUM_SPECIES + 1][POKEMON_NAME_LENGTH + 1] = {
    [0 ... NUM_SPECIES] = {GF_EOS}
};
const u8 gTrainerClassNames[256][13] = {
    [0 ... 255] = {GF_EOS}
};
/* gTrainers: trainerName must be GF-EOS (0xFF) terminated */
const struct Trainer gTrainers[2] = {
    [0] = { .trainerName = {GF_EOS} },
    [1] = { .trainerName = {GF_EOS} },
};
const struct PokedexEntry gPokedexEntries[NUM_SPECIES + 1] = {{0}};

/* ===========================================================================
 * Battle background / window templates
 * =========================================================================== */

const struct BgTemplate gBattleBgTemplates[1] = {{0}};
static const struct WindowTemplate sDummyWindowTemplates[] = {DUMMY_WIN_TEMPLATE};
const struct WindowTemplate *const gBattleWindowTemplates[1] = {sDummyWindowTemplates};

/* Windows array for window system */
struct Window gWindows[64] = {{0}};

/* Map header stub */
struct MapHeader gMapHeader = {0};

/* ===========================================================================
 * Text string stubs
 * =========================================================================== */

const u8 gText_LinkStandby3[]                     = {0xFF};
const u8 gText_PkmnTransferredSomeonesPC[]         = {0xFF};
const u8 gText_PkmnTransferredSomeonesPCBoxFull[]  = {0xFF};
const u8 gText_PkmnTransferredLanettesPC[]         = {0xFF};
const u8 gText_PkmnTransferredLanettesPCBoxFull[]  = {0xFF};
const u8 BattleFrontier_BattleTowerBattleRoom_Text_RecordCouldntBeSaved[] = {0xFF};

/* ===========================================================================
 * Function stubs — battle visual/audio (battle_gfx_sfx_util.c)
 * =========================================================================== */

void HandleLowHpMusicChange(struct Pokemon *mon, u8 battler) {}
void BattleStopLowHpSound(void) {}
void HandleBattleLowHpMusicChange(void) {}
void ClearTemporarySpeciesSpriteData(u8 battler, bool8 dontClearSubstitute) {}
void AllocateMonSpritesGfx(void) {}
void FreeMonSpritesGfx(void) {}
u8 GetScaledHPFraction(s16 hp, s16 maxhp, u8 scale) { return 0; }
void StartHealthboxSlideIn(u8 battler) {}
bool8 BattleInitAllSprites(u8 *state1, u8 *battler) { return TRUE; }

/* ===========================================================================
 * Function stubs — battle background (battle_bg.c)
 * =========================================================================== */

void InitBattleBgsVideo(void) {}
void LoadBattleMenuWindowGfx(void) {}
void LoadBattleTextboxAndBackground(void) {}
void DrawBattleEntryBackground(void) {}
bool8 LoadChosenBattleElement(u8 caseId) { return FALSE; }
void FillAroundBattleWindows(void) {}
void InitLinkBattleVsScreen(u8 taskId) {}

/* ===========================================================================
 * Function stubs — link / RFU (link.c, link_rfu.c)
 * =========================================================================== */

void SetLinkStandbyCallback(void) {}
void SetCloseLinkCallback(void) {}
void LoadWirelessStatusIndicatorSpriteGfx(void) {}
void CreateWirelessStatusIndicatorSprite(u8 x, u8 y) {}
u8 GetLinkPlayerCount_2(void) { return 1; }
void DestroyTask_RfuIdle(void) {}

/* ===========================================================================
 * Function stubs — recorded battle (recorded_battle.c)
 * =========================================================================== */

void RecordedBattle_SetBattlerAction(u8 battler, u8 action) {}
void RecordedBattle_ClearBattlerAction(u8 battler, u8 numActions) {}
void RecordedBattle_CopyBattlerMoves(void) {}
void RecordedBattle_CheckMovesetChanges(u8 case_) {}
u32 RecordedBattle_GetFrontierPassFlag(void) { return 0; }
void RecordedBattle_SetPlaybackFinished(void) {}
bool32 MoveRecordedBattleToSaveData(void) { return FALSE; }
u32 GetAiScriptsInRecordedBattle(void) { return 0; }
u8 GetTextSpeedInRecordedBattle(void) { return 0; }

/* ===========================================================================
 * Function stubs — sound / music (sound.c)
 * =========================================================================== */

void ResetMapMusic(void) {}
void FadeOutMapMusic(u8 speed) {}
void PlayCry_Normal(u16 species, s8 pan) {}
void PlayCry_NormalNoDucking(u16 species, s8 pan, s8 volume, u8 priority) {}
bool8 IsCryFinished(void) { return TRUE; }
void StopCryAndClearCrySongs(void) {}
void m4aMPlayAllStop(void) {}

/* ===========================================================================
 * Function stubs — palette (palette.c)
 * =========================================================================== */

void ResetPaletteFade(void) { gPaletteFade.active = FALSE; }
void BeginFastPaletteFade(u8 submode) {}

/* ===========================================================================
 * Function stubs — party menu (party_menu.c)
 * =========================================================================== */

bool8 IsMultiBattle(void) { return FALSE; }
void BufferBattlePartyCurrentOrderBySide(u8 battler, u8 flankId) {}
void SwitchPartyOrderLinkMulti(u8 battler, u8 slot, u8 slot2) {}
void SwitchPartyMonSlots(u8 slot, u8 slot2) {}
u8 GetPartyIdFromBattlePartyId(u8 battlePartyId) { return battlePartyId; }
void ShowPartyMenuToShowcaseMultiBattleParty(void) {}
u8 *GetMonNickname(struct Pokemon *mon, u8 *dest) { if (dest) dest[0] = 0xFF; return dest; }

/* ===========================================================================
 * Function stubs — evolution / pokedex
 * =========================================================================== */

void BeginEvolutionScene(struct Pokemon *mon, u16 postEvoSpecies, bool8 canStopEvo, u8 partyId) {}
u8 DisplayCaughtMonDexPage(u16 dexNum, u32 otId, u32 personality) { return 0; }
u16 GetPokedexHeightWeight(u16 dexNum, u8 data) { return 0; }
bool32 IsNationalPokedexEnabled(void) { return FALSE; }

/* ===========================================================================
 * Function stubs — overworld / map (overworld.c)
 * =========================================================================== */

u8 GetCurrentMapType(void) { return 0; }
mapsec_u8_t GetCurrentRegionMapSectionId(void) { return 0; }

/* ===========================================================================
 * Function stubs — TV / news (tv.c)
 * =========================================================================== */

void TryPutPokemonTodayOnAir(void) {}
void TryPutBreakingNewsOnAir(void) {}

/* ===========================================================================
 * Function stubs — roamer (roamer.c)
 * =========================================================================== */

void UpdateRoamerHPStatus(struct Pokemon *mon) {}
void SetRoamerInactive(void) {}

/* ===========================================================================
 * Function stubs — frontier (frontier_util.c)
 * =========================================================================== */

u8 GetFrontierBrainTrainerClass(void) { return 0; }
void CopyFrontierBrainTrainerName(u8 *dst) { if (dst) dst[0] = 0xFF; }
void CopyFrontierTrainerText(u8 whichText, u16 trainerId) {}

/* ===========================================================================
 * Function stubs — trainer hill (trainer_hill.c)
 * =========================================================================== */

bool8 InTrainerHillChallenge(void) { return FALSE; }
u8 GetTrainerHillOpponentClass(u16 trainerId) { return 0; }
void GetTrainerHillTrainerName(u8 *dst, u16 trainerId) { if (dst) dst[0] = 0xFF; }
void InitTrainerHillBattleStruct(void) {}
void FreeTrainerHillBattleStruct(void) {}
void CopyTrainerHillTrainerText(u8 which, u16 trainerId) {}
u8 GetTrainerEncounterMusicIdInTrainerHill(u16 trainerId) { return 0; }

/* ===========================================================================
 * Function stubs — pokemon storage (pokemon_storage_system.c)
 * =========================================================================== */

u8 StorageGetCurrentBox(void) { return 0; }
u32 GetBoxMonDataAt(u8 boxId, u8 boxPosition, s32 request) { return 0; }
u8 *GetBoxNamePtr(u8 boxId) { return NULL; }
bool8 ShouldShowBoxWasFullMessage(void) { return FALSE; }
void SetPCBoxToSendMon(u8 boxId) {}

/* ===========================================================================
 * Function stubs — window / BG (window.c, bg.c)
 * =========================================================================== */

void CopyToWindowPixelBuffer(u8 windowId, const void *src, u16 size, u16 tileOffset) {}
void CopyToBgTilemapBufferRect_ChangePalette(u8 bg, const void *src, u8 destX, u8 destY,
                                              u8 rectWidth, u8 rectHeight, u8 palette) {}
int ConvertPixelWidthToTileWidth(int width) { return (width + 7) / 8; }
/* DoBgAffineSet defined in util.c (compiled) */

/* ===========================================================================
 * Function stubs — misc
 * =========================================================================== */

void DrawLevelUpWindowPg1(u16 windowId, u16 *statsBefore, u16 *statsAfter,
                          u8 bgClr, u8 fgClr, u8 shadowClr) {}
void DrawLevelUpWindowPg2(u16 windowId, u16 *currStats,
                          u8 bgClr, u8 fgClr, u8 shadowClr) {}
void GetMonLevelUpWindowStats(struct Pokemon *mon, u16 *currStats) {}
u8 GetPlayerTextSpeedDelay(void) { return 1; }

const u8 *GetApprenticeNameInLanguage(u32 apprenticeId, s32 language) { return NULL; }

u8 GetItemHoldEffect(u16 itemId) { return 0; }
u8 GetItemHoldEffectParam(u16 itemId) { return 0; }
void CopyItemName(u16 itemId, u8 *dst) { if (dst) dst[0] = 0xFF; }

void BgAffineSet(struct BgAffineSrcData *src, struct BgAffineDstData *dest, s16 numCalcs) {}

u8 RecordedBattle_BufferNewBattlerData(u8 *dst) { return 0; }
