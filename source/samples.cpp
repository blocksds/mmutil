// SPDX-License-Identifier: ISC
//
// Copyright (c) 2008 Mukunda Johnson (mukunda@maxmod.org)
// Copyright (c) 2026 Antonio Niño Díaz

/****************************************************************************
 *                ____ ___  ____ __  ______ ___  ____  ____/ /              *
 *               / __ `__ \/ __ `/ |/ / __ `__ \/ __ \/ __  /               *
 *              / / / / / / /_/ />  </ / / / / / /_/ / /_/ /                *
 *             /_/ /_/ /_/\__,_/_/|_/_/ /_/ /_/\____/\__,_/                 *
 *                                                                          *
 ****************************************************************************/

#include <stdlib.h>

#include "defs.h"
#include "mas.h"
#include "files.h"
#include "samples.h"
#include "simple.h"

void *Sample_AllocateData(u32 sample_length)
{
    s16 *sdata = (s16*)malloc(sample_length * 2);
    return sdata;
}

void Sample_ReadData_WAV(void *data, u32 sample_length, u8 bit_depth, u16 num_channels)
{
    s16 *sdata = (s16*)data;
    if (bit_depth == 8)
    {
        // 8-bit is always unsigned
        for (u32 x = 0; x < sample_length; x++)
        {
            s32 v = 0;
            for (u16 c = 0; c < num_channels; c++)
            {
                v += read8();
            }
            v /= num_channels;
            sdata[x] = ((s16)v - 0x80) << 8;
        }
    }
    else
    {
        // 16-bit is always signed
        for (u32 x = 0; x < sample_length; x++)
        {
            s32 v = 0;
            for (u16 c = 0; c < num_channels; c++)
            {
                v += (s16)read16();
            }
            v /= num_channels;
            sdata[x] = (s16)v;
        }
    }
}

void Sample_ReadData_MOD(void *data, u32 sample_length, u8 format, u16 it_compression)
{
    s16 *sdata = (s16*)data;
    if(it_compression == 0)
    {
        switch (format)
        {
            case SAMP_FORMAT_U8:
                for (u32 x = 0; x < sample_length; x++)
                {
                    sdata[x] = ((s16)read8() - 0x80) << 8;
                }
                break;
            case SAMP_FORMAT_S8:
                for (u32 x = 0; x < sample_length; x++)
                {
                    sdata[x] = (s16)read8() << 8;
                }
                break;
            case SAMP_FORMAT_U16:
                for (u32 x = 0; x < sample_length; x++)
                {
                    sdata[x] = (s16)read16() - 0x8000;
                }
                break;
            case SAMP_FORMAT_S16:
                for (u32 x = 0; x < sample_length; x++)
                {
                    sdata[x] = (s16)read16();
                }
                break;
        }
        return;
    }

    // If we reach here, we have an IT compressed format
    // Since we don't global-abort on errors, initialize with silence
    for (u32 x = 0; x < sample_length; x++)
        sdata[x] = 0;

    /*
    NOTICE * NOTICE * NOTICE * NOTICE * NOTICE * NOTICE * NOTICE * NOTICE * NOTICE

    The following sample decompression code is based on CHIBITRACKER's code
    (http://chibitracker.berlios.de) which is based on xmp's code
    (http://xmp.helllabs.org) which is based in openCP code.

    NOTICE * NOTICE * NOTICE * NOTICE * NOTICE * NOTICE * NOTICE * NOTICE * NOTICE
    */

    u32 nbits;
    u32 dsize;
    u16 bsize; // maximum block size in samples
    u16 range; // FFh for 8-bit, FFFFh for 16-bit
    if (format & SAMPF_16BIT)
        nbits = 16, dsize = 4, bsize = 0x4000, range = 0xFFFF;
    else
        nbits = 8, dsize = 3, bsize = 0x8000, range = 0xFF;

    // Now unpack data til the dest buffer is full
    for (u32 x = 0; x < sample_length; )
    {
        // read a new block of compressed data and reset variables
        u8 *c_buffer;
        {
            u32 size = read16();
            c_buffer = (u8*)malloc(size + 4);
            if (!c_buffer) return;

            for (u32 i = 0; i < size; i++)
            {
                c_buffer[i] = read8();
            }
            for (u32 i = 0; i < 4; i++)
            {
                c_buffer[size+i] = 0;
            }
        }

        u32 block_length = bsize; // length of compressed data block in samples
        if((x + block_length) > sample_length)
            block_length = sample_length - x;

        // actual "bit width"
        u8 bit_width = nbits + 1; // start with width of 9 bits

        // integrator buffers (d2 for it2.15). Reset them.
        s16 d1 = 0, d2 = 0;

        // now uncompress the data block
        u32 bit_readpos = 0;
        for (u32 i = 0; i < block_length; )
        {
            u32 aux_value = readbits(c_buffer, bit_readpos, bit_width); // read bits
            bit_readpos += bit_width;

            if (bit_width < 7) // method 1 (1-6 bits)
            {
                if (aux_value == (1u << (bit_width - 1))) // check for "100..."
                {
                    //read_n_bits_from_IT_compressed_block(3) + 1; // yes -> read new width;
                    aux_value = readbits(c_buffer, bit_readpos, dsize) + 1;
                    bit_readpos += dsize;
                    bit_width = (aux_value < bit_width) ? aux_value : aux_value + 1;
                    // and expand it
                    continue; // ... next value
                }
            }
            else if (bit_width < nbits + 1) // method 2 (7-8 bits)
            {
                u16 border = (range >> ((nbits + 1) - bit_width)) - (nbits / 2);
                // lower border for width chg

                if (aux_value > border && aux_value <= (border + nbits))
                {
                    aux_value -= border; // convert width to 1-8
                    bit_width = (aux_value < bit_width) ? aux_value : aux_value + 1;
                    // and expand it
                    continue; // ... next value
                }
            }
            else if (bit_width == nbits + 1) // method 3 (9 bits)
            {
                if (aux_value & (1u << nbits)) // bit 8 set?
                {
                    bit_width = (u8)(aux_value + 1); // new width...
                    continue;                        // ... and next value
                }
            }
            else // illegal width, abort
            {
                free(c_buffer);
                return;
            }

            // now expand value to signed byte
            s16 v = (s16)aux_value;
            if (bit_width < nbits)
            {
                s16 sign_bit = (1 << (bit_width - 1));
                v = (v ^ sign_bit) - sign_bit;
            }

            // integrate upon the sample values
            d1 += v << (16 - nbits);
            d2 += d1;

            // ... and store it into the buffer
            v = (it_compression == 0x0215) ? d2 : d1;
            sdata[x++] = v;
            i++;
        }

        free(c_buffer);
    }
}

void Sample_UnpackDeltaCoding(void *data, u32 sample_length)
{
    s16 old = 0;
    s16 *sdata = (s16*)data;
    for (u32 x = 0; x < sample_length; x++)
    {
        old += sdata[x];
        sdata[x] = old;
    }
}
