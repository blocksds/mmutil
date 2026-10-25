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

// Samples are always loaded as signed 16-bit!
void *Sample_AllocateData(u32 sample_length);
void Sample_ReadData_WAV(void *data, u32 sample_length, u8 bit_depth, u16 num_channels);
void Sample_ReadData_MOD(void *data, u32 sample_length, u8 format, u16 it_compression);
void Sample_UnpackDeltaCoding(void *data, u32 sample_length);

// This function will convert from s16 to the correct output format as needed
void FixSample(Sample *samp, InputTypes input_file_format);

#endif // SAMPLES_H__
