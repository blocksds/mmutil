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

#include "defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

u32 time_start;

void kiwi_start( void )
{
	int r;
	time_start = time(NULL);
	srand( time_start );
	r=0;

	rand();rand();rand();rand();rand();rand();rand();rand();rand();rand();rand();rand();rand();rand();

	switch( r )
	{
	case 0:
		printf( "Your lucky number today is %i!\n", rand() );
		break;
	}

}
