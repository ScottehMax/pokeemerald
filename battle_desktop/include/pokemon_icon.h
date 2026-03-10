#ifndef GUARD_POKEMON_ICON_H
#define GUARD_POKEMON_ICON_H

#include "sprite.h"

static inline const u8 *GetMonIconTiles(u16 species, bool32 handleDeoxys) { return NULL; }
static inline void TryLoadAllMonIconPalettesAtOffset(u16 offset) {}
static inline u8 GetValidMonIconPalIndex(u16 species) { return 0; }
static inline const u8 *GetMonIconPtr(u16 species, u32 personality, bool32 handleDeoxys) { return NULL; }
static inline const u16 *GetValidMonIconPalettePtr(u16 species) { return NULL; }
static inline u16 GetIconSpecies(u16 species, u32 personality) { return species; }
static inline u16 GetUnownLetterByPersonality(u32 personality) { return 0; }
static inline u16 GetIconSpeciesNoPersonality(u16 species) { return species; }
static inline void LoadMonIconPalettes(void) {}
static inline void LoadMonIconPalette(u16 species) {}
static inline void FreeMonIconPalettes(void) {}
static inline u8 CreateMonIconNoPersonality(u16 species, void (*callback)(struct Sprite *), s16 x, s16 y, u8 subpriority, bool32 handleDeoxys) { return 0; }
static inline void FreeMonIconPalette_unused(u16 species) {}
static inline void FreeAndDestroyMonIconSprite(struct Sprite *sprite) {}
static inline u8 CreateMonIcon(u16 species, void (*callback)(struct Sprite *), s16 x, s16 y, u8 subpriority, u32 personality, bool32 handleDeoxys) { return 0; }
static inline u8 UpdateMonIconFrame(struct Sprite *sprite) { return 0; }
static inline void SpriteCB_MonIcon(struct Sprite *sprite) {}
static inline void SetPartyHPBarSprite(struct Sprite *sprite, u8 animNum) {}
static inline u8 GetMonIconPaletteIndexFromSpecies(u16 species) { return 0; }

#endif /* GUARD_POKEMON_ICON_H */
