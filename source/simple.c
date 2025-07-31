// SPDX-License-Identifier: ISC
//
// Copyright (c) 2008, Mukunda Johnson (mukunda@maxmod.org)

/****************************************************************************
 *                ____ ___  ____ __  ______ ___  ____  ____/ /              *
 *               / __ `__ \/ __ `/ |/ / __ `__ \/ __ \/ __  /               *
 *              / / / / / / /_/ />  </ / / / / / /_/ / /_/ /                *
 *             /_/ /_/ /_/\__,_/_/|_/_/ /_/ /_/\____/\__,_/                 *
 *                                                                          *
 ****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "defs.h"
#include "mas.h"
#include "simple.h"
#include "files.h"
#include "samplefix.h"

// Credits to Coda, look at this awesome codes
u32 readbits(u8 *buffer, unsigned int pos, unsigned int size)
{
    u32 result = 0;

    for (u32 i = 0; i < size; i++)
    {
        u32 byte_pos;
        u32 bit_pos;
        byte_pos = (pos + i) >> 3;
        bit_pos = (pos + i) & 7;
        result |= ((buffer[byte_pos] >> bit_pos) & 1) << i;
    }

    return result;
}

int get_ext(char *filename)
{
    int strl = strlen(filename);

    if (strl < 4)
        return INPUT_TYPE_UNK;

    u32 a = 0;

    for (int x = 0; x < 4; x++)
    {
        if (filename[strl - x - 1] != '.')
            a |= tolower(filename[strl - x - 1]) << (x * 8);
        else
            break;
    }

    //a = tolower(filename[strl - 1]) | (tolower(filename[strl - 1]) << 8) |
    //    tolower(filename[strl - 2] << 16) | tolower(filename[strl - 3] << 24);

    switch (a)
    {
        case 'mod':
            return INPUT_TYPE_MOD;
        case 's3m':
            return INPUT_TYPE_S3M;
        case 'txt':
            return INPUT_TYPE_TXT;
        case 'wav':
            return INPUT_TYPE_WAV;
        case 'msl':
            return INPUT_TYPE_MSL;
        case 'xm':
            return INPUT_TYPE_XM;
        case 'it':
            return INPUT_TYPE_IT;
        case 'h':
            return INPUT_TYPE_H;
    }

    return INPUT_TYPE_UNK;
}

u32 calc_samplen_ex2(Sample *s)
{
    if (s->loop_type == 0)
        return s->sample_length;
    else
        return s->loop_end;
}

#define MM_SREPEAT_FORWARD      1 // Forward loop
#define MM_SREPEAT_OFF          2 // No loop

u32 calc_samplooplen(Sample *s)
{
    if (s->loop_type == MM_SREPEAT_FORWARD)
    {
        u32 a = s->loop_end - s->loop_start;
        return a;
    }
    else if (s->loop_type == MM_SREPEAT_OFF)
    {
        u32 a = (s->loop_end - s->loop_start) * 2;
        return a;
    }
    else
    {
        return 0xFFFFFFFF;
    }
}

u32 calc_samplen(Sample *s)
{
    if (s->loop_type == MM_SREPEAT_FORWARD)
    {
        return s->loop_end;
    }
    else if (s->loop_type == MM_SREPEAT_OFF)
    {
        return (s->loop_end - s->loop_start) + s->loop_end;
    }
    else
    {
        return s->sample_length;
    }
}

#define MM_SFORMAT_8BIT         0 // 8 bit
#define MM_SFORMAT_16BIT        1 // 16 bit
#define MM_SFORMAT_ADPCM        2 // ADPCM (invalid)
#define MM_SFORMAT_ERROR        3 // Invalid

u8 sample_dsformat(Sample *samp)
{
    if (samp->format & SAMPF_COMP)
    {
        return MM_SFORMAT_ADPCM;
    }
    else
    {
        if (samp->format & SAMPF_SIGNED)
        {
            if (samp->format & SAMPF_16BIT)
                return MM_SFORMAT_16BIT;
            else
                return MM_SFORMAT_8BIT;
        }
        else
        {
            if (!(samp->format & SAMPF_16BIT))
                return MM_SFORMAT_ERROR;
            else
                return MM_SFORMAT_ERROR; // error
        }
    }
}

u8 sample_dsreptype(Sample *samp)
{
    if (samp->loop_type)
        return MM_SREPEAT_FORWARD;
    else
        return MM_SREPEAT_OFF;
}

int clamp_s8(int value)
{
    if (value < -128)
        value = -128;
    else if (value > 127)
        value = 127;
    return value;
}

int clamp_u8(int value)
{
    if (value < 0)
        value = 0;
    else if (value > 255)
        value = 255;
    return value;
}
