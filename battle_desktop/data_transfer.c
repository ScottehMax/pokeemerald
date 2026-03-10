/*
 * data_transfer.c - Mon data read/write for the desktop console controller
 *
 * Implements the GetMonData/SetMonData protocol used between the battle engine
 * and its controllers. This replaces the static CopyPlayerMonData and
 * SetPlayerMonData functions from the original player/opponent controllers.
 *
 * The data transfer protocol: the engine sends a REQUEST_*_BATTLE id in
 * gBattleBufferA[battler][1], and the controller fills gBattleBufferB with
 * the packed response. SetMonData reads the new value from gBattleBufferA[4..].
 */

#include "global.h"
#include "battle.h"
#include "battle_controllers.h"
#include "pokemon.h"
#include "string_util.h"

/* ===========================================================================
 * CopyMonData: read mon data from a party Pokemon and pack it into dst.
 * Returns number of bytes written.
 * =========================================================================== */

u32 CopyMonData(struct Pokemon *party, u8 monId, u8 *dst)
{
    struct BattlePokemon battleMon;
    struct MovePpInfo moveData;
    u8 nickname[POKEMON_NAME_BUFFER_SIZE];
    u8 *src;
    s16 data16;
    u32 data32;
    s32 size = 0;
    u8 request = gBattleBufferA[gActiveBattler][1];

    switch (request)
    {
    case REQUEST_ALL_BATTLE:
        battleMon.species    = GetMonData(&party[monId], MON_DATA_SPECIES, NULL);
        battleMon.item       = GetMonData(&party[monId], MON_DATA_HELD_ITEM, NULL);
        for (size = 0; size < MAX_MON_MOVES; size++)
        {
            battleMon.moves[size] = GetMonData(&party[monId], MON_DATA_MOVE1 + size, NULL);
            battleMon.pp[size]    = GetMonData(&party[monId], MON_DATA_PP1 + size, NULL);
        }
        battleMon.ppBonuses  = GetMonData(&party[monId], MON_DATA_PP_BONUSES, NULL);
        battleMon.friendship = GetMonData(&party[monId], MON_DATA_FRIENDSHIP, NULL);
        battleMon.experience = GetMonData(&party[monId], MON_DATA_EXP, NULL);
        battleMon.hpIV       = GetMonData(&party[monId], MON_DATA_HP_IV, NULL);
        battleMon.attackIV   = GetMonData(&party[monId], MON_DATA_ATK_IV, NULL);
        battleMon.defenseIV  = GetMonData(&party[monId], MON_DATA_DEF_IV, NULL);
        battleMon.speedIV    = GetMonData(&party[monId], MON_DATA_SPEED_IV, NULL);
        battleMon.spAttackIV = GetMonData(&party[monId], MON_DATA_SPATK_IV, NULL);
        battleMon.spDefenseIV = GetMonData(&party[monId], MON_DATA_SPDEF_IV, NULL);
        battleMon.personality = GetMonData(&party[monId], MON_DATA_PERSONALITY, NULL);
        battleMon.status1    = GetMonData(&party[monId], MON_DATA_STATUS, NULL);
        battleMon.level      = GetMonData(&party[monId], MON_DATA_LEVEL, NULL);
        battleMon.hp         = GetMonData(&party[monId], MON_DATA_HP, NULL);
        battleMon.maxHP      = GetMonData(&party[monId], MON_DATA_MAX_HP, NULL);
        battleMon.attack     = GetMonData(&party[monId], MON_DATA_ATK, NULL);
        battleMon.defense    = GetMonData(&party[monId], MON_DATA_DEF, NULL);
        battleMon.speed      = GetMonData(&party[monId], MON_DATA_SPEED, NULL);
        battleMon.spAttack   = GetMonData(&party[monId], MON_DATA_SPATK, NULL);
        battleMon.spDefense  = GetMonData(&party[monId], MON_DATA_SPDEF, NULL);
        battleMon.isEgg      = GetMonData(&party[monId], MON_DATA_IS_EGG, NULL);
        battleMon.abilityNum = GetMonData(&party[monId], MON_DATA_ABILITY_NUM, NULL);
        battleMon.otId       = GetMonData(&party[monId], MON_DATA_OT_ID, NULL);
        GetMonData(&party[monId], MON_DATA_NICKNAME, nickname);
        StringCopy_Nickname(battleMon.nickname, nickname);
        GetMonData(&party[monId], MON_DATA_OT_NAME, battleMon.otName);
        src = (u8 *)&battleMon;
        for (size = 0; size < (s32)sizeof(battleMon); size++)
            dst[size] = src[size];
        break;

    case REQUEST_SPECIES_BATTLE:
        data16 = GetMonData(&party[monId], MON_DATA_SPECIES, NULL);
        dst[0] = data16; dst[1] = data16 >> 8; size = 2; break;

    case REQUEST_HELDITEM_BATTLE:
        data16 = GetMonData(&party[monId], MON_DATA_HELD_ITEM, NULL);
        dst[0] = data16; dst[1] = data16 >> 8; size = 2; break;

    case REQUEST_MOVES_PP_BATTLE:
        for (size = 0; size < MAX_MON_MOVES; size++)
        {
            moveData.moves[size] = GetMonData(&party[monId], MON_DATA_MOVE1 + size, NULL);
            moveData.pp[size]    = GetMonData(&party[monId], MON_DATA_PP1 + size, NULL);
        }
        moveData.ppBonuses = GetMonData(&party[monId], MON_DATA_PP_BONUSES, NULL);
        src = (u8 *)(&moveData);
        for (size = 0; size < (s32)sizeof(moveData); size++)
            dst[size] = src[size];
        break;

    case REQUEST_MOVE1_BATTLE:
    case REQUEST_MOVE2_BATTLE:
    case REQUEST_MOVE3_BATTLE:
    case REQUEST_MOVE4_BATTLE:
        data16 = GetMonData(&party[monId], MON_DATA_MOVE1 + request - REQUEST_MOVE1_BATTLE, NULL);
        dst[0] = data16; dst[1] = data16 >> 8; size = 2; break;

    case REQUEST_PP_DATA_BATTLE:
        for (size = 0; size < MAX_MON_MOVES; size++)
            dst[size] = GetMonData(&party[monId], MON_DATA_PP1 + size, NULL);
        dst[size] = GetMonData(&party[monId], MON_DATA_PP_BONUSES, NULL);
        size++; break;

    case REQUEST_PPMOVE1_BATTLE:
    case REQUEST_PPMOVE2_BATTLE:
    case REQUEST_PPMOVE3_BATTLE:
    case REQUEST_PPMOVE4_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_PP1 + request - REQUEST_PPMOVE1_BATTLE, NULL);
        size = 1; break;

    case REQUEST_OTID_BATTLE:
        data32 = GetMonData(&party[monId], MON_DATA_OT_ID, NULL);
        dst[0] = data32 & 0xFF; dst[1] = (data32 >> 8) & 0xFF;
        dst[2] = (data32 >> 16) & 0xFF; size = 3; break;

    case REQUEST_EXP_BATTLE:
        data32 = GetMonData(&party[monId], MON_DATA_EXP, NULL);
        dst[0] = data32 & 0xFF; dst[1] = (data32 >> 8) & 0xFF;
        dst[2] = (data32 >> 16) & 0xFF; dst[3] = (data32 >> 24) & 0xFF;
        size = 4; break;

    case REQUEST_HP_EV_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_HP_EV, NULL); size = 1; break;
    case REQUEST_ATK_EV_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_ATK_EV, NULL); size = 1; break;
    case REQUEST_DEF_EV_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_DEF_EV, NULL); size = 1; break;
    case REQUEST_SPEED_EV_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_SPEED_EV, NULL); size = 1; break;
    case REQUEST_SPATK_EV_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_SPATK_EV, NULL); size = 1; break;
    case REQUEST_SPDEF_EV_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_SPDEF_EV, NULL); size = 1; break;
    case REQUEST_FRIENDSHIP_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_FRIENDSHIP, NULL); size = 1; break;
    case REQUEST_POKERUS_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_POKERUS, NULL); size = 1; break;
    case REQUEST_MET_LOCATION_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_MET_LOCATION, NULL); size = 1; break;
    case REQUEST_MET_LEVEL_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_MET_LEVEL, NULL); size = 1; break;
    case REQUEST_MET_GAME_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_MET_GAME, NULL); size = 1; break;
    case REQUEST_POKEBALL_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_POKEBALL, NULL); size = 1; break;

    case REQUEST_ALL_IVS_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_HP_IV, NULL);
        dst[1] = GetMonData(&party[monId], MON_DATA_ATK_IV, NULL);
        dst[2] = GetMonData(&party[monId], MON_DATA_DEF_IV, NULL);
        dst[3] = GetMonData(&party[monId], MON_DATA_SPEED_IV, NULL);
        dst[4] = GetMonData(&party[monId], MON_DATA_SPATK_IV, NULL);
        dst[5] = GetMonData(&party[monId], MON_DATA_SPDEF_IV, NULL);
        size = 6; break;

    case REQUEST_HP_IV_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_HP_IV, NULL); size = 1; break;
    case REQUEST_ATK_IV_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_ATK_IV, NULL); size = 1; break;
    case REQUEST_DEF_IV_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_DEF_IV, NULL); size = 1; break;
    case REQUEST_SPEED_IV_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_SPEED_IV, NULL); size = 1; break;
    case REQUEST_SPATK_IV_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_SPATK_IV, NULL); size = 1; break;
    case REQUEST_SPDEF_IV_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_SPDEF_IV, NULL); size = 1; break;

    case REQUEST_PERSONALITY_BATTLE:
        data32 = GetMonData(&party[monId], MON_DATA_PERSONALITY, NULL);
        dst[0] = data32 & 0xFF; dst[1] = (data32 >> 8) & 0xFF;
        dst[2] = (data32 >> 16) & 0xFF; dst[3] = (data32 >> 24) & 0xFF;
        size = 4; break;

    case REQUEST_CHECKSUM_BATTLE:
        data16 = GetMonData(&party[monId], MON_DATA_CHECKSUM, NULL);
        dst[0] = data16; dst[1] = data16 >> 8; size = 2; break;

    case REQUEST_STATUS_BATTLE:
        data32 = GetMonData(&party[monId], MON_DATA_STATUS, NULL);
        dst[0] = data32 & 0xFF; dst[1] = (data32 >> 8) & 0xFF;
        dst[2] = (data32 >> 16) & 0xFF; dst[3] = (data32 >> 24) & 0xFF;
        size = 4; break;

    case REQUEST_LEVEL_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_LEVEL, NULL); size = 1; break;

    case REQUEST_HP_BATTLE:
        data16 = GetMonData(&party[monId], MON_DATA_HP, NULL);
        dst[0] = data16; dst[1] = data16 >> 8; size = 2; break;

    case REQUEST_MAX_HP_BATTLE:
        data16 = GetMonData(&party[monId], MON_DATA_MAX_HP, NULL);
        dst[0] = data16; dst[1] = data16 >> 8; size = 2; break;

    case REQUEST_ATK_BATTLE:
        data16 = GetMonData(&party[monId], MON_DATA_ATK, NULL);
        dst[0] = data16; dst[1] = data16 >> 8; size = 2; break;

    case REQUEST_DEF_BATTLE:
        data16 = GetMonData(&party[monId], MON_DATA_DEF, NULL);
        dst[0] = data16; dst[1] = data16 >> 8; size = 2; break;

    case REQUEST_SPEED_BATTLE:
        data16 = GetMonData(&party[monId], MON_DATA_SPEED, NULL);
        dst[0] = data16; dst[1] = data16 >> 8; size = 2; break;

    case REQUEST_SPATK_BATTLE:
        data16 = GetMonData(&party[monId], MON_DATA_SPATK, NULL);
        dst[0] = data16; dst[1] = data16 >> 8; size = 2; break;

    case REQUEST_SPDEF_BATTLE:
        data16 = GetMonData(&party[monId], MON_DATA_SPDEF, NULL);
        dst[0] = data16; dst[1] = data16 >> 8; size = 2; break;

    case REQUEST_COOL_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_COOL, NULL); size = 1; break;
    case REQUEST_BEAUTY_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_BEAUTY, NULL); size = 1; break;
    case REQUEST_CUTE_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_CUTE, NULL); size = 1; break;
    case REQUEST_SMART_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_SMART, NULL); size = 1; break;
    case REQUEST_TOUGH_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_TOUGH, NULL); size = 1; break;
    case REQUEST_SHEEN_BATTLE:
        dst[0] = GetMonData(&party[monId], MON_DATA_SHEEN, NULL); size = 1; break;

    default:
        size = 0; break;
    }
    return (u32)size;
}

/* ===========================================================================
 * SetMonData: write updated mon data from gBattleBufferA[3..] back to party.
 * =========================================================================== */

void SetMonDataFromBuffer(struct Pokemon *party, u8 monId)
{
    u8 *data = &gBattleBufferA[gActiveBattler][3];
    u8 request = gBattleBufferA[gActiveBattler][1];
    u16 data16;
    u32 data32;

    switch (request)
    {
    case REQUEST_ALL_BATTLE:
    {
        struct BattlePokemon *bmon = (struct BattlePokemon *)data;
        SetMonData(&party[monId], MON_DATA_SPECIES,    &bmon->species);
        SetMonData(&party[monId], MON_DATA_HELD_ITEM,  &bmon->item);
        for (int i = 0; i < MAX_MON_MOVES; i++) {
            SetMonData(&party[monId], MON_DATA_MOVE1 + i, &bmon->moves[i]);
            SetMonData(&party[monId], MON_DATA_PP1 + i,   &bmon->pp[i]);
        }
        SetMonData(&party[monId], MON_DATA_PP_BONUSES, &bmon->ppBonuses);
        SetMonData(&party[monId], MON_DATA_FRIENDSHIP, &bmon->friendship);
        SetMonData(&party[monId], MON_DATA_EXP,        &bmon->experience);
        SetMonData(&party[monId], MON_DATA_STATUS,     &bmon->status1);
        SetMonData(&party[monId], MON_DATA_LEVEL,      &bmon->level);
        SetMonData(&party[monId], MON_DATA_HP,         &bmon->hp);
        SetMonData(&party[monId], MON_DATA_MAX_HP,     &bmon->maxHP);
        SetMonData(&party[monId], MON_DATA_ATK,        &bmon->attack);
        SetMonData(&party[monId], MON_DATA_DEF,        &bmon->defense);
        SetMonData(&party[monId], MON_DATA_SPEED,      &bmon->speed);
        SetMonData(&party[monId], MON_DATA_SPATK,      &bmon->spAttack);
        SetMonData(&party[monId], MON_DATA_SPDEF,      &bmon->spDefense);
        break;
    }
    case REQUEST_SPECIES_BATTLE:
        data16 = data[0] | (data[1] << 8);
        SetMonData(&party[monId], MON_DATA_SPECIES, &data16); break;
    case REQUEST_HELDITEM_BATTLE:
        data16 = data[0] | (data[1] << 8);
        SetMonData(&party[monId], MON_DATA_HELD_ITEM, &data16); break;
    case REQUEST_MOVES_PP_BATTLE:
    {
        struct MovePpInfo *mpp = (struct MovePpInfo *)data;
        for (int i = 0; i < MAX_MON_MOVES; i++) {
            SetMonData(&party[monId], MON_DATA_MOVE1 + i, &mpp->moves[i]);
            SetMonData(&party[monId], MON_DATA_PP1 + i,   &mpp->pp[i]);
        }
        SetMonData(&party[monId], MON_DATA_PP_BONUSES, &mpp->ppBonuses);
        break;
    }
    case REQUEST_MOVE1_BATTLE:
    case REQUEST_MOVE2_BATTLE:
    case REQUEST_MOVE3_BATTLE:
    case REQUEST_MOVE4_BATTLE:
        data16 = data[0] | (data[1] << 8);
        SetMonData(&party[monId], MON_DATA_MOVE1 + request - REQUEST_MOVE1_BATTLE, &data16); break;
    case REQUEST_PP_DATA_BATTLE:
        for (int i = 0; i < MAX_MON_MOVES; i++)
            SetMonData(&party[monId], MON_DATA_PP1 + i, &data[i]);
        SetMonData(&party[monId], MON_DATA_PP_BONUSES, &data[MAX_MON_MOVES]);
        break;
    case REQUEST_PPMOVE1_BATTLE:
    case REQUEST_PPMOVE2_BATTLE:
    case REQUEST_PPMOVE3_BATTLE:
    case REQUEST_PPMOVE4_BATTLE:
        SetMonData(&party[monId], MON_DATA_PP1 + request - REQUEST_PPMOVE1_BATTLE, &data[0]); break;
    case REQUEST_EXP_BATTLE:
        data32 = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        SetMonData(&party[monId], MON_DATA_EXP, &data32); break;
    case REQUEST_STATUS_BATTLE:
        data32 = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        SetMonData(&party[monId], MON_DATA_STATUS, &data32); break;
    case REQUEST_LEVEL_BATTLE:
        SetMonData(&party[monId], MON_DATA_LEVEL, &data[0]); break;
    case REQUEST_HP_BATTLE:
        data16 = data[0] | (data[1] << 8);
        SetMonData(&party[monId], MON_DATA_HP, &data16); break;
    case REQUEST_MAX_HP_BATTLE:
        data16 = data[0] | (data[1] << 8);
        SetMonData(&party[monId], MON_DATA_MAX_HP, &data16); break;
    case REQUEST_ATK_BATTLE:
        data16 = data[0] | (data[1] << 8);
        SetMonData(&party[monId], MON_DATA_ATK, &data16); break;
    case REQUEST_DEF_BATTLE:
        data16 = data[0] | (data[1] << 8);
        SetMonData(&party[monId], MON_DATA_DEF, &data16); break;
    case REQUEST_SPEED_BATTLE:
        data16 = data[0] | (data[1] << 8);
        SetMonData(&party[monId], MON_DATA_SPEED, &data16); break;
    case REQUEST_SPATK_BATTLE:
        data16 = data[0] | (data[1] << 8);
        SetMonData(&party[monId], MON_DATA_SPATK, &data16); break;
    case REQUEST_SPDEF_BATTLE:
        data16 = data[0] | (data[1] << 8);
        SetMonData(&party[monId], MON_DATA_SPDEF, &data16); break;
    case REQUEST_FRIENDSHIP_BATTLE:
        SetMonData(&party[monId], MON_DATA_FRIENDSHIP, &data[0]); break;
    default: break;
    }
}

/* Public wrappers used by console_controller.c */
u32 CopyPlayerMonData(u8 monId, u8 *dst)  { return CopyMonData(gPlayerParty, monId, dst); }
u32 CopyOpponentMonData(u8 monId, u8 *dst){ return CopyMonData(gEnemyParty,  monId, dst); }
void SetPlayerMonData(u8 monId)   { SetMonDataFromBuffer(gPlayerParty, monId); }
void SetOpponentMonData(u8 monId) { SetMonDataFromBuffer(gEnemyParty,  monId); }
