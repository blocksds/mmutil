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

static FILE *fin;
static FILE *fout;

static int file_byte_count;

bool file_exists(char *filename)
{
    fin = fopen(filename, "rb");
    if (!fin)
        return false;

    file_close_read();
    return true;
}

int file_size(char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
    {
        printf("Can't check size of file: %s\n", filename);
        exit(EXIT_FAILURE);
    }

    fseek(f, 0, SEEK_END);
    int a = ftell(f);
    fclose(f);
    return a;
}

int file_open_read(char *filename)
{
    fin = fopen(filename, "rb");
    if (!fin)
    {
        printf("Can't open file for reading: %s\n", filename);
        exit(EXIT_FAILURE);
    }

    return FILE_OPEN_OKAY;
}

int file_open_write(char *filename)
{
    fout = fopen(filename, "wb");
    if (!fout)
    {
        printf("Can't open file for writing: %s\n", filename);
        exit(EXIT_FAILURE);
    }

    return FILE_OPEN_OKAY;
}

int file_open_write_end(char *filename)
{
    fout = fopen(filename, "r+b");
    if (!fout)
    {
        printf("Can't open file for appending: %s\n", filename);
        exit(EXIT_FAILURE);
    }

    fseek(fout, 0, SEEK_END);
    return FILE_OPEN_OKAY;
}

void file_close_read(void)
{
    fclose(fin);
}

void file_close_write(void)
{
    fclose(fout);
}

int file_seek_read(int offset, int mode)
{
    return fseek(fin, offset, mode);
}

int file_seek_write(int offset, int mode)
{
    return fseek(fout, offset, mode);
}

int file_tell_read(void)
{
    return ftell(fin);
}

int file_tell_write(void)
{
    return ftell(fout);
}

int file_tell_size(void)
{
    int pos = ftell(fin);
    fseek(fin, 0, SEEK_END);
    int size = ftell(fin);
    fseek(fin, pos, SEEK_SET);
    return size;
}

u8 read8(void)
{
    u8 a;
    if (fread(&a, 1, 1, fin) != 1)
    {
        printf("Unable to read input file\n");
        exit(EXIT_FAILURE);
    }
    return a;
}

u16 read16(void)
{
    u16 a;
    a  = read8();
    a |= ((u16)read8()) << 8;
    return a;
}

u32 read24(void)
{
    u32 a;
    a  = read8();
    a |= ((u32)read8()) << 8;
    a |= ((u32)read8()) << 16;
    return a;
}

u32 read32(void)
{
    u32 a;
    a  = read16();
    a |= ((u32)read16()) << 16;
    return a;
}

u8 read8f(FILE *p_fin)
{
    u8 a;
    if (fread(&a, 1, 1, p_fin) != 1)
    {
        printf("Unable to read file\n");
        exit(EXIT_FAILURE);
    }
    return a;
}

u16 read16f(FILE *p_fin)
{
    u16 a;
    a  = read8f(p_fin);
    a |= ((u16)read8f(p_fin)) << 8;
    return a;
}

u32 read32f(FILE *p_fin)
{
    u32 a;
    a  = read16f(p_fin);
    a |= ((u32)read16f(p_fin)) << 16;
    return a;
}

void write8(u8 p_v)
{
    fwrite(&p_v, 1, 1, fout);
    file_byte_count++;
}

void write16(u16 p_v)
{
    write8(p_v & 0xFF);
    write8(p_v >> 8);
    file_byte_count += 2;
}

void write24( u32 p_v )
{
    write8(p_v & 0xFF);
    write8((p_v >> 8) & 0xFF);
    write8((p_v >> 16) & 0xFF);
    file_byte_count += 3;
}

void write32(u32 p_v)
{
    write16(p_v & 0xFFFF);
    write16(p_v >> 16);
    file_byte_count += 4;
}

void align16(void)
{
    if (ftell(fout) & 1)
        write8(BYTESMASHER);
}

void align32(void)
{
    if (ftell(fout) & 3)
        write8(BYTESMASHER);
    if (ftell(fout) & 3)
        write8(BYTESMASHER);
    if (ftell(fout) & 3)
        write8(BYTESMASHER);
}

void align32f(FILE *p_file)
{
    if (ftell(p_file) & 3)
        write8(BYTESMASHER);
    if (ftell(p_file) & 3)
        write8(BYTESMASHER);
    if (ftell(p_file) & 3)
        write8(BYTESMASHER);
}

void skip8(u32 count)
{
    fseek(fin, count, SEEK_CUR);
}

void skip8f(u32 count, FILE *p_file)
{
    fseek(p_file, count, SEEK_CUR);
}

void file_delete(char *filename)
{
    if (file_exists(filename))
        remove(filename);
}

int file_get_byte_count(void)
{
    int a = file_byte_count;
    file_byte_count = 0;
    return a;
}
