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
#include "simple.h"
#include "systems.h"
#include "version.h"

u32 MAS_OFFSET;
u32 MAS_FILESIZE;

static int CalcEnvelopeSize(Instrument_Envelope *env)
{
    return (env->node_count * 4) + 8;
}

static int CalcInstrumentSize(Instrument *instr)
{
    int size = 12;

    if (instr->env_flags & MAS_INSTR_FLAG_VOL_ENV_EXISTS) // Volume envelope exists
        size += CalcEnvelopeSize(&instr->envelope_volume);
    if (instr->env_flags & MAS_INSTR_FLAG_PAN_ENV_EXISTS) // Panning envelope exists
        size += CalcEnvelopeSize(&instr->envelope_pan);
    if (instr->env_flags & MAS_INSTR_FLAG_PITCH_ENV_EXISTS) // Pitch envelope exists
        size += CalcEnvelopeSize(&instr->envelope_pitch);

    return size;
}

void Write_Instrument_Envelope(Instrument_Envelope *env)
{
    write8((u8)(env->node_count * 4 + 8)); // maximum is 6+75
    write8(env->loop_start);
    write8(env->loop_end);
    write8(env->sus_start);
    write8(env->sus_end);
    write8(env->node_count);
    write8(env->env_filter);
    write8(BYTESMASHER);

    if (env->node_count > 1)
    {
        for (int x = 0; x < env->node_count; x++)
        {
            int base = env->node_y[x];
            int delta;
            int range;

            if (x != env->node_count-1)
            {
                range = env->node_x[x + 1] - env->node_x[x];
                if (range > 511)
                    range = 511;
                if (range < 1)
                    range = 1;

                delta = (((env->node_y[x + 1] - base) * 512) + (range / 2)) / range;
                if (delta > 32767)
                    delta = 32767;
                if (delta < -32768)
                    delta = -32768;

                while ((base + ((delta * range) >> 9)) > 64)
                    delta--;
                while ((base + ((delta * range) >> 9)) < 0)
                    delta++;
            }
            else
            {
                range = 0;
                delta = 0;
            }
            write16((u16)delta);
            write16((u16)(base | (range << 7)));
        }
    }
}

void Write_Instrument(Instrument *inst)
{
    align32();
    inst->parapointer = file_tell_write() - MAS_OFFSET;

    write8(inst->global_volume);
    write8((u8)inst->fadeout);
    write8(inst->random_volume);
    write8(inst->dct);
    write8(inst->nna);
    write8(inst->env_flags);
    write8(inst->setpan);
    write8(inst->dca);

    int full_notemap = 0;
    int first_notemap_samp = (inst->notemap[0] >> 8);
    for (int y = 0; y < 120; y++)
    {
        if (((inst->notemap[y] & 0xFF) != y) ||
            ((inst->notemap[y] >> 8) != first_notemap_samp))
        {
            full_notemap = 1;
            break;
        }
    }

    if (full_notemap)
    {
        // full notemap
        // write offset here
        write16((u16)CalcInstrumentSize(inst));
    }
    else
    {
        // single notemap entry
        write16((u16)(0x8000 | first_notemap_samp));
    }

    write16(0); // reserved space

    if (inst->env_flags & MAS_INSTR_FLAG_VOL_ENV_EXISTS) // Write volume envelope
        Write_Instrument_Envelope(&inst->envelope_volume);
    if (inst->env_flags & MAS_INSTR_FLAG_PAN_ENV_EXISTS) // Write panning envelope
        Write_Instrument_Envelope(&inst->envelope_pan);
    if (inst->env_flags & MAS_INSTR_FLAG_PITCH_ENV_EXISTS) // Write pitch envelope
        Write_Instrument_Envelope(&inst->envelope_pitch);

    if (full_notemap)
    {
        for (int y = 0; y < 120; y++)
            write16(inst->notemap[y]);
    }
}

void Write_SampleData(Sample *samp)
{
    u32 sample_length = samp->sample_length;
    u32 sample_looplen = samp->loop_end - samp->loop_start;

    if (target_system == SYSTEM_GBA)
    {
        write32(sample_length);
        write32(samp->loop_type ? sample_looplen : 0xFFFFFFFF);
        write8(SAMP_FORMAT_U8);
        write8(BYTESMASHER);
        write16((u16)((samp->frequency * 1024 + (15768 / 2)) / 15768));
    }
    else
    {
        if (samp->format & SAMPF_16BIT)
        {
            if (samp->loop_type)
            {
                write32(samp->loop_start / 2);
                write32((samp->loop_end-samp->loop_start) / 2);
            }
            else
            {
                write32(0);
                write32(sample_length/2);
            }
        }
        else
        {
            if (samp->loop_type)
            {
                write32(samp->loop_start / 4);
                write32((samp->loop_end-samp->loop_start) / 4);
            }
            else
            {
                write32(0);
                write32(sample_length / 4);
            }
        }
        write8(sample_dsformat(samp));
        write8(sample_dsreptype(samp));
        write16((u16) ((samp->frequency * 1024 + (32768 / 2)) / 32768));
        write32(0);
    }

    // write sample data
    if (samp->format & SAMPF_16BIT)
    {
        for (u32 x = 0; x < sample_length; x++)
            write16(((u16*)samp->data)[x]);

        // add padding data
        if (samp->loop_type && sample_length >= (samp->loop_start + 2))
        {
            write16(((u16*)samp->data)[samp->loop_start]);
            write16(((u16*)samp->data)[samp->loop_start + 1]);
        }
        else
        {
            write16(0);
            write16(0);
        }
    }
    else
    {
        for (u32 x = 0; x < sample_length; x++)
            write8(((u8 *)samp->data)[x]);

        // add padding data
        if (samp->loop_type && sample_length >= (samp->loop_start + 4))
        {
            write8(((u8*)samp->data)[samp->loop_start]);
            write8(((u8*)samp->data)[samp->loop_start + 1]);
            write8(((u8*)samp->data)[samp->loop_start + 2]);
            write8(((u8*)samp->data)[samp->loop_start + 3]);
        }
        else
        {
            for (u32 x = 0; x < 4; x++)
                write8((target_system == SYSTEM_GBA) ? 128 : 0);
        }
    }
}

void Write_Sample(Sample *samp)
{
    align32(); // align data by 32 bits
    samp->parapointer = file_tell_write()-MAS_OFFSET;

    write8(samp->default_volume);
    write8(samp->default_panning);
    write16((u16)(samp->frequency / 4));
    write8(samp->vibtype);
    write8(samp->vibdepth);
    write8(samp->vibspeed);
    write8(samp->global_volume);
    write16(samp->vibrate);

    write16(samp->msl_index);

    if (samp->msl_index == 0xFFFF)
        Write_SampleData(samp);
}

#define MF_START        1
#define MF_DVOL         2
#define MF_HASVCMD      4
#define MF_HASFX        8
#define MF_NEWINSTR     16  // New instrument

#define MF_NOTEOFF      64  // LOCKED
#define MF_NOTECUT      128 // LOCKED

void Write_Pattern(Pattern* patt, bool xm_vol)
{
    u16 last_mask[MAX_CHANNELS];
    u16 last_note[MAX_CHANNELS];
    u16 last_inst[MAX_CHANNELS];
    u16 last_vol[MAX_CHANNELS];
    u16 last_fx[MAX_CHANNELS];
    u16 last_param[MAX_CHANNELS];

    patt->parapointer = file_tell_write()-MAS_OFFSET;
    write8((u8)(patt->nrows - 1));

    patt->cmarks[0] = true;
    u8 emptyvol = xm_vol ? 0 : 255;

    // using IT pattern compression

    for (int row = 0; row < patt->nrows; row++)
    {
        if (patt->cmarks[row])
        {
            for (int col=0; col < MAX_CHANNELS; col++)
            {
                last_mask[col] = 256; // row is marked, clear previous data
                last_note[col] = 256;
                last_inst[col] = 256;
                last_vol[col] = 256;
                last_fx[col] = 256;
                last_param[col] = 256;
            }
        }

        for (int col = 0; col < MAX_CHANNELS; col++)
        {
            PatternEntry *pe = &patt->data[row * MAX_CHANNELS + col];

            if (((pe->note != 250) || (pe->inst != 0) || (pe->vol != emptyvol) ||
                 (pe->fx != 0) || (pe->param != 0)))
            {
                u8 maskvar = 0;
                u8 chanvar = col + 1;

                if (pe->note != 250)
                    maskvar |= MF_START | MF_NEWINSTR;

                if (pe->inst != 0)
                    maskvar |= MF_DVOL | 32;

                if (pe->note > 250) // noteoff/cut disabled start+reset
                    maskvar &= ~(MF_NEWINSTR | 32);

                if (pe->vol != emptyvol)
                    maskvar |= MF_HASFX | MF_NOTEOFF;

                if (pe->fx != 0 || pe->param != 0)
                    maskvar |= MF_HASFX | MF_NOTECUT;

                if (maskvar & MF_START)
                {
                    if (pe->note == last_note[col])
                    {
                        maskvar &= ~MF_START;
                    }
                    else
                    {
                        last_note[col] = pe->note;

                        // DONT LET NOTEOFF/NOTECUT USE PREVIOUS PARAMETERS!
                        if (last_note[col] == 254 || last_note[col] == 255)
                            last_note[col] = 256;
                    }
                }

                if (maskvar & MF_DVOL)
                {
                    if (pe->inst == last_inst[col])
                    {
                        maskvar &= ~MF_DVOL;
                    }
                    else
                    {
                        last_inst[col] = pe->inst;
                    }
                }

                if (maskvar & MF_HASVCMD)
                {
                    if (pe->vol == last_vol[col])
                    {
                        maskvar &= ~MF_HASVCMD;
                    }
                    else
                    {
                        last_vol[col] = pe->vol;
                    }
                }

                if (maskvar & MF_HASFX)
                {
                    if ((pe->fx == last_fx[col]) && (pe->param == last_param[col]))
                    {
                        maskvar &= ~MF_HASFX;
                    }
                    else
                    {
                        last_fx[col] = pe->fx;
                        last_param[col] = pe->param;
                    }
                }

                if (maskvar != last_mask[col])
                {
                    chanvar |= MF_NOTECUT;
                    last_mask[col] = maskvar;
                }

                write8(chanvar);
                if (chanvar & MF_NOTECUT)
                    write8(maskvar);
                if (maskvar & MF_START)
                    write8(pe->note);
                if (maskvar & MF_DVOL)
                    write8(pe->inst);
                if (maskvar & MF_HASVCMD)
                    write8(pe->vol);
                if (maskvar & MF_HASFX)
                {
                    write8(pe->fx);
                    write8(pe->param);
                }
            }
            else
            {
                continue;
            }
        }
        write8(0);
    }
}

void Mark_Pattern_Row(MAS_Module *mod, int order, int row)
{
    if (row >= 256)
        return;

    if (mod->orders[order] == 255)
        order = 0;

    while (mod->orders[order] >= 254)
    {
        if (mod->orders[order] == 255)
            return;
        if (mod->orders[order] == 254)
            order++;
    }

    Pattern *p = &(mod->patterns[mod->orders[order]]);
    p->cmarks[row] = true;
}

void Mark_Patterns(MAS_Module* mod)
{
    for (int o = 0; o < mod->order_count; o++)
    {
        int p = mod->orders[o];

        if (p == 255)
            break;

        if (p == 254)
            continue;

        if (p >= mod->patt_count)
            continue;

        for (int row = 0; row < mod->patterns[p].nrows; row++)
        {
            for (int col = 0; col < MAX_CHANNELS; col++)
            {
                PatternEntry* pe = &(mod->patterns[p].data[row * MAX_CHANNELS + col]);

                if (pe->fx == 3) // PATTERN BREAK
                {
                    if (pe->param != 0) // if param != 0 then mark next pattern
                    {
                        Mark_Pattern_Row(mod, o + 1, pe->param);
                    }
                }
                else if (pe->fx == 19)
                {
                    if (pe->param == 0xB0)
                    {
                        Mark_Pattern_Row(mod, o, row);
                    }
                }
            }
        }
    }
}

int Write_MAS(MAS_Module* mod, bool verbose, bool msl_dep)
{
    file_get_byte_count();

    write32(BYTESMASHER);
    write8(MAS_TYPE_SONG);
    write8(MAS_VERSION);
    write8(BYTESMASHER);
    write8(BYTESMASHER);

    MAS_OFFSET = file_tell_write();

    write8((u8)mod->order_count);
    write8(mod->inst_count);
    write8(mod->samp_count);
    write8(mod->patt_count);
    write8((u8)((mod->link_gxx ? 1 : 0) | (mod->old_effects ? 2 : 0) |
                (mod->freq_mode ? 4 : 0) | (mod->xm_mode ? 8 : 0) |
                (msl_dep ? 16 : 0) | (mod->old_mode ? 32 : 0)));
    write8(mod->global_volume);
    write8(mod->initial_speed);
    write8(mod->initial_tempo);
    write8(mod->restart_pos);

/*
    u8 rsamp = 0;
    u16 rsamps[200];

    for (int x = 0; x < mod->samp_count; x++)
    {
        bool unique = true;
        for (int y = x-1; y >= 0; y--)
        {
            if (mod->samples[x].msl_index == mod->samples[y].msl_index)
            {
                mod->samples[x].rsamp_index = mod->samples[y].rsamp_index;
                unique=false;
                break;
            }
        }
        if (unique)
        {
            rsamps[rsamp]=mod->samples[x].msl_index;
            mod->samples[x].rsamp_index = rsamp;
            rsamp++;
        }
    }
    write8(rsamp);
*/

    write8(BYTESMASHER);
    write8(BYTESMASHER);write8(BYTESMASHER);

    for (int x = 0; x < MAX_CHANNELS; x++)
        write8(mod->channel_volume[x]);
    for (int x = 0; x < MAX_CHANNELS; x++)
        write8(mod->channel_panning[x]);

    int z;
    for (z = 0; z < mod->order_count; z++)
    {
        if (mod->orders[z] < 254)
        {
            if (mod->orders[z] < mod->patt_count)
                write8(mod->orders[z]);
            else
                write8(254);
        }
        else
        {
            write8(mod->orders[z]);
        }
    }
    for ( ; z < 200; z++)
        write8(255);

    // reserve space for offsets
    int fpos_pointer = file_tell_write();
    for (int x = 0; x < mod->inst_count * 4 + mod->samp_count * 4 + mod->patt_count * 4; x++)
        write8(BYTESMASHER); // BA BA BLACK SHEEP

/*
    if (msl_dep && target_system == SYSTEM_NDS)
    {
        for (int x = 0; x < rsamp; x++) // write sample indices
            write16(rsamps[x]);
    }
*/

    // WRITE INSTRUMENTS

    if (verbose)
        printf("Header: %i bytes\n", file_get_byte_count());

    for (int x = 0; x < mod->inst_count; x++)
        Write_Instrument(&mod->instruments[x]);

    for (int x = 0; x < mod->samp_count; x++)
        Write_Sample(&mod->samples[x]);

    if (verbose)
        printf("Instruments: %i bytes\n", file_get_byte_count());

    Mark_Patterns(mod);
    for (int x = 0; x < mod->patt_count; x++)
    {
//        for (y = 0; y < mod->order_count; y++)
//        {
//            if (mod->orders[y] == x)
                Write_Pattern(&mod->patterns[x], mod->xm_mode);
//        }
    }
    align32();

    if (verbose)
        printf("Patterns: %i bytes\n", file_get_byte_count());

    MAS_FILESIZE = file_tell_write() - MAS_OFFSET;

    file_seek_write(MAS_OFFSET - 8, SEEK_SET);
    write32(MAS_FILESIZE);

    file_seek_write(fpos_pointer, SEEK_SET);
    for (int x = 0; x < mod->inst_count; x++)
        write32(mod->instruments[x].parapointer);
    for (int x = 0; x < mod->samp_count; x++)
    {
        printf("sample %s is at %d/%d of %d\n", mod->samples[x].name,
               mod->samples[x].parapointer, file_tell_write(),
               mod->samples[x].sample_length);
        write32(mod->samples[x].parapointer);
    }
    for (int x = 0; x < mod->patt_count; x++)
        write32(mod->patterns[x].parapointer);

    return MAS_FILESIZE;
}

void Delete_Module(MAS_Module *mod)
{
    if (mod->instruments)
        free(mod->instruments);

    if (mod->samples)
    {
        for (int x = 0; x < mod->samp_count; x++)
        {
            if (mod->samples[x].data)
                free(mod->samples[x].data);
        }
        free(mod->samples);
    }
    if (mod->patterns)
    {
        free(mod->patterns);
    }
}
