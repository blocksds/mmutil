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

#ifndef SAMPLES_H__
#define SAMPLES_H__

#include "defs.h"

typedef enum
{
    SAMPLESRC_NEAREST = 0,
    SAMPLESRC_LINEAR  = 1,
    SAMPLESRC_CUBIC   = 2,
    SAMPLESRC_SINC    = 3,
} SampleSRCMethod;

extern u8     Sample_srcMethod;
extern u8     Sample_srcOrder;
extern u32    Sample_minFreq;
extern u32    Sample_maxFreq;
extern double Sample_osRatio;

// The pointer returned by AllocateData() contains N/2-1 samples
// of pre-padding and N samples post-padding for the resampler,
// and always stores the sample data as signed 16-bit.
// Sample_ReadData_WAV()/MOD() account for this already, but if
// the pointer will be accessed outside of those functions, these
// details must be kept in mind (see Sample_GetFirstSample()).
void *Sample_AllocateData(u32 sample_length);
void Sample_ReadData_WAV(void *data, u32 sample_length, u8 bit_depth, u16 num_channels);
void Sample_ReadData_MOD(void *data, u32 sample_length, u8 format, u16 it_compression);
void Sample_UnpackDeltaCoding(void *data, u32 sample_length);

// Sample_Process() applies sample-rate/format conversions and
// removes pre-padding of the sample data; the data to be used
// for output is directly accessible from samp->data.
struct tSample;
void Sample_Process(tSample *samp, InputTypes input_file_format);

// Calculate length to allocate for sample data after padding
// * N/2-1 samples of pre-padding
// * N/2 samples of post-padding
// * N/2 samples of additional post-padding to account for pre-ring
inline u32 Sample_GetPaddedLength(u32 sample_length)
{
    return sample_length + Sample_srcOrder-1 + Sample_srcOrder/2;
}

// This function returns a pointer to the first sample, after pre-padding.
// Do not use this function after using Sample_Process()!
inline s16 *Sample_GetFirstSample(void *data)
{
    s16 *sdata = (s16*)data;
    return sdata + (Sample_srcOrder-1)/2;
}

#endif // SAMPLES_H__
