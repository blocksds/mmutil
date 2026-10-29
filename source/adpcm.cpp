// SPDX-License-Identifier: ISC
//
// Copyright (c) 2008 Mukunda Johnson (mukunda@maxmod.org)

/****************************************************************************
 *                ____ ___  ____ __  ______ ___  ____  ____/ /              *
 *               / __ `__ \/ __ `/ |/ / __ `__ \/ __ \/ __  /               *
 *              / / / / / / /_/ />  </ / / / / / /_/ / /_/ /                *
 *             /_/ /_/ /_/\__,_/_/|_/_/ /_/ /_/\____/\__,_/                 *
 *                                                                          *
 ****************************************************************************/

// Thanks GBATEK for the ima-adpcm specification

#include <stdlib.h>

#include "defs.h"
#include "deftypes.h"
#include "mas.h"

// Number of samples to test for initial table index selection
#define INIT_LOOKAHEAD 7

const s8 IndexTable[8] = {
    -1, -1, -1, -1, 2, 4, 6, 8
};

const u16 AdpcmTable[89] = {
        7,     8,     9,    10,    11,    12,    13,    14,    16,    17,
       19,    21,    23,    25,    28,    31,    34,    37,    41,    45,
       50,    55,    60,    66,    73,    80,    88,    97,   107,   118,
      130,   143,   157,   173,   190,   209,   230,   253,   279,   307,
      337,   371,   408,   449,   494,   544,   598,   658,   724,   796,
      876,   963,  1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
     2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,
     5894,  6484,  7132,  7845,  8630,  9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static int minmax(int value, int low, int high)
{
    if (value < low)
        value = low;
    else if (value > high)
        value = high;
    return value;
}

static int quantize_delta(int diff, int step, u8 *code)
{
    /*
      difference calculation:
      Diff = AdpcmTable[Index]/8
      IF (Data4bit AND 1) THEN Diff = Diff + AdpcmTable[Index]/4
      IF (Data4bit AND 2) THEN Diff = Diff + AdpcmTable[Index]/2
      IF (Data4bit AND 4) THEN Diff = Diff + AdpcmTable[Index]/1
      IF (Data4bit AND 8)=0 THEN Pcm16bit = Max(Pcm16bit+Diff,+7FFFh)
      IF (Data4bit AND 8)=8 THEN Pcm16bit = Min(Pcm16bit-Diff,-7FFFh)
      Index = MinMax (Index+IndexTable[Data4bit AND 7],0,88)
    */

    if (diff < 0)
    {
        *code = 8;
        diff = -diff;
    }
    else
    {
        *code = 0;
    }

    int delta = step >> 3;      // t / 8
    if (diff >= step)           // t / 1
    {
        *code |= 4;
        diff -= step;
        delta += step;
    }

    if (diff >= (step >> 1))    // t / 2
    {
        *code |= 2;
        diff -= step >> 1;
        delta += step >> 1;
    }

    if (diff >= (step >> 2))    // t / 4
    {
        *code |= 1;
        diff -= step >> 2;
        delta += step >> 2;
    }

    return (*code & 8) ? -delta : delta;
}

// Compresses a sample with IMA-ADPCM.
// Make sure the data has proper alignment/length!
void adpcm_compress_sample(Sample *sample)
{
    // We will be storing back to the same buffer we read from,
    // so we must ensure we never overwrite samples we read!
    u8 *output = (u8*)sample->data;
    s16 *sdata = (s16*)sample->data;

    // As per GBATek, the first sample must not be -8000h!
    //   Max(+7FFFh) leaves -8000h unclipped (can happen if initial PCM16 was -8000h)
    if (sdata[0] == -0x8000)
        sdata[0] = -0x7FFF;

    // Determine best (or close to best) initial table value
    int index = 0;
    {
        u32 min_error = ~0u;
        for (int i = 0; i < 88; i++)
        {
            int idx  = i;
            int prev = sdata[0];
            u32 total_error = 0;
            for (u32 n = 1; n <= INIT_LOOKAHEAD; n++)
            {
                u8  code;
                int delta  = sdata[n] - prev;
                int qdelta = quantize_delta(delta, AdpcmTable[idx], &code);
                prev = minmax(prev + qdelta, -32767, +32767);
                idx  = minmax(idx + IndexTable[code & 7], 0, 88);

                int e = delta - qdelta;
                total_error += (u32)(e < 0 ? -e : e);
            }

            if (total_error < min_error)
            {
                index = i;
                min_error = total_error;
            }
        }
    }

    // Write header and begin output
    // We need to store 32 bits of "header" information, which
    // consists of the first sample, and the initial index.
    // The first sample can stay where it is, but the second
    // sample would be overwritten. So to work around this, we
    // just read the next sample before we store the previous.
    int prev_value = sdata[0];
    int next_value = sdata[1];
    output[2] = (u8)index;
    output[3] = 0;
    for (u32 x = 1; x < sample->sample_length; x++)
    {
        u8 code;
        int qdelta = quantize_delta(next_value - prev_value, AdpcmTable[index], &code);
        prev_value = minmax(prev_value + qdelta, -32767, +32767);
        index      = minmax(index + IndexTable[code & 7], 0, 88);
        if(x+1 < sample->sample_length) next_value = sdata[x+1];
        output[4 + (x-1)/2u] >>= 4;
        output[4 + (x-1)/2u]  |= code << 4;
    }

    // Update format information
    // Note that the rest of the mmutil code expects that an ADPCM
    // sample stores length information in bytes because... reasons.
    // Also note that we explicitly aligned ADPCM samples such that
    // their loop points (and length) are 8*N+1 to account for the
    // first sample being stored explicitly, but we need to store
    // the size (in bytes) of the ADPCM words, so the first sample
    // must be excluded in these calculations to be correct!
    // Finally, note the +1 word - this is to include the header.
    sample->format        = SAMP_FORMAT_ADPCM;
    sample->sample_length = ((sample->sample_length - 1) / 8 + 1) * 4;
    sample->loop_start    = ((sample->loop_start    - 1) / 8 + 1) * 4;
    sample->loop_end      = ((sample->loop_end      - 1) / 8 + 1) * 4;
}
