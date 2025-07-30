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

#ifndef SIMPLE_H__
#define SIMPLE_H__

#define INPUT_TYPE_MOD  0
#define INPUT_TYPE_S3M  1
#define INPUT_TYPE_XM   2
#define INPUT_TYPE_IT   3
#define INPUT_TYPE_WAV  4
#define INPUT_TYPE_TXT  5
#define INPUT_TYPE_UNK  6
#define INPUT_TYPE_H    7
#define INPUT_TYPE_MSL  8

int get_ext(char *filename);
u32 calc_samplooplen(Sample *s);
u32 calc_samplen(Sample *s);
u32 calc_samplen_ex2(Sample *s);
int clamp_s8(int value);
int clamp_u8(int value);
u32 readbits(u8* buffer, unsigned int pos, unsigned int size);

u8 sample_dsformat(Sample *samp);
u8 sample_dsreptype(Sample *samp);

#endif // SIMPLE_H__
