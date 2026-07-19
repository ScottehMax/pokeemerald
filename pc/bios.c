#include <math.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "global.h"
#include "pc_platform.h"

#undef CpuSet
#undef CpuFastSet

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void SoftReset(u32 resetFlags)
{
    (void)resetFlags;
    PcPlatformSoftReset();
}

void RegisterRamReset(u32 resetFlags)
{
    if (resetFlags & RESET_EWRAM)
        memset((void *)EWRAM_START, 0, EWRAM_END - EWRAM_START);
    if (resetFlags & RESET_IWRAM)
        memset((void *)IWRAM_START, 0, IWRAM_END - IWRAM_START);
    if (resetFlags & RESET_PALETTE)
        memset((void *)PLTT, 0, PLTT_SIZE);
    if (resetFlags & RESET_VRAM)
        memset((void *)VRAM, 0, VRAM_SIZE);
    if (resetFlags & RESET_OAM)
        memset((void *)OAM, 0, OAM_SIZE);
    if (resetFlags & RESET_REGS)
        memset((void *)REG_BASE, 0, 0x300);
}

void VBlankIntrWait(void)
{
#ifdef _WIN32
    Sleep(17);
#else
    struct timespec delay = {0, 16742706};
    nanosleep(&delay, NULL);
#endif
}

u16 Sqrt(u32 num)
{
    return (u16)sqrt((double)num);
}

u16 ArcTan2(s16 x, s16 y)
{
    double angle = atan2((double)y, (double)x);

    if (angle < 0)
        angle += 2.0 * M_PI;
    return (u16)(angle * (65536.0 / (2.0 * M_PI)));
}

void CpuSet(const void *src, void *dest, u32 control)
{
    u32 count = control & 0x1FFFFF;
    u32 unitSize = (control & CPU_SET_32BIT) ? 4 : 2;
    bool32 fixed = (control & CPU_SET_SRC_FIXED) != 0;
    u32 i;

    if (fixed)
    {
        if (unitSize == 4)
        {
            u32 value = *(const u32 *)src;
            for (i = 0; i < count; i++)
                ((u32 *)dest)[i] = value;
        }
        else
        {
            u16 value = *(const u16 *)src;
            for (i = 0; i < count; i++)
                ((u16 *)dest)[i] = value;
        }
    }
    else
    {
        memmove(dest, src, count * unitSize);
    }
}

void CpuFastSet(const void *src, void *dest, u32 control)
{
    u32 wordCount = control & 0x1FFFFF;
    u32 i;

    if (control & CPU_FAST_SET_SRC_FIXED)
    {
        u32 value = *(const u32 *)src;
        for (i = 0; i < wordCount; i++)
            ((u32 *)dest)[i] = value;
    }
    else
    {
        memmove(dest, src, wordCount * sizeof(u32));
    }
}

void BgAffineSet(struct BgAffineSrcData *src, struct BgAffineDstData *dest, s32 count)
{
    s32 i;

    for (i = 0; i < count; i++)
    {
        double angle = src[i].alpha * (2.0 * M_PI / 65536.0);
        s32 cosine = (s32)lround(cos(angle) * 256.0);
        s32 sine = (s32)lround(sin(angle) * 256.0);

        dest[i].pa = (s16)(cosine * src[i].sx / 256);
        dest[i].pb = (s16)(-sine * src[i].sx / 256);
        dest[i].pc = (s16)(sine * src[i].sy / 256);
        dest[i].pd = (s16)(cosine * src[i].sy / 256);
        dest[i].dx = src[i].texX - dest[i].pa * src[i].scrX - dest[i].pb * src[i].scrY;
        dest[i].dy = src[i].texY - dest[i].pc * src[i].scrX - dest[i].pd * src[i].scrY;
    }
}

void ObjAffineSet(struct ObjAffineSrcData *src, void *dest, s32 count, s32 offset)
{
    u8 *out = dest;
    s32 i;

    for (i = 0; i < count; i++)
    {
        double angle = src[i].rotation * (2.0 * M_PI / 65536.0);
        s32 cosine = (s32)lround(cos(angle) * 256.0);
        s32 sine = (s32)lround(sin(angle) * 256.0);
        s16 matrix[4];
        s32 j;

        matrix[0] = (s16)(cosine * src[i].xScale / 256);
        matrix[1] = (s16)(-sine * src[i].xScale / 256);
        matrix[2] = (s16)(sine * src[i].yScale / 256);
        matrix[3] = (s16)(cosine * src[i].yScale / 256);
        for (j = 0; j < 4; j++)
            *(s16 *)(out + (i * 4 + j) * offset) = matrix[j];
    }
}

static void Lz77UnComp(const u32 *source, void *destination)
{
    const u8 *src = (const u8 *)source;
    u8 *dest = destination;
    u32 outputSize = src[1] | (src[2] << 8) | (src[3] << 16);
    u32 outputPos = 0;

    src += 4;
    while (outputPos < outputSize)
    {
        u8 flags = *src++;
        s32 bit;

        for (bit = 7; bit >= 0 && outputPos < outputSize; bit--)
        {
            if (flags & (1 << bit))
            {
                u16 block = (u16)(src[0] << 8) | src[1];
                u32 length = (block >> 12) + 3;
                u32 distance = (block & 0xFFF) + 1;
                u32 i;

                src += 2;
                for (i = 0; i < length && outputPos < outputSize; i++)
                {
                    dest[outputPos] = outputPos >= distance ? dest[outputPos - distance] : 0;
                    outputPos++;
                }
            }
            else
            {
                dest[outputPos++] = *src++;
            }
        }
    }
}

void LZ77UnCompWram(const u32 *src, void *dest)
{
    Lz77UnComp(src, dest);
}

void LZ77UnCompVram(const u32 *src, void *dest)
{
    Lz77UnComp(src, dest);
}

static void RlUnComp(const u32 *source, void *destination)
{
    const u8 *src = (const u8 *)source;
    u8 *dest = destination;
    u32 outputSize = src[1] | (src[2] << 8) | (src[3] << 16);
    u32 outputPos = 0;

    src += 4;
    while (outputPos < outputSize)
    {
        u8 control = *src++;
        u32 length = (control & 0x7F) + (control & 0x80 ? 3 : 1);
        u32 i;

        if (control & 0x80)
        {
            u8 value = *src++;
            for (i = 0; i < length && outputPos < outputSize; i++)
                dest[outputPos++] = value;
        }
        else
        {
            for (i = 0; i < length && outputPos < outputSize; i++)
                dest[outputPos++] = *src++;
        }
    }
}

void RLUnCompWram(const u32 *src, void *dest)
{
    RlUnComp(src, dest);
}

void RLUnCompVram(const u32 *src, void *dest)
{
    RlUnComp(src, dest);
}

int MultiBoot(struct MultiBootParam *mp)
{
    (void)mp;
    return 1;
}

s32 Div(s32 num, s32 denom)
{
    return denom == 0 ? 0 : num / denom;
}
