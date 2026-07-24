#ifndef POKEEMERALD_SDL_OPENSLES_BUFFERING_H
#define POKEEMERALD_SDL_OPENSLES_BUFFERING_H

/*
 * SDL 2.32's OpenSL backend uses a compile-time two-buffer queue. Three
 * 512-frame buffers cover the callback gaps measured on Android without
 * changing the callback size or the game's mixer.
 *
 * This header is force-included only for SDL_openslES.c. Defining SDL's
 * internal header guard replaces that backend's private buffer declaration.
 */
#include "SDL_internal.h"
#include "SDL.h"
#include "audio/SDL_sysaudio.h"

#define _SDL_openslesaudio_h
#define _THIS SDL_AudioDevice *this
#define NUM_BUFFERS 3

struct SDL_PrivateAudioData
{
    Uint8 *mixbuff;
    int next_buffer;
    Uint8 *pmixbuff[NUM_BUFFERS];
    SDL_sem *playsem;
};

void openslES_ResumeDevices(void);
void openslES_PauseDevices(void);

#endif
