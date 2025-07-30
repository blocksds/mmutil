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

// information from ST3's TECH.DOC

#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "mas.h"
#include "s3m.h"
#include "files.h"
#include "simple.h"
#include "errors.h"
#include "samplefix.h"

#define S3M_NOTE(a) (((a&15)+(a>>4)*12)+12)

#ifdef SUPER_ASCII
#define vstr_s3m_samp  " %5i │ %-4s│ %3i%% │%5ihz│ %-28s│\n"
#define vstr_s3m_sampe " ----- │ --- │ ---- │ ----- │ %-28s│\n"

#define vstr_s3m_div "────────────────────────────────────────────\n"

#define vstr_s3m_sampt_top      "┌─────┬───────┬─────┬──────┬───────┬─────────────────────────────┐\n"
#define vstr_s3m_sampt_mid      "│INDEX│LENGTH │LOOP │VOLUME│ MID-C │             NAME            │\n"
#define vstr_s3m_sampt_slice    "├─────┼───────┼─────┼──────┼───────┼─────────────────────────────┤\n"
#define vstr_s3m_sampt_index    "│ %2i  │"
#define vstr_s3m_sampt_bottom   "└─────┴───────┴─────┴──────┴───────┴─────────────────────────────┘\n"

#define vstr_s3m_pattern " \x0e %2i%s"
#else
#define vstr_s3m_samp  "%-5i   %-3s   %3i%%   %5ihz  %-28s \n"
#define vstr_s3m_sampe "-----   ---   ----   -------  %-28s\n"

#define vstr_s3m_div "--------------------------------------------\n"

#define vstr_s3m_sampt_top      vstr_s3m_div
#define vstr_s3m_sampt_mid      " INDEX LENGTH  LOOP  VOLUME  MID-C   NAME\n"
//#define vstr_s3m_sampt_slice    ""
#define vstr_s3m_sampt_index    " %-2i    "
#define vstr_s3m_sampt_bottom   vstr_s3m_div

#define vstr_s3m_pattern " * %2i%s"
#endif

int Load_S3M_SampleData(Sample *samp, u8 ffi)
{
    if (samp->sample_length == 0)
        return ERR_NONE;

    if (samp->format & SAMPF_16BIT)
        samp->data = malloc(samp->sample_length * 2);
    else
        samp->data = malloc(samp->sample_length);

    if (ffi == 1)
    {
        // signed samples [VERY OLD]
        for (u32 x = 0; x < samp->sample_length; x++)
        {
            if (samp->format & SAMPF_16BIT)
            {
                int a = read16();
                a += 32768;
                ((u16*)samp->data)[x] = (u16)a;
            }
            else
            {
                int a = read8();
                a += 128;
                ((u8*)samp->data)[x] = (u8)a;
            }
        }
    }
    else if (ffi == 2)
    {
        // unsigned samples
        for (u32 x = 0; x < samp->sample_length; x++)
        {
            if (samp->format & SAMPF_16BIT)
            {
                int a = read16();
                ((u16*)samp->data)[x] = (u16)a;
            }
            else
            {
                int a = read8();
                ((u8*)samp->data)[x] = (u8)a;
            }
        }
    }
    else
    {
        return ERR_UNKNOWNSAMPLE;
    }

    FixSample(samp);
    return ERR_NONE;
}

int Load_S3M_Sample(Sample *samp, bool verbose)
{
    memset(samp, 0, sizeof(Sample));
    samp->msl_index = 0xFFFF;

    if (read8() == 1) // type, 1 = sample
    {
        for (u32 x = 0; x < 12; x++)
            samp->filename[x] = read8();

        samp->datapointer = (read8() * 65536 + read16()) * 16; //read24();
        samp->sample_length = read32();
        samp->loop_start = read32();
        samp->loop_end = read32();
        samp->default_volume = read8();
        samp->global_volume = 64;
        read8(); // reserved

        if (read8() != 0) // packing, 0 = unpacked
            return ERR_UNKNOWNSAMPLE;

        u8 flags = read8();
        samp->loop_type = flags & 1 ? 1 : 0;
        if (flags & 2)
            return ERR_UNKNOWNSAMPLE;

        //samp->bit16 = flags & 4 ? true : false;
        samp->format = flags & 4 ? SAMP_FORMAT_U16 : SAMP_FORMAT_U8;
        samp->frequency = read32();
        read32(); // reserved
        skip8(8); // internal variables
        for (u32 x = 0;x < 28; x++)
            samp->name[x] = read8();

        if (read32() != 'SRCS')
            return ERR_UNKNOWNSAMPLE;

        if (verbose)
        {
            //printf("────────────────────────────────────────────\n");
            //printf("Loading Samples...\n");
            //printf("┌─────┬──────┬────┬──────┬─────┬─────────────────────────────┐\n");
            //printf("│LENGTH│LOOP│VOLUME│ MID-C │             NAME            │\n");
            //printf("┼──────┼────┼──────┼─────┼─────────────────────────────┤\n");
            printf(vstr_s3m_samp, samp->sample_length, samp->loop_type ? "Yes" : "No",
                   (samp->default_volume * 100) / 64, samp->frequency, samp->name);
            /*
            printf("  Name......%s\n", samp->name);
            printf("  Length....%i\n", samp->sample_length);
            if (samp->loop_type)
                printf("     Loop......%i->%i\n", samp->loop_start, samp->loop_end);
            else
                printf("  Loop......Disabled\n");
            printf("  Volume....%i\n", samp->default_volume);
            printf("  Middle C..%ihz\n", samp->frequency);
            if (samp->bit16)
                printf("  16 bit....yes\n");
            */
        }
    }
    else
    {
        if (verbose)
        {
            printf(vstr_s3m_sampe, samp->name);
        }
    }

    return ERR_NONE;
}

int Load_S3M_Pattern(Pattern *patt)
{
    int clength = read16();
    // unpack s3m data

    memset(patt, 0, sizeof(Pattern));

    patt->clength = clength;
    patt->nrows = 64;

    for (int row = 0; row < 64 * MAX_CHANNELS; row++)
    {
        patt->data[row].note = 250;
        patt->data[row].vol = 255;
    }

    for (int row = 0; row < 64; row++)
    {
        u8 what;

        while ((what = read8()) != 0) // BYTE:what / 0 = end of row
        {
            int col = what & 31; // & 31 = channel

            int z = row * MAX_CHANNELS + col;

            if (what & 32) // & 32 = follows;  BYTE:note, BYTE:instrument
            {
                patt->data[z].note = read8();

                if (patt->data[z].note == 255)
                    patt->data[z].note = 250;
                else if (patt->data[z].note == 254)
                    patt->data[z].note = 254;
                else
                    patt->data[z].note = S3M_NOTE(patt->data[z].note);

                patt->data[z].inst = read8();
            }

            if (what & 64) // & 64 = follows;  BYTE:volume
            {
                patt->data[z].vol = read8();
            }

            if (what & 128) // & 128 = follows; BYTE:command, BYTE:info
            {
                patt->data[z].fx = read8();
                patt->data[z].param = read8();
                if (patt->data[z].fx == 3) // convert pattern break to hexadecimal
                {
                    patt->data[z].param = (patt->data[z].param & 0xF)
                                        + (patt->data[z].param / 16) * 10;
                }
                if (patt->data[z].fx == 'X' - 64)
                {
                    patt->data[z].param *= 2; // multiply volume scale by 2
                }
                if (patt->data[z].fx == 'V' - 64)
                {
                    patt->data[z].param *= 2; // multiply volume scale by 2
                }
            }

            if (patt->data[z].fx == 255)
            {
                patt->data[z].fx = 0;
                patt->data[z].param = 0;
            }
        }
    }

    return ERR_NONE;
}

int Load_S3M(MAS_Module* mod, bool verbose)
{
    memset(mod, 0, sizeof(MAS_Module));
    for (int x = 0; x < 28; x++)
        mod->title[x] = read8();    // read song name

    read8(); // No need to check this value
//    if (read8() != 0x1A)
//        return ERR_INVALID_MODULE;

    if (read8() != 16)
        return ERR_INVALID_MODULE;

    if (verbose)
    {
        printf(vstr_s3m_div);
        printf("Loading S3M, \"%s\"\n", mod->title);
    }

    skip8(2); // reserved space
    mod->order_count = (u8)read16();
    mod->inst_count = (u8)read16();
    mod->samp_count = mod->inst_count;
    mod->patt_count = (u8)read16();

    for (int x = 0; x < 32; x++)
        mod->channel_volume[x] = 64;

    mod->freq_mode = 0;      // amiga frequencies
    mod->old_effects = true; // old effects (maybe not?)
    mod->link_gxx = false;   // dont link gxx memory
    mod->restart_pos = 0;    // restart from beginning
    mod->old_mode = true;

    u16 s3m_flags = read16();
    (void)s3m_flags;
    u16 cwt = read16();
    (void)cwt;
    u16 ffi = read16();
    if (read32() != 'MRCS') // "SCRM" mark
        return ERR_INVALID_MODULE;

    mod->global_volume = read8()*2;
    mod->initial_speed = read8();
    mod->initial_tempo = read8();

    bool stereo = read8() >> 7; // master volume
    read8(); // ultra click removal
    u8 dp = read8(); // default pan positions (when 252)
    skip8(8 + 2); // reserved space + special pointer

    bool chan_enabled[32];
    for (int x = 0; x < 32; x++)
    {
        u8 chn = read8();
        chan_enabled[x] = chn >> 7;
        if (stereo)
        {
            if ((chn & 127) < 8) // left channel
                mod->channel_panning[x] = clamp_u8(128 - (PANNING_SEP / 2));
            else // right channel
                mod->channel_panning[x] = clamp_u8(128 + (PANNING_SEP / 2));
        }
        else
        {
            mod->channel_panning[x] = 128;
        }
    }

    for (int x = 0; x < mod->order_count; x++)
    {
        mod->orders[x] = read8();
    }

    u16 *parap_inst = (u16*)malloc(mod->inst_count * sizeof(u16));
    u16 *parap_patt = (u16*)malloc(mod->patt_count * sizeof(u16));

    for (int x = 0; x < mod->inst_count; x++)
        parap_inst[x] = read16();
    for (int x = 0; x < mod->patt_count; x++)
        parap_patt[x] = read16();

    if (dp == 252)
    {
        for (int x = 0; x < 32; x++)
        {
            u8 a = read8();
            if (a & 32)
            {
                mod->channel_panning[x] = (a & 15) * 16 > 255 ? 255 : (a & 15) * 16;
            }
            else
            {
                /*
                if (stereo)
                {
                    switch(x & 3)
                    {
                        case 0:
                        case 3:
                            mod->channel_panning[x] = clamp_u8(128 - (PANNING_SEP / 2));
                            break;
                        case 1:
                        case 2:
                            mod->channel_panning[x] = clamp_u8(128 + (PANNING_SEP / 2));
                    }
                }
                else
                {
                    mod->channel_panning[x] = 128;
                }
                */
            }
        }
    }
    else
    {
        for (int x = 0; x < 32; x++)
        {
            if (stereo)
            {
                mod->channel_panning[x] = x & 1 ?
                    clamp_u8(128 - (PANNING_SEP / 2)) : clamp_u8(128 + (PANNING_SEP / 2));
            }
            else
            {
                mod->channel_panning[x] = 128;
            }
        }
    }

    mod->instruments = (Instrument *)malloc(mod->inst_count * sizeof(Instrument));
    mod->samples = (Sample *)malloc(mod->samp_count * sizeof(Sample));
    mod->patterns = (Pattern *)malloc(mod->patt_count * sizeof(Pattern));

    if (verbose)
    {
        printf(vstr_s3m_div);
        printf("Loading Samples...\n");
        printf(vstr_s3m_sampt_top);
        printf(vstr_s3m_sampt_mid);
#ifdef vstr_s3m_sampt_slice
        printf(vstr_s3m_sampt_slice);
#endif
    }

    // load instruments
    for (int x = 0; x < mod->inst_count; x++)
    {
        if (verbose)
        {
            printf(vstr_s3m_sampt_index, x + 1);
            //printf("Sample %i\n", x + 1);
        }

        // create instrument for sample
        memset(&mod->instruments[x], 0, sizeof(Instrument));
        mod->instruments[x].global_volume = 128;

        // make notemap
        for (int y = 0; y < 120; y++)
            mod->instruments[x].notemap[y] = y | ((x + 1) << 8);

        // load sample
        file_seek_read(parap_inst[x] * 16, SEEK_SET);
        if (Load_S3M_Sample(&mod->samples[x], verbose))
        {
            printf("Error loading sample!\n");
            return ERR_UNKNOWNSAMPLE;
        }
    }

    // load patterns
    if (verbose)
    {
        printf(vstr_s3m_sampt_bottom);
        printf("Loading Patterns...\n");
        printf(vstr_s3m_div);
    }

    for (int x = 0; x < mod->patt_count; x++)
    {
        if (verbose)
        {
            printf(vstr_s3m_pattern, x + 1, ((x + 1) % 15) ? "" : "\n");
        }
        //printf("%i...", x+1);
        file_seek_read(parap_patt[x] * 16, SEEK_SET);
        Load_S3M_Pattern(&mod->patterns[x]);
    }

    if (verbose)
    {
        printf("\n");
        printf(vstr_s3m_div);
        printf("Loading Sample Data...\n");
    }

    for (int x = 0; x < mod->samp_count; x++)
    {
        file_seek_read(mod->samples[x].datapointer, SEEK_SET);
        Load_S3M_SampleData(&mod->samples[x], (u8)ffi);
    }

    if (verbose)
    {
        printf(vstr_s3m_div);
    }

    return ERR_NONE;
}
