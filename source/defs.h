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

#ifndef DEFS_H__
#define DEFS_H__

#include "deftypes.h"

#define BYTESMASHER     0xBA
#define MAX_CHANNELS    32
extern int PANNING_SEP;

typedef enum
{
    INPUT_TYPE_MOD  = 0,
    INPUT_TYPE_S3M  = 1,
    INPUT_TYPE_XM   = 2,
    INPUT_TYPE_IT   = 3,
    INPUT_TYPE_WAV  = 4,
    INPUT_TYPE_TXT  = 5,
    INPUT_TYPE_UNK  = 6,
    INPUT_TYPE_H    = 7,
    INPUT_TYPE_MSL  = 8
}
InputTypes;

#endif // DEFS_H__
