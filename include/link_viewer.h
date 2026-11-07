#ifndef GUARD_LINK_VIEWER_H
#define GUARD_LINK_VIEWER_H

// Link Battle Viewer: shows a battle described by a host over the JOY Bus
// (see include/constants/link_viewer.h and tools/link_viewer).

// TRUE while a viewer battle is being set up or shown.
extern bool8 gLinkViewerActive;

// Boot entry point: initializes save data and opens the waiting screen.
// Holding SELECT at boot starts the normal game instead.
void CB2_LinkViewer_Boot(void);

// gBattleMainFunc once the battle intro is over.
void LinkViewer_BattleMainFunc(void);

// Called from AllocateBattleResources instead of creating a secret base
// party: fills gBattleResources->secretBase with the opponent's name.
void LinkViewer_SetUpOpponentTrainer(void);

u8 LinkViewer_GetBattleEnvironment(void);

#endif // GUARD_LINK_VIEWER_H
