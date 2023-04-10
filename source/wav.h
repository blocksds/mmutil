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

#ifndef WAV_H
#define WAV_H

#define LOADWAV_OK				0x00
#define LOADWAV_CORRUPT			0x01
#define LOADWAV_UNKNOWN_COMP	0x11
#define LOADWAV_TOOMANYCHANNELS	0x12
#define LOADWAV_UNSUPPORTED_BD	0x13
#define LOADWAV_BADDATA			0x14

int Load_WAV( Sample* samp, bool verbose, bool fix );

#endif
