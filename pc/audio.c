#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "gba/m4a_internal.h"
#include "main.h"
#include "pc_platform.h"
#include "pc_shared.h"

#define M4A_COMMAND_FINE 0xB1
#define M4A_COMMAND_VOICE 0xBD
#define M4A_COMMAND_TIE 0xCF
#define M4A_CLOCK_BASE 0x80
#define PC_AUDIO_MAX_FRAME_SAMPLES 805
#define PC_AUDIO_MIX_TO_S16_DIVISOR 4
#define WAVE_LOOP_FLAG 0xC0

extern void *const gMPlayJumpTableTemplate[];
extern const u8 gClockTable[];
extern const u8 gCgb3Vol[];
extern const s8 gDeltaEncodingTable[];

char SoundMainRAM[1];

struct PcMixState
{
    u32 phase;
    u32 noise;
    u16 oscillatorFrequency;
    u16 sourceFrequency;
    u8 sweepTimer;
    bool8 oscillatorEnabled;
    const struct WaveData *decodedWave;
    s8 *decoded;
    u32 decodedSize;
    bool8 decodedReverse;
};

static struct PcMixState sDirectState[MAX_DIRECTSOUND_CHANNELS];
static struct PcMixState sCgbState[4];
static u64 sSampleAccumulator;
static u32 sCgbSweepAccumulator;
static s16 sOutput[PC_AUDIO_MAX_FRAME_SAMPLES * 2];

static u8 ReadTrackByte(struct MusicPlayerTrack *track)
{
    return *track->cmdPtr++;
}

static u8 *ReadTrackPointer(struct MusicPlayerTrack *track)
{
    u32 address;

    memcpy(&address, track->cmdPtr, sizeof(address));
    track->cmdPtr += sizeof(address);
    return (u8 *)(uintptr_t)address;
}

static u8 *ReadToneSplitTable(const struct ToneData *tone)
{
    u8 *table;

    memcpy(&table, &tone->attack, sizeof(table));
    return table;
}

static bool32 ChannelIsOn(const struct SoundChannel *channel)
{
    return (channel->statusFlags & SOUND_CHANNEL_SF_ON) != 0;
}

static void ChnVolSet(struct SoundChannel *channel, struct MusicPlayerTrack *track)
{
    s32 pan = (s8)channel->rhythmPan;
    u32 velocity = channel->velocity;
    u32 right = (u32)(pan + 128) * velocity * track->volMR >> 14;
    u32 left = (u32)(127 - pan) * velocity * track->volML >> 14;

    channel->rightVolume = right > 255 ? 255 : right;
    channel->leftVolume = left > 255 ? 255 : left;
}

u32 umul3232H32(u32 multiplier, u32 multiplicand)
{
    return ((u64)multiplier * multiplicand) >> 32;
}

void SoundMainBTM(void)
{
}

void RealClearChain(void *value)
{
    struct SoundChannel *channel = value;
    struct MusicPlayerTrack *track = channel->track;
    struct SoundChannel *previous;
    struct SoundChannel *next;

    if (track == NULL)
        return;

    previous = channel->prevChannelPointer;
    next = channel->nextChannelPointer;
    if (previous != NULL)
        previous->nextChannelPointer = next;
    else
        track->chan = next;
    if (next != NULL)
        next->prevChannelPointer = previous;
    channel->track = NULL;
}

void MPlayJumpTableCopy(MPlayFunc *table)
{
    memcpy(table, gMPlayJumpTableTemplate, sizeof(MPlayFunc) * 36);
}

void TrackStop(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    struct SoundChannel *channel;

    (void)mplayInfo;
    if (!(track->flags & MPT_FLG_EXIST))
        return;

    channel = track->chan;
    while (channel != NULL)
    {
        struct SoundChannel *next = channel->nextChannelPointer;

        if (channel->statusFlags != 0)
        {
            if (channel->type & TONEDATA_TYPE_CGB)
                CgbOscOff(channel->type & TONEDATA_TYPE_CGB);
            channel->statusFlags = 0;
        }
        channel->track = NULL;
        channel = next;
    }
    track->chan = NULL;
}

void ply_fine(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    struct SoundChannel *channel = track->chan;

    (void)mplayInfo;
    while (channel != NULL)
    {
        struct SoundChannel *next = channel->nextChannelPointer;

        if (ChannelIsOn(channel))
            channel->statusFlags |= SOUND_CHANNEL_SF_STOP;
        RealClearChain(channel);
        channel = next;
    }
    track->flags = 0;
}

void ply_goto(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->cmdPtr = ReadTrackPointer(track);
}

void ply_patt(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    if (track->patternLevel >= ARRAY_COUNT(track->patternStack))
    {
        ply_fine(mplayInfo, track);
        return;
    }
    track->patternStack[track->patternLevel++] = track->cmdPtr + 4;
    ply_goto(mplayInfo, track);
}

void ply_pend(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    if (track->patternLevel != 0)
        track->cmdPtr = track->patternStack[--track->patternLevel];
}

void ply_rept(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u8 count = ReadTrackByte(track);

    if (count == 0)
    {
        ply_goto(mplayInfo, track);
        return;
    }

    track->repN++;
    if (track->repN < count)
        ply_goto(mplayInfo, track);
    else
    {
        track->repN = 0;
        track->cmdPtr += 4;
    }
}

void ply_prio(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->priority = ReadTrackByte(track);
}

void ply_tempo(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    mplayInfo->tempoD = ReadTrackByte(track) * 2;
    mplayInfo->tempoI = (u32)mplayInfo->tempoD * mplayInfo->tempoU >> 8;
}

void ply_keysh(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->keyShift = ReadTrackByte(track);
    track->flags |= MPT_FLG_PITCHG;
}

void ply_voice(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u8 voice = ReadTrackByte(track);

    track->tone = mplayInfo->tone[voice];
}

void ply_vol(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->vol = ReadTrackByte(track);
    track->flags |= MPT_FLG_VOLCHG;
}

void ply_pan(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->pan = (s8)(ReadTrackByte(track) - C_V);
    track->flags |= MPT_FLG_VOLCHG;
}

void ply_bend(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->bend = (s8)(ReadTrackByte(track) - C_V);
    track->flags |= MPT_FLG_PITCHG;
}

void ply_bendr(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->bendRange = ReadTrackByte(track);
    track->flags |= MPT_FLG_PITCHG;
}

void ply_lfos(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->lfoSpeed = ReadTrackByte(track);
    if (track->lfoSpeed == 0)
        ClearModM(track);
}

void ply_lfodl(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->lfoDelay = ReadTrackByte(track);
}

void ply_mod(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->mod = ReadTrackByte(track);
    if (track->mod == 0)
        ClearModM(track);
}

void ply_modt(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u8 value;

    (void)mplayInfo;
    value = ReadTrackByte(track);
    if (track->modT != value)
    {
        track->modT = value;
        track->flags |= MPT_FLG_VOLCHG | MPT_FLG_PITCHG;
    }
}

void ply_tune(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    (void)mplayInfo;
    track->tune = (s8)(ReadTrackByte(track) - C_V);
    track->flags |= MPT_FLG_PITCHG;
}

void ply_port(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    u8 offset = ReadTrackByte(track);
    u8 value = ReadTrackByte(track);

    (void)mplayInfo;
    *(vu8 *)(REG_ADDR_SOUND1CNT_L + offset) = value;
}

void ply_endtie(struct MusicPlayerInfo *mplayInfo, struct MusicPlayerTrack *track)
{
    struct SoundChannel *channel;
    u8 key = track->key;

    (void)mplayInfo;
    if (*track->cmdPtr < 0x80)
        key = ReadTrackByte(track);

    for (channel = track->chan; channel != NULL; channel = channel->nextChannelPointer)
    {
        if (ChannelIsOn(channel)
         && !(channel->statusFlags & SOUND_CHANNEL_SF_STOP)
         && channel->midiKey == key)
        {
            channel->statusFlags |= SOUND_CHANNEL_SF_STOP;
            break;
        }
    }
}

static struct SoundChannel *FindDirectChannel(u8 priority,
                                              struct MusicPlayerTrack *track)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;
    struct SoundChannel *best = NULL;
    u8 bestPriority = priority;
    struct MusicPlayerTrack *bestTrack = track;
    bool8 foundStopping = FALSE;
    u8 i;

    for (i = 0; i < soundInfo->maxChans; i++)
    {
        struct SoundChannel *channel = &soundInfo->chans[i];

        if (!ChannelIsOn(channel))
            return channel;

        if (channel->statusFlags & SOUND_CHANNEL_SF_STOP)
        {
            if (!foundStopping)
            {
                foundStopping = TRUE;
                best = channel;
                bestPriority = channel->priority;
                bestTrack = channel->track;
                continue;
            }
        }
        else if (foundStopping)
        {
            continue;
        }

        if (channel->priority < bestPriority
         || (channel->priority == bestPriority
          && (uintptr_t)channel->track > (uintptr_t)bestTrack))
        {
            best = channel;
            bestPriority = channel->priority;
            bestTrack = channel->track;
        }
    }

    return best;
}

static struct SoundChannel *FindCgbChannel(u8 type, u8 priority,
                                          struct MusicPlayerTrack *track)
{
    struct CgbChannel *channel = &SOUND_INFO_PTR->cgbChans[type - 1];

    if (!(channel->statusFlags & SOUND_CHANNEL_SF_ON)
     || (channel->statusFlags & SOUND_CHANNEL_SF_STOP)
     || channel->priority < priority
     || (channel->priority == priority
      && (uintptr_t)channel->track >= (uintptr_t)track))
        return (struct SoundChannel *)channel;
    return NULL;
}

void ply_note(u32 noteCommand,
              struct MusicPlayerInfo *mplayInfo,
              struct MusicPlayerTrack *track)
{
    struct SoundChannel *channel;
    struct ToneData *tone = &track->tone;
    u8 finalKey;
    u8 cgbType;
    u8 priority;
    s32 rhythmPan = 0;
    s32 adjustedKey;

    track->gateTime = gClockTable[noteCommand];
    if (*track->cmdPtr < 0x80)
    {
        track->key = ReadTrackByte(track);
        if (*track->cmdPtr < 0x80)
        {
            track->velocity = ReadTrackByte(track);
            if (*track->cmdPtr < 0x80)
                track->gateTime += ReadTrackByte(track);
        }
    }

    finalKey = track->key;
    if (tone->type & (TONEDATA_TYPE_SPL | TONEDATA_TYPE_RHY))
    {
        u8 index = track->key;

        if (tone->type & TONEDATA_TYPE_SPL)
        {
            u8 *splitTable = ReadToneSplitTable(tone);
            if (splitTable == NULL)
                return;
            index = splitTable[index];
        }
        tone = &((struct ToneData *)tone->wav)[index];
        if (tone->type & (TONEDATA_TYPE_SPL | TONEDATA_TYPE_RHY))
            return;
        if (track->tone.type & TONEDATA_TYPE_RHY)
        {
            finalKey = tone->key;
            if (tone->pan_sweep & 0x80)
                rhythmPan = ((s32)tone->pan_sweep - TONEDATA_P_S_PAN) * 2;
        }
    }

    priority = mplayInfo->priority + track->priority;
    if (priority < mplayInfo->priority)
        priority = 255;
    cgbType = tone->type & TONEDATA_TYPE_CGB;
    channel = cgbType
            ? FindCgbChannel(cgbType, priority, track)
            : FindDirectChannel(priority, track);
    if (channel == NULL)
        return;

    RealClearChain(channel);
    channel->prevChannelPointer = NULL;
    channel->nextChannelPointer = track->chan;
    if (track->chan != NULL)
        track->chan->prevChannelPointer = channel;
    track->chan = channel;
    channel->track = track;

    track->lfoDelayC = track->lfoDelay;
    if (track->lfoDelayC != 0)
        ClearModM(track);
    TrkVolPitSet(mplayInfo, track);

    channel->gateTime = track->gateTime;
    channel->midiKey = track->key;
    channel->velocity = track->velocity;
    channel->priority = priority;
    channel->key = finalKey;
    channel->rhythmPan = rhythmPan;
    channel->type = tone->type;
    channel->wav = tone->wav;
    channel->attack = tone->attack;
    channel->decay = tone->decay;
    channel->sustain = tone->sustain;
    channel->release = tone->release;
    channel->pseudoEchoVolume = track->pseudoEchoVolume;
    channel->pseudoEchoLength = track->pseudoEchoLength;
    channel->count = track->unk_3C;
    ChnVolSet(channel, track);

    adjustedKey = finalKey + (s8)track->keyM;
    if (adjustedKey < 0)
        adjustedKey = 0;
    if (cgbType)
    {
        struct CgbChannel *cgb = (struct CgbChannel *)channel;

        cgb->length = tone->length;
        cgb->sweep = tone->pan_sweep;
        cgb->wavePointer = (u32 *)tone->wav;
        channel->frequency = MidiKeyToCgbFreq(cgbType, adjustedKey, track->pitM);
    }
    else if (tone->wav != NULL)
    {
        channel->frequency = MidiKeyToFreq(tone->wav, adjustedKey, track->pitM);
    }
    channel->statusFlags = SOUND_CHANNEL_SF_START;
    track->flags &= 0xF0;
}

static void UpdateTrackModulation(struct MusicPlayerTrack *track)
{
    s32 wave;
    s32 modulation;

    if (track->lfoSpeed == 0 || track->mod == 0)
        return;
    if (track->lfoDelayC != 0)
    {
        track->lfoDelayC--;
        return;
    }

    track->lfoSpeedC += track->lfoSpeed;
    wave = (s8)track->lfoSpeedC;
    if (track->lfoSpeedC >= 0x40 && track->lfoSpeedC < 0xC0)
        wave = 0x80 - track->lfoSpeedC;
    modulation = track->mod * wave >> 6;
    if (track->modM != modulation)
    {
        track->modM = modulation;
        track->flags |= track->modT == 0 ? MPT_FLG_PITCHG : MPT_FLG_VOLCHG;
    }
}

static void ApplyTrackChanges(struct MusicPlayerInfo *mplayInfo,
                              struct MusicPlayerTrack *track)
{
    struct SoundChannel *channel;
    u8 changed = track->flags;

    if (!(changed & (MPT_FLG_VOLCHG | MPT_FLG_PITCHG)))
        return;
    TrkVolPitSet(mplayInfo, track);

    channel = track->chan;
    while (channel != NULL)
    {
        struct SoundChannel *next = channel->nextChannelPointer;

        if (!ChannelIsOn(channel))
        {
            RealClearChain(channel);
            channel = next;
            continue;
        }
        if (changed & MPT_FLG_VOLCHG)
            ChnVolSet(channel, track);
        if (changed & MPT_FLG_PITCHG)
        {
            s32 key = channel->key + (s8)track->keyM;
            u8 cgbType = channel->type & TONEDATA_TYPE_CGB;

            if (key < 0)
                key = 0;
            if (cgbType)
                channel->frequency = MidiKeyToCgbFreq(cgbType, key, track->pitM);
            else if (channel->wav != NULL)
                channel->frequency = MidiKeyToFreq(channel->wav, key, track->pitM);
        }
        channel = next;
    }
    track->flags &= 0xF0;
}

static void ProcessTrackTick(struct MusicPlayerInfo *mplayInfo,
                             struct MusicPlayerTrack *track)
{
    struct SoundChannel *channel = track->chan;
    u32 commandCount = 0;

    while (channel != NULL)
    {
        struct SoundChannel *next = channel->nextChannelPointer;

        if (!ChannelIsOn(channel))
            RealClearChain(channel);
        else if (channel->gateTime != 0 && --channel->gateTime == 0)
            channel->statusFlags |= SOUND_CHANNEL_SF_STOP;
        channel = next;
    }

    if (track->flags & MPT_FLG_START)
    {
        memset(track, 0, 64);
        track->flags = MPT_FLG_EXIST;
        track->bendRange = 2;
        track->volX = 64;
        track->lfoSpeed = 22;
        track->tone.type = 1;
    }

    while (track->wait == 0 && (track->flags & MPT_FLG_EXIST) && commandCount++ < 1024)
    {
        u8 command = *track->cmdPtr;

        if (command < 0x80)
        {
            command = track->runningStatus;
        }
        else
        {
            track->cmdPtr++;
            if (command >= M4A_COMMAND_VOICE)
                track->runningStatus = command;
        }

        if (command >= M4A_COMMAND_TIE)
        {
            ply_note(command - M4A_COMMAND_TIE, mplayInfo, track);
        }
        else if (command > 0xB0)
        {
            u32 index = command - M4A_COMMAND_FINE;

            mplayInfo->cmd = index;
            if (index < 36 && gMPlayJumpTable[index] != NULL)
                gMPlayJumpTable[index](mplayInfo, track);
            else
                ply_fine(mplayInfo, track);
        }
        else if (command >= M4A_CLOCK_BASE)
        {
            track->wait = gClockTable[command - M4A_CLOCK_BASE];
        }
        else
        {
            ply_fine(mplayInfo, track);
        }
    }

    if (commandCount >= 1024)
        ply_fine(mplayInfo, track);
    if (track->wait != 0)
        track->wait--;
    UpdateTrackModulation(track);
}

void MPlayMain(struct MusicPlayerInfo *mplayInfo)
{
    u32 tempo;

    if (mplayInfo->ident != ID_NUMBER)
        return;
    mplayInfo->ident++;
    if (mplayInfo->status & MUSICPLAYER_STATUS_PAUSE)
        goto done;

    FadeOutBody(mplayInfo);
    if (mplayInfo->status & MUSICPLAYER_STATUS_PAUSE)
        goto done;

    tempo = mplayInfo->tempoC + mplayInfo->tempoI;
    while (tempo >= 150)
    {
        u32 activeTracks = 0;
        u32 bit = 1;
        u8 i;

        tempo -= 150;
        for (i = 0; i < mplayInfo->trackCount; i++, bit <<= 1)
        {
            struct MusicPlayerTrack *track = &mplayInfo->tracks[i];

            if (track->flags & MPT_FLG_EXIST)
            {
                activeTracks |= bit;
                ProcessTrackTick(mplayInfo, track);
            }
        }
        mplayInfo->clock++;
        if (activeTracks == 0)
        {
            mplayInfo->status = MUSICPLAYER_STATUS_PAUSE;
            break;
        }
        mplayInfo->status = activeTracks;
    }
    mplayInfo->tempoC = tempo;

    if (!(mplayInfo->status & MUSICPLAYER_STATUS_PAUSE))
    {
        u8 i;

        for (i = 0; i < mplayInfo->trackCount; i++)
        {
            struct MusicPlayerTrack *track = &mplayInfo->tracks[i];

            if (track->flags & MPT_FLG_EXIST)
                ApplyTrackChanges(mplayInfo, track);
        }
    }

done:
    mplayInfo->ident = ID_NUMBER;
}

static struct PcMixState *GetDirectState(struct SoundChannel *channel)
{
    return &sDirectState[channel - SOUND_INFO_PTR->chans];
}

static const s8 *GetDirectSampleBase(struct SoundChannel *channel)
{
    struct PcMixState *state = GetDirectState(channel);

    if (state->decodedWave == channel->wav && state->decoded != NULL)
        return state->decoded;
    return channel->wav->data;
}

static void DecodeWaveIfNeeded(struct SoundChannel *channel)
{
    struct PcMixState *state = GetDirectState(channel);
    const struct WaveData *wave = channel->wav;
    bool8 reverse = (channel->type & 0x10) != 0;
    u32 decodedSize;
    u32 output = 0;
    u32 block;

    state->phase = 0;
    if (state->decodedWave == wave && state->decodedReverse == reverse)
        return;
    free(state->decoded);
    state->decoded = NULL;
    state->decodedWave = wave;
    state->decodedSize = 0;
    state->decodedReverse = reverse;

    if (wave == NULL || wave->size == 0)
        return;

    decodedSize = wave->size + 1;
    if (wave->type != 1)
    {
        u32 i;

        if (!reverse)
            return;
        state->decoded = malloc(decodedSize);
        if (state->decoded == NULL)
            return;
        state->decodedSize = decodedSize;
        for (i = 0; i < wave->size; i++)
            state->decoded[i] = wave->data[wave->size - i - 1];
        state->decoded[wave->size] = state->decoded[wave->size - 1];
        return;
    }

    state->decoded = malloc(decodedSize);
    if (state->decoded == NULL)
        return;
    state->decodedSize = decodedSize;
    for (block = 0; output < decodedSize; block++)
    {
        const u8 *encoded = (const u8 *)wave->data + block * 33;
        s32 sample = (s8)encoded[0];
        u32 byte;

        state->decoded[output++] = (s8)sample;
        if (output >= decodedSize)
            break;

        // Each 33-byte block contains one signed predictor followed by 63
        // delta nibbles. The high nibble of the first delta byte is padding.
        sample = (s8)(sample + gDeltaEncodingTable[encoded[1] & 0xF]);
        state->decoded[output++] = (s8)sample;
        for (byte = 2; byte < 33 && output < decodedSize; byte++)
        {
            u8 packed = encoded[byte];

            sample = (s8)(sample + gDeltaEncodingTable[packed >> 4]);
            state->decoded[output++] = (s8)sample;
            if (output >= decodedSize)
                break;
            sample = (s8)(sample + gDeltaEncodingTable[packed & 0xF]);
            state->decoded[output++] = (s8)sample;
        }
    }
    if (reverse)
    {
        u32 i;

        for (i = 0; i < wave->size / 2; i++)
        {
            s8 value = state->decoded[i];
            state->decoded[i] = state->decoded[wave->size - i - 1];
            state->decoded[wave->size - i - 1] = value;
        }
        state->decoded[wave->size] = state->decoded[wave->size - 1];
    }
}

static void StopMixedChannel(struct SoundChannel *channel)
{
    channel->statusFlags = 0;
    RealClearChain(channel);
}

static void BeginPseudoEchoOrStop(struct SoundChannel *channel)
{
    channel->envelopeVolume = channel->pseudoEchoVolume;
    if (channel->envelopeVolume != 0 && channel->pseudoEchoLength != 0)
        channel->statusFlags = SOUND_CHANNEL_SF_IEC;
    else
        StopMixedChannel(channel);
}

static void UpdateDirectEnvelope(struct SoundInfo *soundInfo, struct SoundChannel *channel)
{
    u8 envelope;
    u32 scaledEnvelope;

    if (!ChannelIsOn(channel))
        return;
    if (channel->statusFlags & SOUND_CHANNEL_SF_START)
    {
        u32 offset;

        if ((channel->statusFlags & SOUND_CHANNEL_SF_STOP) || channel->wav == NULL)
        {
            StopMixedChannel(channel);
            return;
        }
        DecodeWaveIfNeeded(channel);
        offset = channel->count;
        if (offset >= channel->wav->size)
            offset = 0;
        channel->currentPointer = (s8 *)GetDirectSampleBase(channel) + offset;
        channel->count = channel->wav->size - offset;
        channel->fw = 0;
        channel->envelopeVolume = 0;
        channel->statusFlags = SOUND_CHANNEL_SF_ENV_ATTACK;
        if (((const u8 *)channel->wav)[3] & WAVE_LOOP_FLAG)
            channel->statusFlags |= SOUND_CHANNEL_SF_LOOP;
    }
    else if (channel->statusFlags & SOUND_CHANNEL_SF_IEC)
    {
        if (channel->pseudoEchoLength == 0 || --channel->pseudoEchoLength == 0)
            StopMixedChannel(channel);
        return;
    }

    envelope = channel->envelopeVolume;
    if (channel->statusFlags & SOUND_CHANNEL_SF_STOP)
    {
        envelope = (u32)envelope * channel->release >> 8;
        if (envelope <= channel->pseudoEchoVolume)
        {
            BeginPseudoEchoOrStop(channel);
            return;
        }
    }
    else
    {
        switch (channel->statusFlags & SOUND_CHANNEL_SF_ENV)
        {
        case SOUND_CHANNEL_SF_ENV_ATTACK:
            if ((u16)envelope + channel->attack >= 255)
            {
                envelope = 255;
                channel->statusFlags = (channel->statusFlags & ~SOUND_CHANNEL_SF_ENV) | SOUND_CHANNEL_SF_ENV_DECAY;
            }
            else
                envelope += channel->attack;
            break;
        case SOUND_CHANNEL_SF_ENV_DECAY:
            envelope = (u32)envelope * channel->decay >> 8;
            if (envelope <= channel->sustain)
            {
                envelope = channel->sustain;
                if (envelope == 0)
                {
                    BeginPseudoEchoOrStop(channel);
                    return;
                }
                channel->statusFlags = (channel->statusFlags & ~SOUND_CHANNEL_SF_ENV) | SOUND_CHANNEL_SF_ENV_SUSTAIN;
            }
            break;
        }
    }
    channel->envelopeVolume = envelope;
    scaledEnvelope = (u32)envelope * (soundInfo->masterVolume + 1) >> 4;
    channel->envelopeVolumeRight = (u32)channel->rightVolume * scaledEnvelope >> 8;
    channel->envelopeVolumeLeft = (u32)channel->leftVolume * scaledEnvelope >> 8;
}

static u8 ResetStartingCgbChannels(struct SoundInfo *soundInfo)
{
    u8 i;
    u8 starting = 0;

    for (i = 0; i < 4; i++)
    {
        if (soundInfo->cgbChans[i].statusFlags & SOUND_CHANNEL_SF_START)
        {
            sCgbState[i].phase = 0;
            sCgbState[i].noise = 0x7FFF;
            starting |= 1 << i;
        }
    }
    return starting;
}

static u32 CalculateCgbSweepFrequency(u32 frequency, u8 sweep)
{
    u32 delta = frequency >> (sweep & 7);

    if (sweep & 0x08)
        return frequency - delta;
    return frequency + delta;
}

static void SyncCgbOscillatorState(struct SoundInfo *soundInfo, u8 starting)
{
    u8 i;

    for (i = 0; i < 4; i++)
    {
        struct CgbChannel *channel = &soundInfo->cgbChans[i];
        struct PcMixState *state = &sCgbState[i];
        u16 frequency = channel->frequency & 0x7FF;

        if (starting & (1 << i))
        {
            u8 sweepPeriod = (channel->sweep >> 4) & 7;

            state->oscillatorFrequency = frequency;
            state->sourceFrequency = frequency;
            state->sweepTimer = sweepPeriod == 0 ? 8 : sweepPeriod;
            state->oscillatorEnabled = ChannelIsOn((struct SoundChannel *)channel);

            // Triggering pulse channel 1 performs one overflow calculation,
            // but does not apply its result to the oscillator yet.
            if (i == 0
             && (channel->sweep & 7) != 0
             && CalculateCgbSweepFrequency(frequency, channel->sweep) > 0x7FF)
                state->oscillatorEnabled = FALSE;
        }
        else if (state->sourceFrequency != frequency)
        {
            state->sourceFrequency = frequency;
            state->oscillatorFrequency = frequency;
        }
    }
}

static void ClockCgbSweep(struct CgbChannel *channel)
{
    struct PcMixState *state = &sCgbState[0];
    u8 sweep = channel->sweep;
    u8 sweepPeriod = (sweep >> 4) & 7;
    u32 frequency;

    if (!state->oscillatorEnabled || !ChannelIsOn((struct SoundChannel *)channel))
        return;
    if (--state->sweepTimer != 0)
        return;

    state->sweepTimer = sweepPeriod == 0 ? 8 : sweepPeriod;
    if (sweepPeriod == 0 || (sweep & 7) == 0)
        return;

    frequency = CalculateCgbSweepFrequency(state->oscillatorFrequency, sweep);
    if (frequency > 0x7FF)
    {
        state->oscillatorEnabled = FALSE;
        return;
    }

    state->oscillatorFrequency = frequency;
    if (CalculateCgbSweepFrequency(frequency, sweep) > 0x7FF)
        state->oscillatorEnabled = FALSE;
}

static s32 MixDirectSample(struct SoundInfo *soundInfo, struct SoundChannel *channel)
{
    struct PcMixState *state = GetDirectState(channel);
    const s8 *base;
    u32 phase;
    u32 advance;
    s32 sample;
    s32 nextSample;

    if (!ChannelIsOn(channel) || channel->currentPointer == NULL || channel->count == 0)
        return 0;
    base = GetDirectSampleBase(channel);
    sample = *channel->currentPointer;
    if (channel->type & TONEDATA_TYPE_FIX)
    {
        advance = 1;
        state->phase = 0;
    }
    else
    {
        nextSample = channel->currentPointer[1];
        sample += (nextSample - sample) * (s32)state->phase >> 23;
        phase = state->phase + channel->frequency * soundInfo->divFreq;
        advance = phase >> 23;
        state->phase = phase & 0x7FFFFF;
    }

    while (advance != 0 && ChannelIsOn(channel))
    {
        if (advance < channel->count)
        {
            channel->currentPointer += advance;
            channel->count -= advance;
            break;
        }

        advance -= channel->count;
        if (channel->statusFlags & SOUND_CHANNEL_SF_LOOP)
        {
            u32 loopStart = channel->wav->loopStart;
            if (loopStart >= channel->wav->size)
                loopStart = 0;
            channel->currentPointer = (s8 *)base + loopStart;
            channel->count = channel->wav->size - loopStart;
            if (channel->count == 0)
            {
                StopMixedChannel(channel);
                break;
            }
        }
        else
        {
            StopMixedChannel(channel);
            break;
        }
    }
    return sample;
}

static double CgbFrequency(const struct CgbChannel *channel, u8 index)
{
    u32 type = channel->type & TONEDATA_TYPE_CGB;
    u32 value = index == 0 ? sCgbState[0].oscillatorFrequency : channel->frequency;

    if (type == 4)
    {
        u32 shift = (value >> 4) & 0xF;
        u32 divisor = value & 7;

        return 524288.0 / (divisor == 0 ? 1 : divisor * 2) / (1u << shift);
    }
    if (value >= 2048)
        value = 2047;
    return (type == 3 ? 65536.0 : 131072.0) / (2048 - value);
}

static u8 CgbWaveVolumeNumerator(u8 envelopeVolume)
{
    switch (gCgb3Vol[envelopeVolume])
    {
    case 0x20: // 100%
        return 4;
    case 0x40: // 50%
        return 2;
    case 0x60: // 25%
        return 1;
    case 0x80: // 75%
        return 3;
    default:
        return 0;
    }
}

static s32 MixCgbDacEighths(struct CgbChannel *channel, u8 index)
{
    struct PcMixState *state = &sCgbState[index];
    u64 step = (u64)(CgbFrequency(channel, index) * 4294967296.0 / PC_AUDIO_RATE);
    u32 type = channel->type & TONEDATA_TYPE_CGB;
    u32 envelope = channel->envelopeVolume;

    if (!state->oscillatorEnabled)
        return 0;

    if (type == 4)
    {
        u64 phase = (u64)state->phase + step;
        u32 clocks = phase >> 32;
        bool8 shortMode = ((uintptr_t)channel->wavePointer & 1) != 0;

        state->phase = (u32)phase;
        while (clocks-- != 0)
        {
            u32 feedback = (state->noise ^ (state->noise >> 1)) & 1;

            state->noise = (state->noise >> 1) | (feedback << 14);
            if (shortMode)
                state->noise = (state->noise & ~(1 << 6)) | (feedback << 6);
        }
        return (state->noise & 1) ? -(s32)envelope * 4 : (s32)envelope * 4;
    }

    state->phase += step;
    if (type == 3 && channel->wavePointer != NULL)
    {
        const u8 *wave = (const u8 *)channel->wavePointer;
        u8 position = state->phase >> 27;
        u8 packed = wave[position >> 1];
        u8 value = (position & 1) ? (packed & 0xF) : (packed >> 4);
        u8 volume = CgbWaveVolumeNumerator(envelope);

        return ((s32)value * 2 - 15) * volume;
    }
    else
    {
        static const u8 dutyThreshold[] = {1, 2, 4, 6};
        u8 duty = (uintptr_t)channel->wavePointer & 3;
        u8 position = state->phase >> 29;
        u8 threshold = dutyThreshold[duty];

        // The output capacitor removes the pulse wave's duty-dependent DC.
        return (position < threshold ? 8 - threshold : -(s32)threshold) * envelope;
    }
}

static s32 ScaleCgbBus(s32 dacEighths, u8 masterVolume)
{
    // Four 4-bit PSG channels form a 6-bit sum, then occupy the 9-bit mix path
    // by shifting three bits. Direct Sound shifts its signed 8-bit sample once.
    // dacEighths already contains the PSG x8 shift; x16 applies NR50 / 8 and
    // converts the resulting 9-bit value to the shared x256 Direct Sound bus.
    return dacEighths * (masterVolume + 1) * 16;
}

static s32 ApplyCgbOutputRatio(s32 sample)
{
    switch (REG_SOUNDCNT_H & 3)
    {
    case SOUND_CGB_MIX_QUARTER:
        return sample / 4;
    case SOUND_CGB_MIX_HALF:
        return sample / 2;
    default:
        return sample;
    }
}

static s16 ClipSample(s32 value)
{
    if (value > 32767)
        return 32767;
    if (value < -32768)
        return -32768;
    return value;
}

static u32 GetPcmBufferOffset(const struct SoundInfo *soundInfo)
{
    u32 samplesPerVBlank = soundInfo->pcmSamplesPerVBlank;
    u32 offset;

    if (soundInfo->pcmDmaCounter < 2)
        offset = 0;
    else
        offset = (soundInfo->pcmDmaPeriod + 1 - soundInfo->pcmDmaCounter) * samplesPerVBlank;
    if (offset + samplesPerVBlank > PCM_DMA_BUF_SIZE)
        offset = 0;
    return offset;
}

static s8 AddDirectPcmSample(s8 current, s32 contribution)
{
    return (s8)((u8)current + (u8)contribution);
}

static s32 ScaleDirectSample(s32 sample, u8 volume)
{
    return sample * (s32)volume >> 8;
}

static void InitializeDirectPcmBuffer(struct SoundInfo *soundInfo, u32 offset)
{
    u32 samplesPerVBlank = soundInfo->pcmSamplesPerVBlank;
    s8 *right = soundInfo->pcmBuffer + offset;
    s8 *left = soundInfo->pcmBuffer + PCM_DMA_BUF_SIZE + offset;

    if (soundInfo->reverb == 0)
    {
        memset(right, 0, samplesPerVBlank);
        memset(left, 0, samplesPerVBlank);
        return;
    }

    {
        u32 delayedOffset = soundInfo->pcmDmaCounter == 2 ? 0 : offset + samplesPerVBlank;
        s8 *delayedRight = soundInfo->pcmBuffer + delayedOffset;
        s8 *delayedLeft = soundInfo->pcmBuffer + PCM_DMA_BUF_SIZE + delayedOffset;
        u32 i;

        for (i = 0; i < samplesPerVBlank; i++)
        {
            s32 mixed = right[i] + left[i] + delayedRight[i] + delayedLeft[i];

            mixed = mixed * soundInfo->reverb >> 9;
            if (mixed & 0x80)
                mixed++;
            right[i] = mixed;
            left[i] = mixed;
        }
    }
}

static void MixDirectPcmBuffer(struct SoundInfo *soundInfo, u32 offset)
{
    u32 samplesPerVBlank = soundInfo->pcmSamplesPerVBlank;
    s8 *right = soundInfo->pcmBuffer + offset;
    s8 *left = soundInfo->pcmBuffer + PCM_DMA_BUF_SIZE + offset;
    u32 frame;

    InitializeDirectPcmBuffer(soundInfo, offset);
    for (frame = 0; frame < samplesPerVBlank; frame++)
    {
        u8 i;

        for (i = 0; i < soundInfo->maxChans; i++)
        {
            struct SoundChannel *channel = &soundInfo->chans[i];
            s32 sample;

            if (!ChannelIsOn(channel))
                continue;
            sample = MixDirectSample(soundInfo, channel);
            right[frame] = AddDirectPcmSample(
                right[frame],
                ScaleDirectSample(sample, channel->envelopeVolumeRight));
            left[frame] = AddDirectPcmSample(
                left[frame],
                ScaleDirectSample(sample, channel->envelopeVolumeLeft));
        }
    }
}

static void GetDirectBusSample(const struct SoundInfo *soundInfo,
                               u32 offset,
                               u32 frame,
                               s32 *left,
                               s32 *right)
{
    s32 fifoA = soundInfo->pcmBuffer[offset + frame];
    s32 fifoB = soundInfo->pcmBuffer[PCM_DMA_BUF_SIZE + offset + frame];
    u32 scaleA = (REG_SOUNDCNT_H & SOUND_A_MIX_FULL) ? 256 : 128;
    u32 scaleB = (REG_SOUNDCNT_H & SOUND_B_MIX_FULL) ? 256 : 128;

    *left = 0;
    *right = 0;
    if (REG_SOUNDCNT_H & SOUND_A_LEFT_OUTPUT)
        *left += fifoA * (s32)scaleA;
    if (REG_SOUNDCNT_H & SOUND_A_RIGHT_OUTPUT)
        *right += fifoA * (s32)scaleA;
    if (REG_SOUNDCNT_H & SOUND_B_LEFT_OUTPUT)
        *left += fifoB * (s32)scaleB;
    if (REG_SOUNDCNT_H & SOUND_B_RIGHT_OUTPUT)
        *right += fifoB * (s32)scaleB;
}

static void MixFrame(struct SoundInfo *soundInfo)
{
    u32 frameCount;
    u32 pcmOffset;
    u32 frame;

    sSampleAccumulator += (u64)PC_AUDIO_RATE * 10000;
    frameCount = sSampleAccumulator / 597275;
    sSampleAccumulator %= 597275;
    if (frameCount > PC_AUDIO_MAX_FRAME_SAMPLES)
        frameCount = PC_AUDIO_MAX_FRAME_SAMPLES;
    pcmOffset = GetPcmBufferOffset(soundInfo);
    MixDirectPcmBuffer(soundInfo, pcmOffset);

    for (frame = 0; frame < frameCount; frame++)
    {
        u32 directFrame = (u64)frame * soundInfo->pcmSamplesPerVBlank / frameCount;
        s32 left;
        s32 right;
        s32 cgbLeft = 0;
        s32 cgbRight = 0;
        u8 i;

        sCgbSweepAccumulator += 128;
        if (sCgbSweepAccumulator >= PC_AUDIO_RATE)
        {
            sCgbSweepAccumulator -= PC_AUDIO_RATE;
            ClockCgbSweep(&soundInfo->cgbChans[0]);
        }

        GetDirectBusSample(soundInfo, pcmOffset, directFrame, &left, &right);

        for (i = 0; i < 4; i++)
        {
            struct CgbChannel *channel = &soundInfo->cgbChans[i];
            s32 sample;

            if (!(channel->statusFlags & SOUND_CHANNEL_SF_ON))
                continue;
            sample = MixCgbDacEighths(channel, i);
            if (channel->pan & (0x10 << i))
                cgbLeft += ScaleCgbBus(sample, (REG_NR50 >> 4) & 7);
            if (channel->pan & (1 << i))
                cgbRight += ScaleCgbBus(sample, REG_NR50 & 7);
        }
        left += ApplyCgbOutputRatio(cgbLeft);
        right += ApplyCgbOutputRatio(cgbRight);

        // The hardware compositor and bias have a signed 10-bit range. In this
        // x256 bus scale, division by four maps it onto signed 16-bit output.
        sOutput[frame * 2] = ClipSample(left / PC_AUDIO_MIX_TO_S16_DIVISOR);
        sOutput[frame * 2 + 1] = ClipSample(right / PC_AUDIO_MIX_TO_S16_DIVISOR);
    }
    PcPlatformQueueAudio(sOutput, frameCount);
}

void SoundMain(void)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;
    struct MusicPlayerInfo *mplayInfo;
    u8 startingCgbChannels;
    u8 i;

    if (soundInfo == NULL || soundInfo->ident != ID_NUMBER)
        return;
    soundInfo->ident++;

    for (mplayInfo = soundInfo->musicPlayerHead;
         mplayInfo != NULL;
         mplayInfo = mplayInfo->musicPlayerNext)
        MPlayMain(mplayInfo);

    startingCgbChannels = ResetStartingCgbChannels(soundInfo);
    if (soundInfo->CgbSound != NULL)
        soundInfo->CgbSound();
    SyncCgbOscillatorState(soundInfo, startingCgbChannels);
    for (i = 0; i < soundInfo->maxChans; i++)
        UpdateDirectEnvelope(soundInfo, &soundInfo->chans[i]);
    MixFrame(soundInfo);

    soundInfo->ident = ID_NUMBER;
}

void m4aSoundVSync(void)
{
    struct SoundInfo *soundInfo = SOUND_INFO_PTR;

    if (soundInfo == NULL || soundInfo->ident < ID_NUMBER || soundInfo->ident > ID_NUMBER + 1)
        return;
    if (soundInfo->pcmDmaCounter == 0)
        soundInfo->pcmDmaCounter = soundInfo->pcmDmaPeriod;
    else
        soundInfo->pcmDmaCounter--;
}
