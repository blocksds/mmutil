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

// information from ITTECH.TXT

#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "mas.h"
#include "it.h"
#include "files.h"
#include "simple.h"
#include "errors.h"
#include "samplefix.h"

#ifdef SUPER_ASCII
#define vstr_it_div "────────────────────────────────────────────\n"

#define vstr_it_instr_top    "┌─────┬──────┬─────┬─────┬───────────────────────────┐\n"
#define vstr_it_instr_head   "│INDEX│VOLUME│ NNA │ ENV │            NAME           │\n"
#define vstr_it_instr_slice  "├─────┼──────┼─────┼─────┼───────────────────────────┤\n"
#define vstr_it_instr        "│%3i  │ %3i%% │ %3s │ %s%s%s │ %-26s│\n"
#define vstr_it_instr_bottom "└─────┴──────┴─────┴─────┴───────────────────────────┘\n"

#define vstr_it_samp_top    "┌─────┬──────┬───────┬──────┬─────────┬───────────────────────────┐\n"
#define vstr_it_samp_head   "│INDEX│VOLUME│DVOLUME│ LOOP │  MID-C  │            NAME           │\n"
#define vstr_it_samp_slice  "├─────┼──────┼───────┼──────┼─────────┼───────────────────────────┤\n"
#define vstr_it_samp        "│%3i  │ %3i%% │ %3i%%  │ %4s │%6ihz │ %-26s│\n"
#define vstr_it_samp_bottom "└─────┴──────┴───────┴──────┴─────────┴───────────────────────────┘\n"

#define vstr_it_pattern " \x0e %2i"
#else
#define vstr_it_div "--------------------------------------------\n"

#define vstr_it_instr_top   vstr_it_div
#define vstr_it_instr_head  " INDEX VOLUME  NNA   ENV   NAME\n"
//#define vstr_it_instr_slice ""
#define vstr_it_instr       " %-3i   %3i%%    %3s   %s%s%s   %-26s \n"
#define vstr_it_instr_bottom vstr_it_div

#define vstr_it_samp_top    vstr_it_div
#define vstr_it_samp_head   " INDEX VOLUME DVOLUME LOOP   MID-C     NAME            \n"
//#define vstr_it_samp_slice  ""
#define vstr_it_samp        " %-3i   %3i%%   %3i%%    %4s  %6ihz   %-26s \n"
#define vstr_it_samp_bottom vstr_it_div

#define vstr_it_pattern " * %2i"
#endif

bool Load_IT_Envelope(Instrument_Envelope *env, bool unsign)
{
    // Read envelopes

    bool env_loop = false;
    bool env_sus = false;
    bool env_enabled = false;
    bool env_filter = false;

    memset(env, 0, sizeof(Instrument_Envelope));

    u8 a = read8();

    if (a & 1)
        env_enabled = true;

    if (!(a & 2))
    {
        env->loop_start = 255;
        env->loop_end = 255;
    }
    else
    {
        env_loop=true;
    }

    if (!(a & 4))
    {
        env->sus_start = 255;
        env->sus_end = 255;
    }
    else
    {
        env_sus = true;
    }

    if (a & 128)
    {
        unsign = false;
        env_filter = true;
        env->env_filter = env_filter;
    }

    u8 node_count = read8();
    if(node_count != 0)
        env->env_valid = true;

    env->node_count = node_count;

    if (env_loop)
    {
        env->loop_start = read8();
        env->loop_end = read8();
    }
    else
    {
        skip8(2);
    }

    if (env_sus)
    {
        env->sus_start = read8();
        env->sus_end = read8();
    }
    else
    {
        skip8(2);
    }

    for (int x = 0; x < 25; x++)
    {
        env->node_y[x] = read8();
        if (unsign)
            env->node_y[x] += 32;
        env->node_x[x] = read16();

    }

    read8(); // Unused byte
    env->env_enabled = env_enabled;

    return env_enabled;
}

int Load_IT_Instrument(Instrument *inst, bool verbose, int index)
{
    u16 a;

    memset(inst, 0, sizeof(Instrument));

    inst->is_valid = true;

    skip8(17);

    inst->nna = read8();
    inst->dct = read8();
    inst->dca = read8();

    a = read16();
    if (a > 255)
        a = 255;
    inst->fadeout = (u8)a;

    skip8(2);

    inst->global_volume = read8();

    a = read8();
    a = (a & 128) | ((a & 127) * 2 > 127 ? 127 : (a & 127) * 2);
    inst->setpan = a ^ 128;

    inst->random_volume = read8();

    skip8(5);

    for (int x = 0; x < 26; x++)
        inst->name[x] = read8();

    skip8(6);

    for (int x = 0; x < 120; x++)
        inst->notemap[x] = read16();

    inst->env_flags = 0;

    Load_IT_Envelope(&inst->envelope_volume, false);
    if (inst->envelope_volume.env_valid)
        inst->env_flags |= MAS_INSTR_FLAG_VOL_ENV_EXISTS;
    if (inst->envelope_volume.env_enabled)
        inst->env_flags |= MAS_INSTR_FLAG_VOL_ENV_ENABLED;

    Load_IT_Envelope(&inst->envelope_pan, true);
    if (inst->envelope_pan.env_enabled)
        inst->env_flags |= MAS_INSTR_FLAG_PAN_ENV_EXISTS;

    Load_IT_Envelope(&inst->envelope_pitch, true);
    if (inst->envelope_pitch.env_enabled)
        inst->env_flags |= MAS_INSTR_FLAG_PITCH_ENV_EXISTS;

    if (verbose)
    {
        printf(vstr_it_instr,
               index + 1,
               (inst->global_volume * 100) / 128,
               ((inst->nna == 0) ? "CUT" : ((inst->nna == 1) ? "CON" :
                   ((inst->nna == 2) ? "OFF" : ((inst->nna == 3) ? "FAD" : "???")))),
               (inst->env_flags & MAS_INSTR_FLAG_VOL_ENV_ENABLED) ? "V" : "-",
               (inst->env_flags & MAS_INSTR_FLAG_PAN_ENV_EXISTS) ? "P" : "-",
               (inst->env_flags & MAS_INSTR_FLAG_PITCH_ENV_EXISTS) ? "T" : "-",
               inst->name);

    /*
        printf("%i%%    ", (inst->global_volume * 100) / 128);
        switch (inst->nna)
        {
            case 0:
                printf("%s    ", "CUT");
                break;
            case 1:
                printf("%s    ", "OFF");
                break;
            case 2:
                printf("%s    ", "CONT");
                break;
            case 3:
                printf("%s    ", "FADE");
                break;
        }

        if ((!(inst->env_flags & MAS_INSTR_FLAG_PAN_ENV_EXISTS)) &&
            (!(inst->env_flags & MAS_INSTR_FLAG_PITCH_ENV_EXISTS)) &&
            (!(inst->env_flags & MAS_INSTR_FLAG_VOL_ENV_ENABLED)))
        {
            printf("-    ");
        }
        else
        {
            if (inst->env_flags & MAS_INSTR_FLAG_VOL_ENV_ENABLED)
                printf("V");
            if (inst->env_flags & MAS_INSTR_FLAG_PAN_ENV_EXISTS)
                printf("P");
            if (inst->env_flags & MAS_INSTR_FLAG_PITCH_ENV_EXISTS)
                printf("S");
            printf("    ");
        }
        printf("%s\n", inst->name);
    */
    }

    skip8(7);
    return 0;
}

void Create_IT_Instrument(Instrument *inst, int sample)
{
    memset(inst, 0, sizeof(Instrument));

    inst->is_valid = true;

    inst->global_volume = 128;

    for (int x = 0; x < 120; x++)
        inst->notemap[x] = x + sample * 256;
}

int Load_IT_Sample(Sample *samp)
{
    u8 a;

    memset(samp, 0, sizeof(Sample));
    samp->msl_index = 0xFFFF;

    if (read32() != 'SPMI')
        return ERR_UNKNOWNSAMPLE;

    for (int x = 0; x < 12; x++) // DOS filename
        samp->filename[x] = read8();

    if (read8() != 0)
        return ERR_UNKNOWNSAMPLE;

    samp->global_volume = read8();
    a = read8();
    samp->it_compression = a & 8 ? 1 : 0;

    bool bit16 = a & 2;
    bool hasloop = a & 16;
    bool pingpong = a & 64;

    samp->default_volume = read8();
    for (int x = 0; x < 26; x++)
        samp->name[x] = read8();

    bool samp_unsigned = false;
    a = read8();
    samp->default_panning = read8();
    samp->default_panning = (((samp->default_panning & 127) == 64) ?
                127 : (samp->default_panning << 1)) | (samp->default_panning & 128);
    if (!(a & 1))
        samp_unsigned = true;

    u32 samp_length = read32();
    u32 loop_start = read32();
    u32 loop_end = read32();
    u32 c5spd = read32();

    samp->frequency = c5spd;
    samp->sample_length = samp_length;
    samp->loop_start = loop_start;
    samp->loop_end = loop_end;

    skip8(8); // Susloop start/end

    u32 data_address = read32();
    samp->vibspeed = read8();
    samp->vibdepth = read8();
    samp->vibrate = read8();
    samp->vibtype = read8();
    samp->datapointer = data_address;

    if (hasloop)
    {
        if (pingpong)
            samp->loop_type = 2;
        else
            samp->loop_type = 1;

        samp->loop_start = loop_start;
        samp->loop_end = loop_end;
    }
    else
    {
        samp->loop_type = 0;
    }

    samp->format = (bit16 ? SAMPF_16BIT : 0) | (samp_unsigned ? 0 : SAMPF_SIGNED);
    if (samp->sample_length == 0)
        samp->loop_type = 0;

    return 0;
}

int Load_IT_Sample_CMP(u8 *p_dest_buffer, int samp_len, u16 cmwt, bool bit16);

int Load_IT_SampleData(Sample *samp, u16 cwmt)
{
    if (samp->sample_length == 0)
        return 0;

    if (samp->format & SAMPF_16BIT)
        samp->data = (u16 *)malloc(((u32)samp->sample_length) * 2);
    else
        samp->data = (u8 *)malloc(samp->sample_length);

    if (!samp->it_compression)
    {
        for (u32 x = 0; x < samp->sample_length; x++)
        {
            int a;

            if (samp->format & SAMPF_16BIT)
            {
                if (!(samp->format & SAMPF_SIGNED))
                {
                    a = (unsigned short)read16();
                }
                else
                {
                    a = (signed short)read16();
                    a += 32768;
                }
                ((u16 *)samp->data)[x] = (u16)a;
            }
            else
            {
                if (!(samp->format & SAMPF_SIGNED))
                {
                    a = (unsigned char)read8();
                }
                else
                {
                    a = (signed char)read8();
                    a += 128;
                }
                ((u8 *)samp->data)[x] = (u8)a;
            }
        }
    }
    else
    {
        Load_IT_Sample_CMP(samp->data, samp->sample_length, cwmt,
                           (bool)(samp->format & SAMPF_16BIT));
    }

    FixSample(samp);
    return 0;
}

int Empty_IT_Pattern(Pattern *patt)
{
    memset(patt, 0, sizeof(Pattern));
    patt->nrows = 64;

    for (int x = 0; x < patt->nrows*MAX_CHANNELS; x++)
    {
        patt->data[x].note = 250; // Special clears for vol and note
        patt->data[x].vol = 255;
    }

    return ERR_NONE;
}

int Load_IT_Pattern(Pattern *patt)
{
    u8 old_maskvar[MAX_CHANNELS];
    u8 old_note[MAX_CHANNELS];
    u8 old_inst[MAX_CHANNELS];
    u8 old_vol[MAX_CHANNELS];
    u8 old_fx[MAX_CHANNELS];
    u8 old_param[MAX_CHANNELS];

    memset(patt, 0, sizeof(Pattern));

    int clength = read16();
    patt->nrows = read16();
    skip8(4);

    patt->clength = clength;

    for (int x = 0; x < patt->nrows*MAX_CHANNELS; x++)
    {
        patt->data[x].note = 250; // special clears for vol&note
        patt->data[x].vol = 255;
    }

    // DECOMPRESS IT PATTERN

    u8 chanvar;
    u8 chan;
    u8 maskvar;

    for (int x = 0; x < patt->nrows; x++)
    {
GetNextChannelMarker:
        // Read byte into channelvariable.
        chanvar = read8();

        // if (channelvariable = 0) then end of row
        if (chanvar == 0)
            continue;

        // Channel = (channelvariable-1) & 63
        chan = (chanvar - 1) & 63;
        if (chan >= MAX_CHANNELS)
            return ERR_MANYCHANNELS;

        // if (channelvariable & 128) then read byte into maskvariable
        if (chanvar & 128)
            old_maskvar[chan] = read8();

        maskvar = old_maskvar[chan];

        // if (maskvariable & 1), then read note. (byte value)
        if (maskvar & 1)
        {
            old_note[chan] = read8();
            patt->data[x * MAX_CHANNELS + chan].note = old_note[chan];
        }

        // if (maskvariable & 2), then read instrument (byte value)
        if (maskvar & 2)
        {
            old_inst[chan] = read8();
            patt->data[x * MAX_CHANNELS + chan].inst = old_inst[chan];
        }

        // if (maskvariable & 4), then read volume/panning (byte value)
        if (maskvar & 4)
        {
            old_vol[chan] = read8();
            patt->data[x * MAX_CHANNELS + chan].vol = old_vol[chan];
        }

        // if (maskvariable & 8), then read command (byte value) and commandvalue
        if (maskvar & 8)
        {
            old_fx[chan] = read8();
            patt->data[x * MAX_CHANNELS + chan].fx = old_fx[chan];
            old_param[chan] = read8();
            patt->data[x * MAX_CHANNELS + chan].param = old_param[chan];
        }

        // if (maskvariable & 16), then note = lastnote for channel
        if (maskvar & 16)
            patt->data[x * MAX_CHANNELS + chan].note = old_note[chan];

        // if (maskvariable & 32), then instrument = lastinstrument for channel
        if (maskvar & 32)
            patt->data[x * MAX_CHANNELS + chan].inst = old_inst[chan];

        // if (maskvariable & 64), then volume/pan = lastvolume/pan for channel
        if (maskvar & 64)
            patt->data[x * MAX_CHANNELS + chan].vol = old_vol[chan];

        // if (maskvariable & 128), then {
        if (maskvar & 128)
        {
            // command = lastcommand for channel and
            patt->data[x * MAX_CHANNELS + chan].fx = old_fx[chan];
            // commandvalue = lastcommandvalue for channel
            patt->data[x * MAX_CHANNELS + chan].param = old_param[chan];
        }
        goto GetNextChannelMarker;
    }

    return ERR_NONE;
}

int Load_IT(MAS_Module *itm, bool verbose)
{
    int cc;

    memset(itm, 0, sizeof(MAS_Module));

    if (read32() != 'MPMI')
        return ERR_INVALID_MODULE;

    for (int x = 0; x < 28; x++)
        itm->title[x] = read8();

    itm->order_count = (u16)read16();
    itm->inst_count  = (u8)read16();
    itm->samp_count  = (u8)read16();
    itm->patt_count  = (u8)read16();

    u16 cwt = read16();
    (void)cwt; // Unused
    u16 cmwt = read16(); // upward compatible
    //skip8(4);          // created with tracker / upward compatible

    // Flags
    u16 w = read16();
    itm->stereo = w & 1;
    bool instr_mode = w & 4;
    itm->inst_mode = instr_mode;
    itm->freq_mode = w & 8;
    itm->old_effects = w & 16;
    itm->link_gxx = w & 32;

    skip8(2); // special
    itm->global_volume = read8();
    skip8(1); // mix volume
    itm->initial_speed = read8();
    itm->initial_tempo = read8();

    if (verbose)
    {
        printf(vstr_it_div);
        printf("Loading IT, \"%s\"\n", itm->title);
        printf(vstr_it_div);
        printf("#Orders......%i\n", itm->order_count);
        printf("#Instr.......%i\n", itm->inst_count);
        printf("#Samples.....%i\n", itm->samp_count);
        printf("#Patterns....%i\n", itm->patt_count);
        printf("Stereo.......%s\n", itm->stereo ? "Yes" : "No");
        printf("Slides.......%s\n", itm->freq_mode ? "Linear" : "Amiga");
        printf("Old Effects..%s\n", itm->old_effects ? "Yes" : "No");
        printf("Global Vol...%i%%\n", (itm->global_volume * 100) / 128);
        printf("Speed........%i\n", itm->initial_speed);
        printf("Tempo........%i\n", itm->initial_tempo);
        printf("Instruments..%s\n", instr_mode ? "Yes" : "Will be supplied");
        printf(vstr_it_div);
    }

    skip8(12); // SEP, PWD, MSGLENGTH, MESSAGE OFFSET, [RESERVED]
    for (int x = 0; x < 64; x++)
    {
        u8 b = read8();
        if (x < MAX_CHANNELS)
            itm->channel_panning[x] = b * 4 > 255 ? 255 : b * 4; // map 0->64 to 0->255
    }

    for (int x = 0; x < 64; x++)
    {
        u8 b = read8();
        if (x < MAX_CHANNELS)
            itm->channel_volume[x] = b;
    }

    for (int x = 0; x < itm->order_count; x++)
        itm->orders[x] = read8();

    u32 *parap_inst = malloc(itm->inst_count * sizeof(u32));
    u32 *parap_samp = malloc(itm->samp_count * sizeof(u32));
    u32 *parap_patt = malloc(itm->patt_count * sizeof(u32));

    for (int x = 0; x < itm->inst_count; x++)
        parap_inst[x] = read32();
    for (int x = 0; x < itm->samp_count; x++)
        parap_samp[x] = read32();
    for (int x = 0; x < itm->patt_count; x++)
        parap_patt[x] = read32();

    itm->samples = (Sample *)calloc(itm->samp_count, sizeof(Sample));
    itm->patterns = (Pattern *)calloc(itm->patt_count, sizeof(Pattern));

    if (instr_mode)
    {
        itm->instruments = (Instrument *)calloc(itm->inst_count, sizeof(Instrument));
        if (verbose)
        {
            printf("Loading Instruments...\n");
            printf(vstr_it_instr_top);
            printf(vstr_it_instr_head);
#ifdef vstr_it_instr_slice
            printf(vstr_it_instr_slice);
#endif
            //printf("INDEX    VOLUME    NNA    ENV    NAME\n");
        }

        // read instruments
        for (int x = 0; x < itm->inst_count; x++)
        {
            //if (verbose)
            //    printf("%i    ", x + 1);
            file_seek_read(parap_inst[x], SEEK_SET);
            Load_IT_Instrument(&itm->instruments[x], verbose, x);
        }

        if (verbose)
        {
            printf(vstr_it_instr_bottom);
        }
    }

    if (verbose)
    {
        printf("Loading Samples...\n");
        printf(vstr_it_samp_top);
        printf(vstr_it_samp_head);
#ifdef vstr_it_samp_slice
        printf(vstr_it_samp_slice);
#endif
        //printf("INDEX    VOLUME    DVOLUME    LOOP    MID-C    NAME\n");
    }

    // read samples
    for (int x = 0; x < itm->samp_count; x++)
    {
        file_seek_read(parap_samp[x], SEEK_SET);
        Load_IT_Sample(&itm->samples[x]);

        if (verbose)
        {
            printf(vstr_it_samp, x + 1, (itm->samples[x].global_volume * 100) / 64,
                   (itm->samples[x].default_volume * 100) / 64,
                   itm->samples[x].loop_type == 0 ?
                       "None" : (itm->samples[x].loop_type == 1 ? "Forw" : "BIDI"),
                   itm->samples[x].frequency, itm->samples[x].name);

            //printf("%i    %i%%    %i%%    %s    %ihz    %s\n", x + 1,
            //       (itm->samples[x].global_volume * 100) / 64,
            //       (itm->samples[x].default_volume * 100) / 64,
            //       itm->samples[x].loop_type == 0 ?
            //           "None" : (itm->samples[x].loop_type == 1 ? "Yes" : "BIDI"),
            //       itm->samples[x].frequency, itm->samples[x].name);
        }
    }

    if (verbose)
    {
        printf(vstr_it_samp_bottom);
    }

    if (!instr_mode)
    {
        if (verbose)
        {
            printf("Adding Instrument Templates...\n");
            printf(vstr_it_div);
        }

        itm->inst_count = itm->samp_count;
        itm->instruments = (Instrument*)calloc(itm->inst_count, sizeof(Instrument));
        cc = 0;

        int x;

        for (x = 0; x < itm->samp_count; x++)
        {
            if (verbose)
            {
                printf(" * %2i", x + 1);
                cc++;
                if (cc == 15)
                {
                    cc = 0;
                    printf("\n");
                }
            }

            if (itm->samples[x].sample_length > 0)
                Create_IT_Instrument(&itm->instruments[x], x + 1);
        }

        if (verbose)
        {
            if (cc != 0)
                printf((((x + 1) % 15) == 0) ? "" : "\n");
            printf(vstr_it_div);
        }
    }

    if (verbose)
    {
        printf("Reading Patterns...\n");
        printf(vstr_it_div);
    }

    // read patterns
    cc = 0;
    for (int x = 0; x < itm->patt_count; x++)
    {
        file_seek_read(parap_patt[x], SEEK_SET);

        if (parap_patt[x] != 0)
        {
            if (verbose)
            {
                printf(vstr_it_pattern, x+1);
                cc++;
                if (cc == 15)
                {
                    cc = 0;
                    printf("\n");
                }
            }
            Load_IT_Pattern(&itm->patterns[x]);
        }
        else
        {
            Empty_IT_Pattern(&itm->patterns[x]);
            //memset(&itm->patterns[x], 0, sizeof(Pattern));
        }
    }

    if (verbose)
    {
        if (cc != 0)
            printf("\n");
        printf(vstr_it_div);
        printf("Loading Sample Data...\n");
    }
    // read sample data
    for (int x = 0; x < itm->samp_count; x++)
    {
        file_seek_read(itm->samples[x].datapointer, SEEK_SET);
        Load_IT_SampleData(&itm->samples[x], cmwt);
    }

    if (verbose)
    {
        printf(vstr_it_div);
    }

    free(parap_inst);
    free(parap_samp);
    free(parap_patt);

    Sanitize_Module(itm, verbose);

    return ERR_NONE;
}

/*
NOTICE * NOTICE * NOTICE * NOTICE * NOTICE * NOTICE * NOTICE * NOTICE * NOTICE

The following sample decompression code is based on CHIBITRACKER's code
(http://chibitracker.berlios.de) which is based on xmp's code
(http://xmp.helllabs.org) which is based in openCP code.

NOTICE * NOTICE * NOTICE * NOTICE * NOTICE * NOTICE * NOTICE * NOTICE * NOTICE
*/

int Load_IT_CompressedSampleBlock(u8 **buffer)
{
    u32 size = read16();

    (*buffer) = malloc(size + 4);
    (*buffer)[size + 0] = 0;
    (*buffer)[size + 1] = 0;
    (*buffer)[size + 2] = 0;
    (*buffer)[size + 3] = 0;

    for (u32 x = 0; x < size; x++)
        (*buffer)[x] = read8();

    return ERR_NONE;
}

int Load_IT_Sample_CMP(u8 *p_dest_buffer, int samp_len, u16 cmwt, bool bit16)
{
    u8 *c_buffer = NULL;

    u8 *dest8_write = (u8 *)p_dest_buffer;
    u16 *dest16_write = (u16 *)p_dest_buffer;

    u32 nbits = bit16 ? 16 : 8;
    u32 dsize = bit16 ? 4 : 3;

    for (int i = 0; i < samp_len; i++)
        p_dest_buffer[i] = 128;

    bool it215 = (cmwt == 0x215); // Is this an it215 module?

    // Now unpack data till the dest buffer is full

    while (samp_len)
    {
        s8 v8;   // sample value
        s16 v16; // sample value 16 bit

        // read a new block of compressed data and reset variables
        Load_IT_CompressedSampleBlock(&c_buffer);
        u32 bit_readpos = 0;

        u16 block_length;   // length of compressed data block in samples
        u16 block_position; // position in block

        if (bit16)
            block_length = (samp_len < 0x4000) ? samp_len : 0x4000;
        else
            block_length = (samp_len < 0x8000) ? samp_len : 0x8000;

        block_position = 0;

        // actual "bit width"
        u8 bit_width = nbits + 1; // start with width of 9 bits

        // integrator buffers (d2 for it2.15). Reset them.
        s16 d1 = 0, d2 = 0;
        s8 d18 = 0, d28 = 0;

        // now uncompress the data block
        while (block_position < block_length)
        {
            u32 aux_value = readbits(c_buffer, bit_readpos, bit_width); // read bits
            bit_readpos += bit_width;

            if (bit_width < 7) // method 1 (1-6 bits)
            {
                if (bit16)
                {
                    if ((signed)aux_value == (1 << (bit_width - 1))) // check for "100..."
                    {
                        //read_n_bits_from_IT_compressed_block(3) + 1; // yes -> read new width;
                        aux_value = readbits(c_buffer, bit_readpos, dsize) + 1;
                        bit_readpos += dsize;
                        bit_width = (aux_value < bit_width) ? aux_value : aux_value + 1;
                        // and expand it
                        continue; // ... next value
                    }
                }
                else
                {
                    if (aux_value == ((u32)1 << ((u32)bit_width - 1))) // check for "100..."
                    {
                        //read_n_bits_from_IT_compressed_block(3) + 1; // yes -> read new width;
                        aux_value = readbits(c_buffer, bit_readpos, dsize) + 1;
                        bit_readpos += dsize;
                        bit_width = (aux_value < bit_width) ? aux_value : aux_value + 1;
                        // and expand it
                        continue; // ... next value
                    }
                }

            }
            else if (bit_width < nbits + 1) // method 2 (7-8 bits)
            {
                if (bit16)
                {
                    u16 border = (0xFFFF >> ((nbits + 1) - bit_width)) - (nbits / 2);
                    // lower border for width chg

                    if ((aux_value > border) && (aux_value <= (border + nbits)))
                    {
                        aux_value -= border; // convert width to 1-8
                        bit_width = (aux_value < bit_width) ? aux_value : aux_value + 1;
                        // and expand it
                        continue; // ... next value
                    }
                }
                else
                {
                    u16 border = (0xFF >> ((nbits + 1) - bit_width)) - (nbits / 2);
                    // lower border for width chg

                    if (aux_value > border && aux_value <= (border + nbits))
                    {
                        aux_value -= border; // convert width to 1-8
                        bit_width = (aux_value < bit_width) ? aux_value : aux_value + 1;
                        // and expand it
                        continue; // ... next value
                    }
                }
            }
            else if (bit_width == nbits + 1) // method 3 (9 bits)
            {
                if (aux_value & (1 << nbits)) // bit 8 set?
                {
                    bit_width = (aux_value + 1) & 0xff; // new width...
                    continue;                           // ... and next value
                }
            }
            else // illegal width, abort
            {
                if (c_buffer)
                {
                    free(c_buffer);
                    c_buffer = NULL;
                }
                return ERR_UNKNOWNSAMPLE;
            }

            // now expand value to signed byte
            if (bit_width < nbits)
            {
                u8 tmp_shift = nbits - bit_width;

                if (bit16)
                {
                    v16 = aux_value << tmp_shift;
                    v16 >>= tmp_shift;
                }
                else
                {
                    v8 = aux_value << tmp_shift;
                    v8 >>= tmp_shift;
                }
            }
            else
            {
                if (bit16)
                    v16 = (s16)aux_value;
                else
                    v8 = (s8)aux_value;
            }

            if (bit16)
            {
                // integrate upon the sample values
                d1 += v16;
                d2 += d1;

                // ... and store it into the buffer
                *(dest16_write++) = (it215 ? d2 + 32768 : d1 + 32768);
            }
            else
            {
                // integrate upon the sample values
                d18 += v8;
                d28 += d18;

                // ... and store it into the buffer
                *(dest8_write)++ = (it215 ? (int)d28 + 128 : (int)d18 + 128);
            }

            block_position++;
        }

        // now subtract block lenght from total length and go on
        if (c_buffer)
        {
            free(c_buffer);
            c_buffer = NULL;
        }
        samp_len -= block_length;
    }

    return ERR_NONE;
}
