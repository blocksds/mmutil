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

#ifndef FILES_H__
#define FILES_H__

#include <stdio.h>

int file_size(char *filename);
int file_open_read(char *filename);
int file_open_write(char *filename);
int file_open_write_end(char *filename);
void file_close_read(void);
void file_close_write(void);
u8 read8(void);
u16 read16(void);
u32 read24(void);
u32 read32(void);
void write8(u8 p_v);
void write16(u16 p_v);
void write24(u32 p_v);
void write32(u32 p_v);
void align16(void);
void align32(void);
void skip8(u32 count);
int file_seek_read(int offset, int mode);
int file_seek_write(int offset, int mode);
int file_tell_read(void);
int file_tell_write(void);

u8 read8f(FILE *p_fin);
u16 read16f(FILE *p_fin);
u32 read32f(FILE *p_fin);
void align32f(FILE *p_file);
void skip8f(u32 count, FILE *p_file);

void file_delete(char *filename);

bool file_exists(char *filename);

int file_get_byte_count(void);

int file_tell_size(void);

#define FILE_OPEN_OKAY      0
#define FILE_OPEN_ERROR    -1

#endif // FILES_H__
