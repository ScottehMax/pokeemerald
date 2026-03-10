#ifndef GUARD_SOUND_H
#define GUARD_SOUND_H

#include "gba/types.h"
#include "m4a.h"

static inline void PlayBGM(u16 songId) {}
static inline void StopMapMusic(void) {}
static inline void PlaySE(u16 songId) {}
static inline void PlaySE1WithPanning(u16 songId, s8 panning) {}
static inline void PlaySE2WithPanning(u16 songId, s8 panning) {}
static inline void PlaySE12WithPanning(u16 songId, s8 panning) {}
static inline void PlayFanfare(u16 songId) {}
static inline void PlayNewMapMusic(u16 songId) {}
static inline void PlayRainStoppingSoundEffect(void) {}
static inline void FadeOutBGM(u8 speed) {}
static inline void FadeInNewBGM(u16 songId, u8 speed) {}
static inline void FadeOutAndFadeInNewBGM(u16 songId) {}
static inline void StopBGM(void) {}
static inline void ResumeBGM(void) {}
static inline bool8 IsBGMStopped(void) { return TRUE; }
static inline bool8 IsSEPlaying(void) { return FALSE; }
static inline bool8 IsFanfarePlaying(void) { return FALSE; }
static inline void WaitForFanfareToFinish(void) {}
static inline void PlayCry1(u16 species, s8 pan) {}
static inline void PlayCry2(u16 species, s8 pan, u8 vol, u8 priority) {}
static inline void PlayCry3(u16 species, s8 pan, u8 mode) {}
static inline void PlayCry4(u16 species, s8 pan, u8 mode) {}
static inline void PlayCry5(u16 species, s8 pan, u8 vol, u8 mode) {}
static inline void PlayCry6(u16 species, s8 pan) {}
static inline bool8 IsCryPlaying(void) { return FALSE; }
static inline void StopCryAndFadeOutBGM(void) {}

#endif // GUARD_SOUND_H
