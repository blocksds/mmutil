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
#include "defs.h"
#include "files.h"
#include "mas.h"

const u8 GBA_ROM[] = {
#embed "maxmod_demo.gba"
};

void Write_GBA(void)
{
    for (size_t x = 0; x < sizeof(GBA_ROM); x++)
        write8(GBA_ROM[x]);
}

