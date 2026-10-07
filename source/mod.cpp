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

// information from FMODDOC.TXT by FireLight

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "defs.h"
#include "mas.h"
#include "mod.h"
#include "log.h"
#include "files.h"
#include "simple.h"
#include "errors.h"
#include "samplefix.h"

#ifdef SUPER_ASCII
#define vstr_mod_div "────────────────────────────────────────────\n"

#define vstr_mod_samp_top       "┌─────┬──────┬─────┬──────┬───────┬───────────────────────┐\n"
#define vstr_mod_samp_header    "│INDEX│LENGTH│LOOP │VOLUME│ MID-C │ NAME                  │\n"
#define vstr_mod_samp_slice     "├─────┼──────┼─────┼──────┼───────┼───────────────────────┤\n"
#define vstr_mod_samp           "│ %2i  │%5i │ %3s │ %3i%% │ %ihz│ %-22s│\n"
#define vstr_mod_samp_bottom    "└─────┴──────┴─────┴──────┴───────┴───────────────────────┘\n"

#define vstr_mod_pattern " * %2i\n"
#else
#define vstr_mod_div "--------------------------------------------\n"

#define vstr_mod_samp_top       vstr_mod_div
#define vstr_mod_samp_header    " INDEX LENGTH LOOP  VOLUME  MID-C   NAME                   \n"
//#define vstr_mod_samp_slice     ""
#define vstr_mod_samp           " %-2i    %-5i  %-3s   %3i%%    %ihz  %-22s \n"
#define vstr_mod_samp_bottom    vstr_mod_div

#define vstr_mod_pattern " * %2i\n"
#endif

#define ID4(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))

int Create_MOD_Instrument(Instrument *inst, u8 sample)
{
    memset(inst, 0, sizeof(Instrument));

    inst->is_valid = true;

    inst->global_volume = 128;

    // setup notemap
    for (int x = 0; x < 120; x++)
        inst->notemap[x] = x | ((sample + 1) << 8);

    return ERR_NONE;
}

int Load_MOD_SampleData(Sample *samp)
{
    if (samp->sample_length > 0)
    {
        // allocate a SAMPLE_LENGTH sized pointer to buffer in memory and load the sample into it
        samp->data = malloc(samp->sample_length);
        for (u32 t = 0; t < samp->sample_length; t++)
            ((u8*)samp->data)[t] = read8() + 128; // unsign data
    }
    FixSample(samp);
    return ERR_NONE;
}

// XM effects are an extension of MOD. The MAS format expects S3M/IT effects, so
// this function converts from MOD to MAS.
static void conv_mod_to_mas(u8 *fx, u8 *param, int pattern, int row, int channel)
{
#define cho 64

    int wfx = *fx;
    int wpm = *param;

    switch (wfx)
    {
        case 0: // 0xy arpeggio
            if (wpm != 0)
                wfx = 'J' - cho;
            else
                wfx = wpm = 0;
            break;

        case 1: // 1xx porta up
            wfx = 'F' - cho;
            if (wpm >= 0xE0)
                wpm = 0xDF;
            break;

        case 2: // 2xx porta down
            wfx = 'E' - cho;
            if (wpm >= 0xE0)
                wpm = 0xDF;
            break;

        case 3: // 3xx porta to note
            wfx = 'G' - cho;
            break;

        case 4: // 4xy vibrato
            wfx = 'H' - cho;
            break;

        case 5: // 5xy volslide+glissando
            wfx = 'L' - cho;
            break;

        case 6: // 6xy volslide+vibrato
            wfx = 'K' - cho;
            break;

        case 7: // 7xy tremolo
            wfx = 'R' - cho;
            break;

        case 8: // 8xx set panning
            wfx = 'X' - cho;
            break;

        case 9: // 9xx set offset
            wfx = 'O' - cho;
            break;

        case 0xA: // Axy volume slide
            wfx = 'D' - cho;
            break;

        case 0xB: // Bxx position jump
            wfx = 'B' - cho;
            break;

        case 0xC: // Cxx set volume
            wfx = 27; // compatibility effect
            break;

        case 0xD: // Dxx pattern break
            wfx = 'C' - cho;
            wpm = (wpm & 0xF) + (wpm >> 4) * 10;
            break;

        case 0xE: // Exy extended
            switch (wpm >> 4)
            {
                case 0: // set filter
                case 3: // glissando control
                case 5: // set finetune
                    // TODO: Unsupported
                    WARNING("Pattern %d, Row %d, Channel %d. Unsupported effect 'E%02X'\n",
                            pattern, row, channel, wpm);
                    wfx = 0;
                    wpm = 0;
                    break;

                case 1: // fine porta up
                    wfx = 'F' - cho;
                    wpm = 0xF0 | (wpm & 0xF);
                    break;

                case 2: // fine porta down
                    wfx = 'E' - cho;
                    wpm = 0xF0 | (wpm & 0xF);
                    break;

                case 4: // vibrato control
                    WARNING("Pattern %d, Row %d, Channel %d. Unsupported effect 'E%02X'\n",
                            pattern, row, channel, wpm);
                    wfx = 'S' - cho;
                    wpm = 0x30 | (wpm & 0xF);
                    break;

                case 6: // pattern loop
                    wfx = 'S' - cho;
                    wpm = 0xB0 | (wpm & 0xF);
                    break;

                case 7: // tremolo control
                    WARNING("Pattern %d, Row %d, Channel %d. Unsupported effect 'E%02X'\n",
                            pattern, row, channel, wpm);
                    wfx = 'S' - cho;
                    wpm = 0x40 | (wpm & 0xF);
                    break;

                case 8: // set panning
                    wfx = 'X' - cho;
                    wpm = (wpm & 0xF) * 16;
                    break;

                case 9: // old retrig
                    wfx = 'S' - cho;
                    wpm = 0x20 | (wpm & 0xF);
                    break;

                case 10: // fine volslide up
                    wfx = 'S' - cho;
                    wpm = 0x00 | (wpm & 0xF);
                    break;

                case 11: // fine volslide down
                    wfx = 'S' - cho;
                    wpm = 0x10 | (wpm & 0xF);
                    break;

                case 12: // note cut
                    wfx = 'S' - cho;
                    wpm = 0xC0 | (wpm & 0xF);
                    break;

                case 13: // note delay
                    wfx = 'S' - cho;
                    wpm = 0xD0 | (wpm & 0xF);
                    break;

                case 14: // pattern delay
                    wfx = 'S' - cho;
                    wpm = 0xE0 | (wpm & 0xF);
                    break;

                case 15: // Invert loop. Maxmod uses it as "Event callback"
                    VERBOSE("Pattern %d, Row %d, Channel %d. Event '0x%X'\n",
                            pattern, row, channel, wpm & 0xF);
                    wfx = 'S' - cho;
                    wpm = wpm;
                    break;
            }
            break;

        case 0xF: // Fxx set speed
            if (wpm >= 32)
                wfx = 'T' - cho;
            else
                wfx = 'A' - cho;
            break;

        default:
            wfx = 0;
            wpm = 0;
            break;
    }
    *fx = wfx;
    *param = wpm;
}

int Load_MOD_Pattern(Pattern *patt, u8 nchannels, u16 *inst_count, int pattern)
{
    memset(patt, 0, sizeof(Pattern));
    patt->nrows = 64; // MODs have fixed 64 rows per pattern

    for (u32 row = 0; row < 64 * MAX_CHANNELS; row++)
    {
        patt->data[row].note = 250;
    }

    for (u32 row = 0; row < 64; row++)
    {
        for (u32 col = 0; col < nchannels; col++)
        {
            u8 data1 = read8();    // +-------------------------------------+
            u8 data2 = read8();    // | Byte 0    Byte 1   Byte 2   Byte 3  |
            u8 data3 = read8();    // |-------------------------------------|
            u8 data4 = read8();    // |aaaaBBBB CCCCCCCCC DDDDeeee FFFFFFFFF|
                                   // +-------------------------------------+

            u16 period = (data1 & 0xf) * 256 + data2; // BBBBCCCCCCCC = sample period value
            u8 inst = (data1 & 0xF0) + (data3 >> 4);  // aaaaDDDD     = sample number
            u8 effect = data3 & 0xF;                  // eeee         = effect number
            u8 param = data4;                         // FFFFFFFF     = effect parameters

            // fix parameter for certain MOD effects
            switch (effect)
            {
                case 5: // 5xy glis+slide
                case 6: // 6xy vib+slide
                    if (param & 0xF0) // clear Y if X
                        param &= 0xF0;
            }

            PatternEntry* p = &patt->data[row * MAX_CHANNELS + col]; // copy data to pattern entry

            p->inst = inst;
            conv_mod_to_mas(&effect, &param, pattern, row, col);
            p->fx = effect;
            p->param = param;

            if (period != 0) // 0 = no note, otherwise calculate note value from the amiga period
                p->note = (int)round(12.0 * log(856.0 / (double)period) / log(2)) + 37 + 11;

            if (*inst_count < (inst + 1))
            {
                *inst_count = inst + 1;
                if (*inst_count > 31)
                    *inst_count = 31;
            }
        }
    }
    return ERR_NONE;
}

int Load_MOD_Sample(Sample *samp, int index)
{
    memset(samp, 0, sizeof(Sample));
    samp->msl_index = 0xFFFF;

    for (int x = 0; x < 22; x++) // 22 bytes : SAMPLE_NAME
        samp->name[x] = read8();
    for (int x = 0; x < 12; x++) // copy to filename
        samp->filename[x] = samp->name[x];

    samp->sample_length = (read8() * 256 + read8()) * 2; // 2 bytes : SAMPLE_LENGTH

    int finetune = read8(); // 1 byte : FINE_TUNE
    if (finetune >= 8)
        finetune -= 16;

    samp->default_volume = read8(); // 1 byte : VOLUME
    samp->loop_start = (read8() * 256 + read8()) * 2; // 2 bytes : LOOP_START
    samp->loop_end = samp->loop_start + (read8() * 256 + read8()) * 2; // 2 bytes : LOOP_LENGTH

    // calculate frequency...
    // IS THIS WRONG?? :
    samp->frequency = (int)(8363.0 * pow(2.0, ((double)finetune) * (1.0 / 192.0)));

    samp->global_volume = 64; // max global volume
    if ((samp->loop_end - samp->loop_start) <= 2) // if loop length <= 2 then disabled loop
    {
        samp->loop_type = samp->loop_start = samp->loop_end = 0;
    }
    else // otherwise enable
    {
        samp->loop_type = 1;
    }

    if (samp->sample_length != 0)
    {
        //VERBOSE("%i    %s    %i%%    %ihz\n", samp->sample_length,
        //       samp->loop_type != 0 ? "Yes" : "No", (samp->default_volume * 100) / 64,
        //       samp->frequency);
        VERBOSE(vstr_mod_samp, index + 1, samp->sample_length, samp->loop_type != 0 ? "Yes" : "No",
                (samp->default_volume * 100) / 64, samp->frequency, samp->name);
        /*
        VERBOSE("  Length......%i\n", samp->sample_length);
        if (samp->loop_type != 0)
        {
            VERBOSE("  Loop........%i->%i\n", samp->loop_start, samp->loop_end);
        }
        else
        {
            VERBOSE("  Loop........None\n");
        }
        VERBOSE("  Volume......%i\n", samp->default_volume);
        VERBOSE("  Middle C....%ihz\n", samp->frequency);*/
    }
    else
    {
        //VERBOSE("---\n");
    }

    return ERR_NONE;
}

int Load_MOD(MAS_Module *mod)
{
    VERBOSE("Loading MOD, ");

    memset(mod, 0, sizeof(MAS_Module));

    u32 file_start = file_tell_read();
    file_seek_read(0x438, SEEK_SET);    // Seek to offset 1080 (438h) in the file

    char sigs[5];

    u32 sig = read32(); // read in 4 bytes
    sigs[0] = sig & 0xFF;
    sigs[1] = (sig >> 8) & 0xFF;
    sigs[2] = (sig >> 16) & 0xFF;
    sigs[3] = (sig >> 24);
    sigs[4] = 0;

    u32 mod_channels;

    switch (sig)
    {
        case ID4('1', 'C', 'H', 'N'):
            mod_channels = 1;
            break;
        case ID4('2', 'C', 'H', 'N'):
            mod_channels = 2;
            break;
        case ID4('3', 'C', 'H', 'N'):
            mod_channels = 3;
            break;
        case ID4('M', '.', 'K', '.'):
        case ID4('4', 'C', 'H', 'N'):
            mod_channels = 4;
            break;
        case ID4('5', 'C', 'H', 'N'):
            mod_channels = 5;
            break;
        case ID4('6', 'C', 'H', 'N'):
            mod_channels = 6;
            break;
        case ID4('7', 'C', 'H', 'N'):
            mod_channels = 7;
            break;
        case ID4('8', 'C', 'H', 'N'):
            mod_channels = 8;
            break;
        case ID4('9', 'C', 'H', 'N'):
            mod_channels = 9;
            break;
        default: // There are also rare tunes that use **CH where ** = 10-32 channels
        {
            if ((((sig >> 16) & 0xFF) == 'C') &&
                (((sig >> 24) & 0xFF) == 'H'))
            {
                char chn_number[3];
                chn_number[0] = (char)(sig & 0xFF);
                chn_number[1] = (char)((sig >> 8) & 0xFF);
                chn_number[2] = 0;
                mod_channels = atoi(chn_number);
                if (mod_channels > MAX_CHANNELS)
                    return ERR_MANYCHANNELS;
            }
            else
            {
                return ERR_INVALID_MODULE; // otherwise exit and display error message.
            }
        }
    }

    file_seek_read(file_start, SEEK_SET); // - Seek back to position 0, the start of the file
    for (int x = 0; x < 20; x++)
        mod->title[x] = read8();          // - read in 20 bytes, store as MODULE_NAME.

    VERBOSE("\"%s\"\n", mod->title);
    VERBOSE("%i channels (%s)\n", mod_channels, sigs);

    for (int x = 0; x < MAX_CHANNELS; x++)
    {
        if ((x & 3) != 1 && (x & 3) != 2)
            mod->channel_panning[x] = clamp_u8(128 - (PANNING_SEP / 2));
        else
            mod->channel_panning[x] = clamp_u8(128 + (PANNING_SEP / 2));
        mod->channel_volume[x] = 64;
    }

    // set MOD settings
    mod->freq_mode = 0;
    mod->global_volume = 64;
    mod->initial_speed = 6;
    mod->initial_tempo = 125;
    mod->inst_count = 0; // filled in by Load_MOD_Pattern
    mod->inst_mode = false;
    mod->instruments = (Instrument *)calloc(31, sizeof(Instrument));
    mod->link_gxx = false;
    mod->old_effects = true;
    mod->restart_pos = 0;
    mod->samp_count = 0; // filled in before Load_MOD_SampleData
    mod->samples = (Sample *)calloc(31, sizeof(Sample));
    mod->stereo = true;
    mod->xm_mode = true;
    mod->old_mode = true;

    VERBOSE(vstr_mod_div);
    VERBOSE("Loading Samples...\n");
    VERBOSE(vstr_mod_samp_top);
    VERBOSE(vstr_mod_samp_header);
#ifdef vstr_mod_samp_slice
    VERBOSE(vstr_mod_samp_slice);
#endif

    // Load Sample Information
    for (int x = 0; x < 31; x++)
    {
        //VERBOSE("Loading Sample %i...\n", x+1);
        Load_MOD_Sample(&mod->samples[x], x);

        // Only setup instrument for samples that have any length
        if (mod->samples[x].sample_length != 0)
            Create_MOD_Instrument(&mod->instruments[x], (u8)x);
    }

    // Read sequence

    // read a byte, store as SONG_LENGTH (this is the number of orders in a song)
    mod->order_count = read8();
    // read a byte, discard it (this is the UNUSED byte - used to be used in PT
    // as the restart position, but not now since jump to pattern was
    // introduced)
    mod->restart_pos = read8();
    if (mod->restart_pos >= 127)
        mod->restart_pos = 0;

    int npatterns = 0; // set NUMBER_OF_PATTERNS to equal 0......... or -1 :)
    for (int x = 0; x < 128; x++) // from this point, loop 128 times
    {
        // read 1 byte, store it as ORDER <loopcounter>
        mod->orders[x] = read8();
        // if this value was bigger than NUMBER_OF_PATTERNS then set it to that value.
        if (mod->orders[x] >= npatterns)
            npatterns=mod->orders[x] + 1;
    }

    // read 4 bytes, discard them (we are at position 1080 again, this is M.K. etc!)
    read32();

    mod->patt_count = npatterns;
    mod->patterns = (Pattern *)calloc(mod->patt_count, sizeof(Pattern));

    VERBOSE(vstr_mod_samp_bottom);
    VERBOSE("Sequence has %i entries.\n", mod->order_count);
    VERBOSE("Module has %i pattern%s.\n", mod->patt_count, mod->patt_count == 1 ? "" : "s");
    VERBOSE(vstr_mod_div);
    VERBOSE("Loading Patterns...\n");
    VERBOSE(vstr_mod_div);

    // Load Patterns
    for (int x = 0; x < mod->patt_count; x++)
    {
        VERBOSE(vstr_mod_pattern, x + 1);
        Load_MOD_Pattern(&mod->patterns[x], (u8)mod_channels, &(mod->inst_count), x);
    }

    VERBOSE("\n");
    VERBOSE(vstr_mod_div);

    // Load Sample Data
    VERBOSE("Loading Sample Data...\n");

    mod->samp_count = mod->inst_count;
    for (int x = 0; x < 31; x++)
    {
        Load_MOD_SampleData(&mod->samples[x]);
    }

    VERBOSE(vstr_mod_div);

    Sanitize_Module(mod);

    return ERR_NONE;
}
