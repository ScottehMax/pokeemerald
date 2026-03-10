#ifndef GUARD_POKEMON_STORAGE_SYSTEM_H
#define GUARD_POKEMON_STORAGE_SYSTEM_H

#define TOTAL_BOXES_COUNT 14
#define IN_BOX_ROWS       5
#define IN_BOX_COLUMNS    6
#define IN_BOX_COUNT      (IN_BOX_ROWS * IN_BOX_COLUMNS)
#define BOX_NAME_LENGTH   8

struct PokemonStorage
{
    u8 currentBox;
    struct BoxPokemon boxes[TOTAL_BOXES_COUNT][IN_BOX_COUNT];
    u8 boxNames[TOTAL_BOXES_COUNT][BOX_NAME_LENGTH + 1];
    u8 boxWallpapers[TOTAL_BOXES_COUNT];
};

extern struct PokemonStorage *gPokemonStoragePtr;

static inline void DrawTextWindowAndBufferTiles(const u8 *string, void *dst, u8 zero1, u8 zero2, s32 bytesToBuffer) {}
static inline u8 CountMonsInBox(u8 boxId) { return 0; }
static inline u8 GetBoxMonCount(u8 boxId) { return 0; }
static inline struct BoxPokemon *GetBoxedMonPtr(u8 boxId, u8 monId) { return NULL; }
static inline void ReleaseBoxMon(u8 boxId, u8 monId) {}
static inline u8 StorageSystemSendMonToPC(struct Pokemon *mon) { return 0; }

#endif /* GUARD_POKEMON_STORAGE_SYSTEM_H */
