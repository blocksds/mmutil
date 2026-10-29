// SPDX-License-Identifier: ISC
//
// Copyright (c) 2008 Mukunda Johnson (mukunda@maxmod.org)
// Copyright (c) 2026 Antonio Niño Díaz

/****************************************************************************
 *                ____ ___  ____ __  ______ ___  ____  ____/ /              *
 *               / __ `__ \/ __ `/ |/ / __ `__ \/ __ \/ __  /               *
 *              / / / / / / /_/ />  </ / / / / / /_/ / /_/ /                *
 *             /_/ /_/ /_/\__,_/_/|_/_/ /_/ /_/\____/\__,_/                 *
 *                                                                          *
 ****************************************************************************/

// Will unroll upto 1kb more of data (when fixing NDS samples)
#define MAX_UNROLL_THRESHOLD    1024

#define GBA_MIN_LOOP_SIZE       512

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "defs.h"
#include "mas.h"
#include "errors.h"
#include "log.h"
#include "systems.h"
#include "adpcm.h"
#include "files.h"
#include "samples.h"
#include "simple.h"

extern int ignore_sflags;

u8     Sample_srcMethod = SAMPLESRC_CUBIC;
u8     Sample_srcOrder  = 4;
u32    Sample_minFreq   = 1;
u32    Sample_maxFreq   = ~0u;
double Sample_osRatio   = 1.0;

// This is an internal function to apply sample-rate conversion
void Sample_ProcessSRC(tSample *samp, u32 oldrate, u32 newrate, u8 method, u8 order, double Fc);

// This function ensures that newrate will fit in u32 by
// shifting down as needed, and returns newrate as u32.
// This can theoretically result in a non-integer loop
// length, but only if the resampling ratio is extreme.
static u32 ClipResampleRatio(u32 *oldrate, double newrate_f)
{
    u32 carry = 0;
    while (newrate_f > 0xFFFFFFFFu)
    {
        carry = *oldrate & 1;
        *oldrate  /= 2;
        newrate_f *= 0.5;
    }
    *oldrate += carry;
    return (u32)llrint(newrate_f);
}

static void Sample_PadEnd(Sample *samp, u32 count)
{
    // Pad end of sample with loop data (or zero) and shift loops forward
    // Note that this function forcibly sets the loop points such that:
    //  loop_start = new_length - loop_len
    //  loop_end   = new_length
    // This relies on sample_length being pre-sanitized; see the notes in
    // FixSample() below for exact details.

    if (count == 0)
        return; // nothing to do

    u32 lpoint = samp->loop_start;
    u32 length = samp->sample_length;
    samp->data = realloc(samp->data, Sample_GetPaddedLength(length + count) * 2);

    s16 *sdata = Sample_GetFirstSample(samp->data);
    if (samp->loop_type)
    {
        for (u32 x = 0; x < count; x++)
            sdata[length + x] = sdata[lpoint + x];
    }
    else
    {
        for (u32 x = 0; x < count; x++)
            sdata[length + x] = 0;
    }

    samp->sample_length = length + count;
    if (samp->loop_type)
    {
        samp->loop_start = samp->sample_length - (samp->loop_end - samp->loop_start);
        samp->loop_end   = samp->sample_length;
    }
}

static void Unroll_Sample_Loop(Sample *samp, u32 count)
{
    u32 addition = count*(samp->loop_end - samp->loop_start);
    Sample_PadEnd(samp, addition);
    samp->loop_start -= addition;
}

static void Unroll_BIDI_Sample(Sample* samp)
{
    // sample MUST have loop type 2 (BIDI)

    u32 lpoint = samp->loop_start;
    u32 length = samp->loop_end;
    u32 looplen = length - lpoint;

    samp->data = realloc(samp->data, Sample_GetPaddedLength(length + looplen) * 2);

    s16 *sdata = Sample_GetFirstSample(samp->data);
    for (u32 x = 0; x < looplen; x++)
        sdata[length + x] = sdata[length-1 - x];

    samp->loop_type = 1;
    samp->sample_length = samp->loop_end = length + looplen;
}

static void ApplyResampling(Sample *samp, u32 loopalign)
{
    if (samp->frequency < Sample_minFreq || samp->frequency > Sample_maxFreq || Sample_osRatio != 1.0)
    {
        u32 oldrate;
        u32 newrate;
        u32 targetFreq = samp->frequency;
        if (targetFreq < Sample_minFreq)
            targetFreq = Sample_minFreq;
        if (targetFreq > Sample_maxFreq)
            targetFreq = Sample_maxFreq;
        if (samp->loop_type)
        {
            oldrate = samp->loop_end - samp->loop_start;
            double newrate_f = Sample_osRatio * oldrate*targetFreq/samp->frequency;
                   newrate_f = floor(newrate_f / loopalign) * loopalign;
            if (newrate_f < 2)
            {
                // We need at least 2 samples for a loop!
                newrate_f = 2;
            }
            if (newrate_f < loopalign)
            {
                // ... or at least 1 block, depending on the format
                newrate_f = loopalign;
            }
            newrate = ClipResampleRatio(&oldrate, newrate_f);
        }
        else
        {
            oldrate = samp->frequency;
            newrate = ClipResampleRatio(&oldrate, Sample_osRatio * targetFreq);
        }
        if (oldrate != newrate)
        {
            // If we are oversampling, ensure that we place the cutoff
            // at the Nyquist of the non-oversampled source!
            double Fc = 1.0;
            if (newrate < oldrate*Sample_osRatio)
                Fc = newrate / (oldrate*Sample_osRatio);
            Sample_ProcessSRC(
                samp,
                oldrate,
                newrate,
                Sample_srcMethod,
                Sample_srcOrder,
                Fc
            );
        }
    }
}

static void FixSample_GBA(Sample *samp)
{
    // unroll BIDI loop
    if (samp->loop_type == 2)
        Unroll_BIDI_Sample(samp);

    // Apply maximum sampling rate, oversampling
    ApplyResampling(samp, 1);

    if (samp->loop_type)
    {
        int loop_length = samp->loop_end - samp->loop_start;

        if (loop_length < GBA_MIN_LOOP_SIZE)
        {
            Unroll_Sample_Loop(samp, (GBA_MIN_LOOP_SIZE / (samp->loop_end - samp->loop_start)) + 1);
        }
    }
}

static void FixSample_NDS(Sample *samp)
{
    // unroll BIDI loop
    if (samp->loop_type == 2)
        Unroll_BIDI_Sample(samp);

    // %o option.
    //
    // NDS hardware mixer mode only. Used to support the `Sample Offset` effects
    // when using the DS hardware channels for playback. You can use a sample
    // offset command to start playing beyond the loop start, and then when the
    // sample hits the loop end it'll still correctly loop back to the start.
    if (samp->loop_type)
    {
        if (!ignore_sflags)
        {
            if (strstr(samp->name, "%o") != NULL)
            {
                Unroll_Sample_Loop(samp, 1);
                samp->loop_start += (samp->loop_end-samp->loop_start) / 2;
            }
        }
    }

    // %c option
    //
    // NDS hardware mixer mode only. Compress the sample using IMA-ADPCM.
    if (!ignore_sflags)
    {
        if (strstr(samp->name, "%c") != NULL)
        {
            samp->format |= SAMPF_COMP;
        }
    }

    u32 alignmask = (samp->format & SAMPF_COMP) ? 7 : (samp->format & SAMPF_16BIT) ? 1 : 3;

    // Resize loop
    if (
        samp->loop_type &&
        samp->frequency >= Sample_minFreq &&
        samp->frequency <= Sample_maxFreq &&
        Sample_osRatio == 1.0
    )
    {
        u32 count = 0;
        u32 looplen = samp->loop_end-samp->loop_start;
        u32 unrolledlen = looplen;
        while (unrolledlen & alignmask)
        {
            count++;
            unrolledlen += looplen;
        }

        u32 addition = unrolledlen - looplen;
        if (addition > MAX_UNROLL_THRESHOLD)
        {
            Sample_ProcessSRC(
                samp,
                looplen,
                looplen + ((-looplen) & alignmask),
                Sample_srcMethod,
                Sample_srcOrder,
                1.0
            );
        }
        else if (count > 0)
        {
            Unroll_Sample_Loop(samp, count);
        }
    }
    else
    {
        ApplyResampling(samp, alignmask + 1);
    }

    // Align loop_start (and thus loop_end), or pad sample_length if loop is disabled
    // Note that for ADPCM, we want to align such that loop_end (or sample_length) is
    // of the form 8*N+1; this is because the first sample is stored directly, and is
    // followed by blocks of 8 samples per word.
    u32 n_pad;
    if (samp->loop_type)
    {
        n_pad = (-samp->loop_start) & alignmask;
    }
    else
    {
        n_pad = (-samp->sample_length) & alignmask;
    }
    if (samp->format & SAMPF_COMP)
        n_pad += 1;
    Sample_PadEnd(samp, n_pad);
}

// Looping samples playback works differently in different file formats.
//
// - MOD: The sample is played from START to END. Then, it jumps to loop_start
//   until loop_end, and back to loop_start.
//
// - S3M/XM/IT: The sample is played from START to LOOP_END. Then, it jumps to
//   loop_start until loop_end, and back to loop_start.
void Sample_Process(Sample *samp, InputTypes input_file_format)
{
    if (samp->sample_length == 0)
    {
        // sample has no data
        samp->loop_end = 0;
        samp->loop_start = 0;
        return;
    }

    if (samp->loop_type)
    {
        // Clamp loop_start and loop_end (f.e. FR_TOWER.MOD)
        if (samp->loop_start > samp->sample_length)
            samp->loop_start = samp->sample_length;

        if (samp->loop_end > samp->sample_length)
            samp->loop_end = samp->sample_length;

        int loop_length = samp->loop_end - samp->loop_start;

        // if loop exists and is empty, there is no actual loop
        if (loop_length <= 0)
            samp->loop_type = 0;
    }

    // When the loop start of a MOD sample is zero, looping samples are always
    // played until the end first. When the loop start isn't zero we can delete
    // any data after the loop:
    //
    //                  S   E
    // - Before   |.....LLLLL.....|
    //
    //                  S    E
    // - After    |.....LLLLL|
    //
    //            S        E
    // - Before   |LLLLLLLLL.....|
    //
    //                           S        E
    // - After    |LLLLLLLLL.....LLLLLLLLL|
    //
    //
    // S3M/XM/IT files start looping right after reaching the loop end,
    // regardless of the loop start, so we can do this:
    //
    //                 S   E
    // - Before  |.....LLLLL.....|
    //
    //                 S   E
    // - After   |.....LLLLL|
    //
    //            S        E
    // - Before   |LLLLLLLLL.....|
    //
    //            S         E
    // - After    |LLLLLLLLL|
    //
    // The result is that all samples can be played back with the same mixer
    // code instead of needing additional checks.

    if ((samp->loop_type != 0) && (samp->loop_end != samp->sample_length))
    {
        if (input_file_format == INPUT_TYPE_MOD)
        {
            if (samp->loop_start == 0)
            {
                VERBOSE("Appending loop after sample to fix MOD loop playback.\n");
                Sample_PadEnd(samp, samp->loop_end - samp->loop_start);
            }
            else
            {
                // Delete data after loop_end
                VERBOSE("Trimming unused sample end after loop end to save memory.\n");
                samp->sample_length = samp->loop_end;
            }
        }
        else
        {
            // Delete data after loop_end
            VERBOSE("Trimming unused sample end after loop end to save memory.\n");
            samp->sample_length = samp->loop_end;
        }
    }

    if (target_system == SYSTEM_GBA)
    {
        FixSample_GBA(samp);
        samp->format &= ~(SAMPF_16BIT | SAMPF_SIGNED | SAMPF_COMP);
    }
    else if (target_system == SYSTEM_NDS)
    {
        FixSample_NDS(samp);
        samp->format |= SAMPF_SIGNED;
    }
    else
    {
        ApplyResampling(samp, 1);
    }

    // Apply sign convention, bit depth, and remove pre-padding
    // On NDS, we clip to -127 (or -32767) because the software
    // mixer can have interpolation errors that cause overflows
    s16 min_value = (target_system == SYSTEM_NDS) ? -32767 : -32768;
    if (samp->format & (SAMPF_16BIT|SAMPF_COMP))
    {
        s16 sign_flip = (samp->format & (SAMPF_SIGNED|SAMPF_COMP)) ? 0 : (s16)0x8000;
        s16 *dst = (s16*)samp->data;
        const s16 *src = Sample_GetFirstSample(samp->data);
        for(u32 x = 0; x < samp->sample_length; x++)
        {
            dst[x] = ((src[x] >= min_value) ? src[x] : min_value) ^ sign_flip;
        }
    }
    else
    {
        s8 sign_flip = (samp->format & SAMPF_SIGNED) ? 0 : (s8)0x80;
        s8 *dst = (s8*)samp->data;
        const s16 *src = Sample_GetFirstSample(samp->data);
        for(u32 x = 0; x < samp->sample_length; x++)
        {
            dst[x] = (s8)(((src[x] >= min_value) ? src[x] : min_value) >> 8) ^ sign_flip;
        }
    }

    // Apply ADPCM compression if required
    if (samp->format & SAMPF_COMP)
        adpcm_compress_sample(samp);
}
