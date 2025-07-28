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

// Thanks GBATEK for the ima-adpcm specification

#include <stdlib.h>

#include "defs.h"
#include "deftypes.h"
#include "mas.h"

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

static int read_sample( Sample* sample, int position )
{
    int s;

    if (sample->format & SAMPF_16BIT)
    {
        s = ((s16 *)sample->data)[position];
    }
    else
    {
        // Expand 8-bit value
        int a = ((s8 *)sample->data)[position];
        s = (a << 8);
    }

    if (s < -32767) // Is this necessary?...
        s = -32767;

    return s;
}

static int minmax( int value, int low, int high )
{
    if (value < low)
        value = low;
    else if (value > high)
        value = high;
    return value;
}

static int calc_delta( int diff, int step )
{
    int delta = step >> 3;      // t / 8

    if (diff >= step)           // t / 1
    {
        diff -= step;
        delta += step;
    }

    if (diff >= (step >> 1))    // t / 2
    {
        diff -= step >> 1;
        delta += step >> 1;
    }

    if (diff >= (step >> 2))    // t / 4
    {
        diff -= step >> 2;
        delta += step >> 2;
    }

    return delta;
}

// Compresses a sample with IMA-ADPCM.
// Make sure the data has proper alignment/length!
void adpcm_compress_sample(Sample *sample)
{
    // Allocate space for sample (compressed size)
    u8 *output = malloc(sample->sample_length / 2 + 4);

    // Determine best (or close to best) initial table value

    int prev_value = read_sample(sample, 0);
    int index = 0;

    {
        int smallest_error = 9999999;

        int diff = read_sample(sample, 1) - read_sample(sample, 0);

        for (int i = 0; i < 88; i++)
        {
            int tmp_error = calc_delta(diff, i) - diff;
            if (tmp_error < smallest_error)
            {
                smallest_error = tmp_error;
                index = i;
            }
        }
    }

    // Set data header

    (*((u32 *)output)) = prev_value    // Initial PCM16 value
                      | (index << 16); // Initial table index value

    int step = AdpcmTable[index];

    for (u32 x = 0; x < sample->sample_length; x++)
    {
        int data;

        int curr_value = read_sample(sample, x);

        int diff = curr_value - prev_value;
        if (diff < 0)
        {
            // Negate difference & set negative bit
            diff = -diff;
            data = 8;
        }
        else
        {
            // Clear negative flag
            data = 0;
        }

        /*
          difference calculation:
          Diff = AdpcmTable[Index]/8
          IF (data4bit AND 1) THEN Diff = Diff + AdpcmTable[Index]/4
          IF (data4bit AND 2) THEN Diff = Diff + AdpcmTable[Index]/2
          IF (data4bit AND 4) THEN Diff = Diff + AdpcmTable[Index]/1
        */

        int delta = step >> 3;      // t / 8 (always)

        if (diff >= step)           // t / 1
        {
            data |= 4;
            diff -= step;
            delta += step;
        }
        if (diff >= (step >> 1))    // t / 2
        {
            data |= 2;
            diff -= step >> 1;
            delta += step >> 1;
        }
        if (diff >= (step >> 2))    // t / 4
        {
            data |= 1;
            diff -= step >> 2;
            delta += step >> 2;
        }

        // Add/subtract delta
        prev_value += (data & 8) ? -delta : delta;

        // Clamp output
        prev_value = minmax(prev_value, -0x7FFF, 0x7FFF);

        // Add index table value (and clamp)
        index = minmax(index + IndexTable[data & 7], 0, 88);

        // Read new step value
        step = AdpcmTable[index];

        // Write output
        if (x & 1)
            output[(x >> 1) + 4] |= data << 4;
        else
            output[(x >> 1) + 4] = data;
    }

    // Delete old sample
    free(sample->data);

    // Assign new sample
    sample->data = output;

    // New format
    sample->format = SAMP_FORMAT_ADPCM;

    // New length/loop
    sample->sample_length = (sample->sample_length / 2) + 4;
    sample->loop_start /= 2;
    sample->loop_end /= 2;

    // Step loop past adpcm header
    sample->loop_start += 4;
    sample->loop_end += 4;
}
