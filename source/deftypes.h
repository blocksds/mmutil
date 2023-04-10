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

#ifndef DEFTYPES_H
#define DEFTYPES_H

#ifdef HOST_16_BITS


typedef unsigned int u16;
typedef unsigned long u32;
typedef signed int s16;
typedef signed long s32;

#else

typedef unsigned short u16;
typedef unsigned int u32;
typedef signed short s16;
typedef signed long s32;

#endif

typedef unsigned char u8;
typedef signed char s8;

typedef unsigned char bool;
#define true (!0)
#define false (0)

#endif
