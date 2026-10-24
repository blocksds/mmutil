// SPDX-License-Identifier: ISC
//
// Copyright (c) 2008 Mukunda Johnson (mukunda@maxmod.org)
// Copyright (c) 2025-2026 Antonio Niño Díaz

/****************************************************************************
 *                ____ ___  ____ __  ______ ___  ____  ____/ /              *
 *               / __ `__ \/ __ `/ |/ / __ `__ \/ __ \/ __  /               *
 *              / / / / / / /_/ />  </ / / / / / /_/ / /_/ /                *
 *             /_/ /_/ /_/\__,_/_/|_/_/ /_/ /_/\____/\__,_/                 *
 *                                                                          *
 ****************************************************************************/

// information from ST3's TECH.DOC

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "mas.h"
#include "s3m.h"
#include "log.h"
#include "mas.h"
#include "files.h"
#include "simple.h"
#include "errors.h"
#include "samplefix.h"

#define S3M_NOTE(a) ((((a) & 15) + ((a) >> 4) * 12) + 12)

#define ID4(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))

static int Load_S3M_SampleData(Sample *samp, u8 ffi)
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

    FixSample(samp, INPUT_TYPE_S3M);
    return ERR_NONE;
}

static int Load_S3M_Sample(Sample *samp, int sample_num)
{
    memset(samp, 0, sizeof(Sample));
    samp->msl_index = 0xFFFF;

    u8 type = read8();

    if (type == 0) // Empty instrument (message only)
    {
        VERBOSE("│ %2i  │ ----- │ --- │ ---- │ ----- │ %-28s│\n",
                sample_num, samp->name);
    }
    else if (type == 1) // 1 = PCM instrument
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
        if (flags & 2) // Stereo
        {
            WARNING("Sample %d is stereo. "
                    "It will be converted to mono by keeping the left channel.\n",
                    sample_num);
        }

        samp->original_sample_length = samp->sample_length;
        samp->original_loop_start = samp->loop_start;
        samp->original_loop_end = samp->loop_end;

        //samp->bit16 = flags & 4 ? true : false;
        samp->format = flags & 4 ? SAMP_FORMAT_U16 : SAMP_FORMAT_U8;
        samp->frequency = read32();
        read32(); // reserved
        skip8(8); // internal variables
        for (u32 x = 0;x < 28; x++)
            samp->name[x] = read8();

        if (read32() != ID4('S', 'C', 'R', 'S'))
            return ERR_UNKNOWNSAMPLE;

        VERBOSE("│ %2i  │ %5i │ %-4s│ %3i%% │%5ihz│ %-28s│\n", sample_num,
                samp->sample_length, samp->loop_type ? "Yes" : "No",
                (samp->default_volume * 100) / 64, samp->frequency, samp->name);

        EXTRA("  Name......%s\n", samp->name);
        EXTRA("  Length....%i\n", samp->sample_length);
        if (samp->loop_type)
            EXTRA("     Loop......%i->%i\n", samp->loop_start, samp->loop_end);
        else
            EXTRA("  Loop......Disabled\n");
        EXTRA("  Volume....%i\n", samp->default_volume);
        EXTRA("  Middle C..%ihz\n", samp->frequency);
        //if (samp->bit16)
        //    EXTRA("  16 bit....yes\n");
    }
    else if (type < 8)
    {
        const char *instr_type_str[8] =
        {
            [0] = "Empty instrument (message only)",
            [1] = "PCM instrument",
            [2] = "Adlib melody instrument",
            [3] = "Adlib percussive: bass drum",
            [4] = "Adlib percussive: snare",
            [5] = "Adlib percussive: tom tom",
            [6] = "Adlib percussive: top cymbal",
            [7] = "Adlib percussive: hi-hat",
        };

        ERROR("Unsupported instrument type: %s\n", instr_type_str[type]);
    }
    else
    {
        ERROR("Unknown instrument type: 0x%02X\n", type);
    }

    return ERR_NONE;
}

static int S3M_Waveform_To_MAS(int type)
{
    const int result[8] =
    {
        [0] = MAS_WAVE_SINE,
        [1] = MAS_WAVE_DOWN_RAMP_OFFSET,
        [2] = MAS_WAVE_SQUARE_FULL,
        [3] = MAS_WAVE_RANDOM,
    };

    return result[type & 3];
}

static u8 Sanitize_S3M_Volume_Slide(char effect, u8 param, int pattern, int row, int channel)
{
    u8 x = param >> 4;
    u8 y = param & 0xF;

    if ((x == 0xF) || (y == 0xF)) // Fine volume slide
    {
        // Always valid
    }
    else // Volume slide
    {
        if ((x != 0) && (y != 0))
        {
            // Down volume slide has priority over up volume slide. Clear the up
            // volume slide setting (x) if there is an down volume slide setting
            // (y). The code in Maxmod ignores invalid volume slides, which is
            // the expected behaviour of IT files.

            u8 new_param = y;

            WARNING("Pattern %d, Row %d, Channel %d. "
                    "Invalid volume slide parameter. '%c%02X' replaced by '%c%02X'.\n",
                    pattern, row, channel, effect, param, effect, new_param);

            param = new_param;
        }
    }

    return param;
}

static void Empty_S3M_Pattern(Pattern *patt)
{
    memset(patt, 0, sizeof(Pattern));
    patt->nrows = 1;

    for (u32 row = 0; row < patt->nrows * MAS_MAX_CHANNELS; row++)
        patt->data[row].note = 250;
}

static int Load_S3M_Pattern(Pattern *patt, int pattern_number,
                            bool channel_enabled[MAS_MAX_CHANNELS])
{
    int clength = read16();
    // unpack s3m data

    memset(patt, 0, sizeof(Pattern));

    patt->clength = clength;
    patt->nrows = 64;

    for (int row = 0; row < 64 * MAS_MAX_CHANNELS; row++)
    {
        patt->data[row].note = 250;
        patt->data[row].vol = 255;
    }

    for (int row = 0; row < 64; row++)
    {
        u8 what;

        while ((what = read8()) != 0) // BYTE:what / 0 = end of row
        {
            // MAS only supports MAS_MAX_CHANNELS, which is 32, so we don't need
            // to check if we're over the limit.
            assert(MAS_MAX_CHANNELS >= 32);

            int col = what & 31; // & 31 = channel

            int z = row * MAS_MAX_CHANNELS + col;

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
                // 0 to 64 = Mute to max volume
                // 0x80 | (0 to 64) = Left to right panning (non-standard)
                // 255 = ignore (use instrument volume)
                patt->data[z].vol = read8();
            }

            if (what & 128) // & 128 = follows; BYTE:command, BYTE:info
            {
                u32 fx = read8();
                u32 param = read8();

                char letter = fx + 64;

                if (letter == 'C') // Pattern break
                {
                    // The row is stored in the file as BCD
                    u32 new_row = (param & 0xF) + (param >> 4) * 10;

                    // The valid range is 0 to 63. Higher values are ignored.
                    if (new_row < 64)
                    {
                        param = new_row;
                    }
                    else
                    {
                        WARNING("Pattern %d, Row %d, Channel. Ignored 'C%02X'.\n",
                                pattern_number, row, new_row, col + 1);
                        fx = 0;
                        param = 0;
                    }
                }
                else if ((letter == 'D') || (letter == 'K') || (letter == 'L') ||
                         (letter == 'N') || (letter == 'W')) // Volume slides
                {
                    param = Sanitize_S3M_Volume_Slide(letter, param,
                                                      pattern_number, row, col + 1);
                }
                else if (letter == 'S') // Extended effects
                {
                    bool supported = true;

                    switch (param >> 4)
                    {
                        case 1: // Glissando Control
                        case 2: // Set Finetune
                        case 5: // Set Panbrello Waveform
                        case 9: // Sound Control
                        case 10: // High Offset
                            supported = false;
                            break;

                        case 3: // Set Vibrato Waveform
                            fx = MAS_FX_EXTENDED;
                            param = (MAS_FX_EXT_VIBRATO_WAVEFORM << 4) | S3M_Waveform_To_MAS(param);
                            supported = true;
                            break;

                        case 4: // Set Tremolo Waveform
                            fx = MAS_FX_EXTENDED;
                            param = (MAS_FX_EXT_TREMOLO_WAVEFORM << 4) | S3M_Waveform_To_MAS(param);
                            supported = true;
                            break;

                        case 13: // Note Delay
                            supported = true;
                            if ((param & 0xF) == 0)
                            {
                                // In S3M, a note delay with parameter 0 doesn't
                                // sound. In MOD/XM/IT, it does.
                                WARNING("Pattern %d, Row %d, Channel %d. "
                                        "Effect 'S%02X' replaced by 'v00'.\n",
                                        pattern_number, row, col + 1, param);
                                fx = MAS_FX_XM_SET_VOLUME;
                                param = 0;
                            }
                            break;

                        case 15: // Unused effect. Maxmod uses it as "Event callback"
                            VERBOSE("Pattern %d, Row %d, Channel %d. Event '0x%X'\n",
                                    pattern_number, row, col + 1, param & 0xF);
                            supported = true;
                            break;

                        default:
                            supported = true;
                            break;
                    }

                    if (!supported)
                    {
                        WARNING("Pattern %d, Row %d, Channel %d. Unsupported effect 'S%02X'\n",
                                pattern_number, row, col + 1, param);
                        fx = 0;
                        param = 0;
                    }
                }
                else if (letter == 'X') // Set panning
                {
                    //  00h (left) to 80h (right)
                    u32 value = param * 2;
                    if (value > 255)
                        value = 255;
                    param = value;
                }
                else if (letter == 'A')
                {
                    if (param == 0)
                    {
                        WARNING("Pattern %d, Row %d, Channel %d. Removed invalid effect 'A00'\n",
                            pattern_number, row, col + 1);
                        fx = 0;
                    }
                    else
                    {
                        fx = MAS_FX_SET_SPEED;
                    }
                }
                else if (letter == 'V')
                {
                    u32 vol = param;
                    if (vol <= 64)
                    {
                        param = vol * 2; // multiply volume scale by 2
                    }
                    else
                    {
                        WARNING("Pattern %d, Row %d, Channel %d. Ignored 'V%2X' (max is 'V40')\n",
                                pattern_number, row, col + 1, vol);
                        fx = 0;
                        param = 0;
                    }
                }
                else if ((letter == 'P') || (letter == 'Y') || (letter == 'Z'))
                {
                    WARNING("Pattern %d, Row %d, Channel %d. Unsupported effect '%c'\n",
                            pattern_number, row, col + 1, letter);
                    fx = 0;
                    param = 0;
                }

                patt->data[z].fx = fx;
                patt->data[z].param = param;
            }

            if (patt->data[z].fx == 255)
            {
                patt->data[z].fx = 0;
                patt->data[z].param = 0;
            }

            if (!channel_enabled[col])
            {
                // If the channel is disabled, clear everything
                patt->data[z].note = 250; // special clear
                patt->data[z].inst = 0;
                patt->data[z].vol = 255; // special clear
                patt->data[z].fx = 0;
                patt->data[z].param = 0;
            }
        }
    }

    return ERR_NONE;
}

static void S3M_SanitizePatterns(MAS_Module *itm)
{
    VERBOSE("Sanitizing patterns...\n");
    VERBOSE("────────────────────────────────────────────\n");

    for (int p = 0; p < itm->patt_count; p++)
    {
        VERBOSE("Fixing pattern %d...\n", p);

        Pattern *patt = &itm->patterns[p];

        for (int r = 0; r < patt->nrows; r++)
        {
            for (int c = 0; c < MAS_MAX_CHANNELS; c++)
            {
                u32 inst = patt->data[r * MAS_MAX_CHANNELS + c].inst;
                u32 fx = patt->data[r * MAS_MAX_CHANNELS + c].fx;
                u32 param = patt->data[r * MAS_MAX_CHANNELS + c].param;

                if (fx == MAS_FX_SAMPLE_OFFSET)
                {
                    if (inst == 0)
                    {
                        // Sample Offset is ineffective if there is no note in
                        // the same pattern cell.

                        WARNING("Pattern %d, Row %d, Channel %d. "
                                "Removed ineffective 'O%02X' without instrument.\n",
                                p, r, c, param);

                        patt->data[r * MAS_MAX_CHANNELS + c].fx = 0;
                        patt->data[r * MAS_MAX_CHANNELS + c].param = 0;
                    }
                }
            }
        }
    }

    VERBOSE("────────────────────────────────────────────\n");
}

int Load_S3M(MAS_Module *mod)
{
    memset(mod, 0, sizeof(MAS_Module));
    for (int x = 0; x < 28; x++)
        mod->title[x] = read8();    // read song name

    read8(); // No need to check this value
//    if (read8() != 0x1A)
//        return ERR_INVALID_MODULE;

    if (read8() != 16)
        return ERR_INVALID_MODULE;

    VERBOSE("────────────────────────────────────────────\n");
    VERBOSE("Loading S3M, \"%s\"\n", mod->title);

    skip8(2); // reserved space
    mod->order_count = read16();
    if (mod->order_count >= MAS_MAX_PATTERN_ORDERS)
    {
        ERROR("The file has too many pattern orders, it has been clamped. %u >= %u\n",
              mod->order_count, MAS_MAX_PATTERN_ORDERS);
        mod->order_count = MAS_MAX_PATTERN_ORDERS - 1;
    }

    mod->inst_count = read16();
    mod->samp_count = mod->inst_count;
    mod->patt_count = read16();

    for (int x = 0; x < 32; x++)
        mod->channel_volume[x] = 64;

    mod->freq_mode = 0;      // amiga frequencies
    mod->old_effects = true; // old effects (maybe not?)
    mod->link_gxx = false;   // dont link gxx memory
    mod->restart_pos = 0;    // restart from beginning
    mod->old_mode = true;

    u16 s3m_flags = read16();

    if (s3m_flags & 4)
    {
        WARNING("S3M Amiga Limits flag is enabled, but this isn't supported\n");
        // This isn't as easy as ignoring all notes that are outside of the
        // valid limits, other effects like "slide up" are also affected.
    }

    (void)s3m_flags;
    u16 cwt = read16();
    (void)cwt;
    u16 ffi = read16();
    if (read32() != ID4('S', 'C', 'R', 'M')) // "SCRM" mark
        return ERR_INVALID_MODULE;

    mod->global_volume = read8() * 2;
    mod->initial_speed = read8();
    mod->initial_tempo = read8();

    bool stereo = read8() >> 7; // master volume
    read8(); // ultra click removal
    u8 dp = read8(); // default pan positions (when 252)
    skip8(8 + 2); // reserved space + special pointer

    bool channel_enabled[32];
    for (int x = 0; x < 32; x++)
    {
        u8 chn = read8();

        if (chn & (1 << 7))
        {
            channel_enabled[x] = false;
            EXTRA("Channel %u is disabled, it will be ignored.\n", x + 1);
            continue;
        }

        chn &= 127;
        channel_enabled[x] = true;

        if (stereo)
        {
            if (chn < 8) // Left PCM channel
            {
                mod->channel_panning[x] = clamp_u8(128 - (PANNING_SEP / 2));
            }
            else if (chn < 16) // Right PCM channel
            {
                mod->channel_panning[x] = clamp_u8(128 + (PANNING_SEP / 2));
            }
            else if (chn < 30)
            {
                u8 type = chn - 16;
                const char *type_str[] = {
                    "Adlib melody channel 1",
                    "Adlib melody channel 2",
                    "Adlib melody channel 3",
                    "Adlib melody channel 4",
                    "Adlib melody channel 5",
                    "Adlib melody channel 6",
                    "Adlib melody channel 7",
                    "Adlib melody channel 8",
                    "Adlib melody channel 9",
                    "Adlib percussion channel: bass drum",
                    "Adlib percussion channel: snare drum",
                    "Adlib percussion channel: tom tom",
                    "Adlib percussion channel: top cymbal",
                    "Adlib percussion channel: hi-hat",
                };

                ERROR("Unsupported channel %d type %u: '%s'\n", x + 1, chn,
                      type_str[type]);
            }
            else
            {
                ERROR("Invalid channel %d type %u.\n", x + 1, chn);
            }
        }
        else
        {
            mod->channel_panning[x] = 128; // Center
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

    mod->instruments = (Instrument *)calloc(mod->inst_count, sizeof(Instrument));
    mod->samples = (Sample *)calloc(mod->samp_count, sizeof(Sample));
    mod->patterns = (Pattern *)calloc(mod->patt_count, sizeof(Pattern));

    VERBOSE("────────────────────────────────────────────\n");
    VERBOSE("Loading Samples...\n");
    VERBOSE("┌─────┬───────┬─────┬──────┬───────┬─────────────────────────────┐\n");
    VERBOSE("│INDEX│LENGTH │LOOP │VOLUME│ MID-C │             NAME            │\n");
    VERBOSE("├─────┼───────┼─────┼──────┼───────┼─────────────────────────────┤\n");

    // load instruments
    for (int x = 0; x < mod->inst_count; x++)
    {
        // load sample
        file_seek_read(parap_inst[x] * 16, SEEK_SET);
        if (Load_S3M_Sample(&mod->samples[x], x + 1))
        {
            ERROR("Error loading sample %u.\n", x + 1);
            return ERR_UNKNOWNSAMPLE;
        }

        // Create instrument for sample only if the sample isn't empty
        memset(&mod->instruments[x], 0, sizeof(Instrument));

        if (mod->samples[x].sample_length > 0)
        {
            mod->instruments[x].global_volume = 128;
            mod->instruments[x].is_valid = true;

            // make notemap
            for (int y = 0; y < 120; y++)
                mod->instruments[x].notemap[y] = y | ((x + 1) << 8);
        }
    }

    // load patterns
    VERBOSE("└─────┴───────┴─────┴──────┴───────┴─────────────────────────────┘\n");
    VERBOSE("Loading Patterns...\n");
    VERBOSE("────────────────────────────────────────────\n");

    for (int x = 0; x < mod->patt_count; x++)
    {
        if (!Pattern_Is_Used(mod, x))
        {
            VERBOSE("Pattern %u is unused. Skipped.\n", x);
            Empty_S3M_Pattern(&mod->patterns[x]);
            continue;
        }

        VERBOSE(" * %2i\n", x + 1);

        file_seek_read(parap_patt[x] * 16, SEEK_SET);
        Load_S3M_Pattern(&mod->patterns[x], x, &channel_enabled[0]);
    }

    VERBOSE("\n");
    VERBOSE("────────────────────────────────────────────\n");
    VERBOSE("Loading Sample Data...\n");
    VERBOSE("────────────────────────────────────────────\n");

    for (int x = 0; x < mod->samp_count; x++)
    {
        file_seek_read(mod->samples[x].datapointer, SEEK_SET);
        Load_S3M_SampleData(&mod->samples[x], (u8)ffi);
    }

    VERBOSE("────────────────────────────────────────────\n");

    S3M_SanitizePatterns(mod);
    Sanitize_Module(mod);

    return ERR_NONE;
}
