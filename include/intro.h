#ifndef GUARD_INTRO_H
#define GUARD_INTRO_H

// Exported type declarations

// Exported RAM declarations

// Exported ROM declarations
void CB2_InitCopyrightScreenAfterBootup(void);
void CB2_InitCopyrightScreenAfterTitleScreen(void);
#if PLATFORM_PC
void CB2_LoadProfileAndInitMainMenu(void);
#endif
void PanFadeAndZoomScreen(u16 screenX, u16 screenY, u16 zoom, u16 alpha);

#endif // GUARD_INTRO_H
