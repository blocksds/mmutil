// SPDX-License-Identifier: ISC
//
// Copyright (c) 2008, Mukunda Johnson (mukunda@maxmod.org)
// Copyright (c) 2025-2026 Antonio Niño Díaz

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
#include "log.h"
#include "mas.h"
#include "errors.h"
#include "samples.h"

#define ID4(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))

static bool Load_IT_Envelope(Instrument_Envelope *env, bool unsign)
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

static int Load_IT_Instrument(Instrument *inst, int index)
{
    memset(inst, 0, sizeof(Instrument));

    inst->is_valid = true;

    skip8(17); // Magic, filename, 0x00

    inst->nna = read8();
    inst->dct = read8();
    inst->dca = read8();

    u16 fadeout = read16();
    if (fadeout > 255)
    {
        WARNING("Fadeout value over MAS limit: %u > 255\n", fadeout);
        fadeout = 255;
    }
    inst->fadeout = fadeout;

    s8 pps = read8(); // PPS: Pitch-Pan separation, range -32 -> +32
    u8 ppc = read8(); // PPC: Pitch-Pan center: C-0 to B-9 represented as 0->119
    if ((pps != 0) || (ppc != 60)) // C-5
    {
        WARNING("PPS/PPC are unsupported: %d/%u\n", pps, ppc);
    }

    inst->global_volume = read8();

    // If bit 7 is set in the IT, IGNORE the panning value. However, the MAS
    // format works the other way around. We need to set bit 7 to 0.
    u8 pan = read8();
    if (pan & (1 << 7))
    {
        inst->setpan = 0;
    }
    else
    {
        // The original value goes from 0 to 64 (not 63)
        pan = (pan & 127) * 2;
        if (pan > 127)
            pan = 127;

        inst->setpan = pan | (1 << 7);
    }

    EXTRA("    Volume........%i\n", inst->global_volume);
    EXTRA("    Panning.......%i (%s)\n", inst->setpan & 127,
          (inst->setpan & (1 << 7)) ? "set" : "not set");

    inst->random_volume = read8();
    u8 random_panning = read8();
    if (random_panning > 0)
    {
        WARNING("Random panning ignored: %u\n", random_panning);
    }

    u16 tracker_version = read16();
    EXTRA("    Tracker ver...0x%04X\n", tracker_version);

    u8 number_of_samples = read8();
    EXTRA("    Num. samples..%d\n", number_of_samples);

    skip8(1); // Padding

    for (int x = 0; x < 26; x++)
        inst->name[x] = read8();

    skip8(6);

    for (int x = 0; x < 120; x++)
        inst->notemap[x] = read16();

    inst->env_flags = 0;

    const char *volenv = "-";
    Load_IT_Envelope(&inst->envelope_volume, false);
    if (inst->envelope_volume.env_valid)
    {
        volenv = "v";
        inst->env_flags |= MAS_INSTR_FLAG_VOL_ENV_EXISTS;
    }
    if (inst->envelope_volume.env_enabled)
    {
        volenv = "V";
        inst->env_flags |= MAS_INSTR_FLAG_VOL_ENV_ENABLED;
    }

    const char *panenv = "-";
    Load_IT_Envelope(&inst->envelope_pan, true);
    if (inst->envelope_pan.env_valid)
    {
        panenv = "p";
        inst->env_flags |= MAS_INSTR_FLAG_PAN_ENV_EXISTS;
    }
    if (inst->envelope_pan.env_enabled)
    {
        panenv = "P";
        inst->env_flags |= MAS_INSTR_FLAG_PAN_ENV_ENABLED;
    }

    const char *pitchenv = "-";
    Load_IT_Envelope(&inst->envelope_pitch, true);
    if (inst->envelope_pitch.env_valid)
    {
        pitchenv = "t";
        inst->env_flags |= MAS_INSTR_FLAG_PITCH_ENV_EXISTS;
    }
    if (inst->envelope_pitch.env_enabled)
    {
        pitchenv = "T";
        inst->env_flags |= MAS_INSTR_FLAG_PITCH_ENV_ENABLED;
    }

    const char *nna_modes[4] = {
        "CUT", "CON", "OFF", "FAD"
    };

    VERBOSE("│%3i  │ %3i%% │ %3s │ %s%s%s │ %-26s│\n",
            index + 1,
            (inst->global_volume * 100) / 128,
            (inst->nna > 3) ? "???" : nna_modes[inst->nna],
            volenv, panenv, pitchenv, inst->name);

    skip8(7);
    return 0;
}

static void Sanitize_IT_Volume_Slide(char effect, u8 param, int pattern, int row, int channel)
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
            // The code in Maxmod ignores invalid volume slides, which is the
            // expected behaviour of IT files. However, we should warn the user.
            WARNING("Pattern %d, Row %d, Channel %d. "
                    "Invalid volume slide parameter '%c%02X'. Ignored by Maxmod.\n",
                    pattern, row, channel, effect, param);
        }
    }
}

static void Create_IT_Instrument(Instrument *inst, int sample)
{
    memset(inst, 0, sizeof(Instrument));

    inst->is_valid = true;

    inst->global_volume = 128;

    for (int x = 0; x < 120; x++)
        inst->notemap[x] = x + sample * 256;
}

static int Load_IT_Sample(Sample *samp, int sample_num)
{
    memset(samp, 0, sizeof(Sample));
    samp->msl_index = 0xFFFF;

    if (read32() != ID4('I', 'M', 'P', 'S'))
        return ERR_UNKNOWNSAMPLE;

    for (int x = 0; x < 12; x++) // DOS filename
        samp->filename[x] = read8();

    if (read8() != 0)
    {
        ERROR("Expected zero in sample data.\n");
        return ERR_UNKNOWNSAMPLE;
    }

    samp->global_volume = read8();
    u8 flg = read8();
    samp->it_compression = flg & 8 ? 1 : 0;

    bool bit16 = flg & 2;
    bool stereo = flg & 4;
    bool hasloop = flg & 16;
    bool pingpong = flg & 64;

    if (stereo)
    {
        WARNING("Sample %d is stereo. "
                "It will be converted to mono by keeping the left channel.\n",
                sample_num + 1);
    }

    samp->default_volume = read8();
    for (int x = 0; x < 26; x++)
        samp->name[x] = read8();

    bool samp_unsigned = false;
    u8 cvt = read8();

    // If bit 7 is set in the IT, USE the panning value. The MAS format also
    // expects bit 7 to mean that.
    u8 pan = read8();
    if (pan & (1 << 7))
    {
        // The original value goes from 0 to 64 (not 63)
        pan = (pan & 127) * 2;
        if (pan > 127)
            pan = 127;

        samp->default_panning = pan | (1 << 7);
    }
    else
    {
        samp->default_panning = 0;
    }

    if (!(cvt & 1))
        samp_unsigned = true;

    samp->it_compression = 0;
    if (flg & (1 << 3)) // Compressed samples
    {
        if (cvt & (1 << 2))
            samp->it_compression = 0x0215;
        else
            samp->it_compression = 0x0214;
    }

    if (samp->it_compression == 0)
    {
        if (!bit16 && (cvt == 0xFF))
        {
            ERROR("Samples compressed as ADPCM are not supported.\n");
            return ERR_UNKNOWNSAMPLE;
        }

        if (cvt & (1 << 1))
        {
            ERROR("Sample stored as big endian are not supported.\n");
            return ERR_UNKNOWNSAMPLE;
        }
    }

    u32 samp_length = read32();
    u32 loop_start = read32();
    u32 loop_end = read32();
    u32 c5spd = read32();

    samp->frequency = c5spd;
    samp->sample_length = samp_length;
    samp->loop_start = loop_start;
    samp->loop_end = loop_end;

    samp->original_sample_length = samp_length;
    samp->original_loop_start = loop_start;
    samp->original_loop_end = loop_end;

    u32 susloop_begin = read32();
    u32 susloop_end = read32();
    if ((susloop_begin != 0) || (susloop_end != 0))
    {
        WARNING("Sample %d has a sustain loop, which isn't supported.\n",
                sample_num + 1);
    }

    u32 data_address = read32();
    samp->vibspeed = read8();
    samp->vibdepth = read8();
    samp->vibrate = read8();
    samp->vibtype = read8();
    samp->datapointer = data_address;

    EXTRA("    Length........%i\n", samp->sample_length);
    if (samp->loop_type == 0)
        EXTRA("    Loop..........Disabled\n");
    else if (samp->loop_type == 1)
        EXTRA("    Loop..........Forward %i->%i\n", samp->loop_start, samp->loop_end);
    else if (samp->loop_type == 2)
        EXTRA("    Loop..........BIDI %i->%i\n", samp->loop_start, samp->loop_end);
    EXTRA("    Volume........%i\n", samp->default_volume);
    EXTRA("    Panning.......%i (%s)\n", samp->default_panning & 127,
          (samp->default_panning & (1 << 7)) ? "set" : "not set");
    EXTRA("    Middle C......%ihz\n", samp->frequency);
    EXTRA("    Vibrato\n");
    EXTRA("      Type........%d\n", samp->vibtype);
    EXTRA("      Depth.......%d\n", samp->vibdepth);
    EXTRA("      Rate........%d\n", samp->vibrate);
    EXTRA("      Speed.......%d\n", samp->vibspeed);

    if (hasloop)
    {
        if (pingpong)
            samp->loop_type = 2;
        else
            samp->loop_type = 1;
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

static int Load_IT_SampleData(Sample *samp, int index)
{
    if (samp->sample_length == 0)
        return 0;

    const char *compression = "Unknown";
    if (samp->it_compression == 0)
        compression = "Disabled";
    else if (samp->it_compression == 0x214)
        compression = "IT214";
    else if (samp->it_compression == 0x215)
        compression = "IT215";

    VERBOSE("%2d: Format: %s | Compression: %s\n", index,
            samp->format & SAMPF_16BIT ? "16 bits" : " 8 bits", compression);

    // For mono samples, reading the sample length is enough. For stereo
    // samples, the left channel data before the right channel data, so this
    // code simply ignores the right channel data.
    // TODO: Calculate the average of left and right and use that as mono sample?
    samp->data = Sample_AllocateData(samp->sample_length);
    Sample_ReadData_MOD(samp->data, samp->sample_length, samp->format, samp->it_compression);
    FixSample(samp, INPUT_TYPE_IT);
    return 0;
}

static int Empty_IT_Pattern(Pattern *patt)
{
    memset(patt, 0, sizeof(Pattern));
    patt->nrows = 64;

    for (int x = 0; x < patt->nrows * MAS_MAX_CHANNELS; x++)
    {
        patt->data[x].note = 250; // Special clears for vol and note
        patt->data[x].vol = 255;
    }

    return ERR_NONE;
}

static int IT_Waveform_To_MAS(u8 param, int pattern, int row, int channel)
{
    if (param & 0xC)
    {
        // OpenMPT sets the waveform to "sine" if the type is over 3
        int new_param = param & 0xF0;
        WARNING("Pattern %d, Row %d, Channel %d. "
                "Invalid waveform 'S%02X' replaced by 'S%02X'.\n",
                pattern, row, channel, param, new_param);
        param = 0;
    }

    const int result[4] =
    {
        [0] = MAS_WAVE_SINE_CONTINUE,
        [1] = MAS_WAVE_DOWN_RAMP_NORMAL_CONTINUE,
        [2] = MAS_WAVE_SQUARE_HALF_CONTINUE,
        [3] = MAS_WAVE_RANDOM,
    };

    return result[param & 3];
}

static int Load_IT_Pattern(Pattern *patt, int pattern_number,
                           bool channel_enabled[MAS_MAX_CHANNELS])
{
    u8 old_maskvar[MAS_MAX_CHANNELS];
    u8 old_note[MAS_MAX_CHANNELS];
    u8 old_inst[MAS_MAX_CHANNELS];
    u8 old_vol[MAS_MAX_CHANNELS];
    u8 old_fx[MAS_MAX_CHANNELS];
    u8 old_param[MAS_MAX_CHANNELS];

    memset(patt, 0, sizeof(Pattern));

    int clength = read16();

    patt->nrows = read16();
    if (patt->nrows > MAX_ROWS)
    {
        ERROR("Pattern %u has too many rows, it has been clamped. %u > %u\n",
              pattern_number, patt->nrows, MAX_ROWS);
        patt->nrows = MAX_ROWS;
    }

    skip8(4);

    patt->clength = clength;

    for (int x = 0; x < patt->nrows * MAS_MAX_CHANNELS; x++)
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
        if (chan >= MAS_MAX_CHANNELS)
        {
            ERROR("The file has too many channels: %u > %u\n",
                  chan, MAS_MAX_CHANNELS - 1);
            return ERR_MANYCHANNELS;
        }

        // if (channelvariable & 128) then read byte into maskvariable
        if (chanvar & 128)
            old_maskvar[chan] = read8();

        maskvar = old_maskvar[chan];

        // if (maskvariable & 1), then read note. (byte value)
        if (maskvar & 1)
        {
            old_note[chan] = read8();
            patt->data[x * MAS_MAX_CHANNELS + chan].note = old_note[chan];
        }

        // if (maskvariable & 2), then read instrument (byte value)
        if (maskvar & 2)
        {
            old_inst[chan] = read8();
            patt->data[x * MAS_MAX_CHANNELS + chan].inst = old_inst[chan];
        }

        // if (maskvariable & 4), then read volume/panning (byte value)
        if (maskvar & 4)
        {
            old_vol[chan] = read8();
            patt->data[x * MAS_MAX_CHANNELS + chan].vol = old_vol[chan];
        }

        // if (maskvariable & 8), then read command (byte value) and commandvalue
        if (maskvar & 8)
        {
            u32 fx = read8();
            u32 param = read8();

            char letter = fx + 64;

            if ((letter == 'I') || (letter == 'P') || (letter == 'Y') ||
                (letter == 'Z'))
            {
                WARNING("Pattern %d, Row %d, Channel %d. Unsupported effect '%c%02X'\n",
                        pattern_number, x, chan + 1, letter, param);

                fx = 0;
                param = 0;
            }
            else if (letter == 'A')
            {
                if (param == 0)
                {
                    WARNING("Pattern %d, Row %d, Channel %d. Removed invalid effect 'A00'\n",
                            pattern_number, x, chan + 1);
                    fx = 0;
                }
                else
                {
                    fx = MAS_FX_SET_SPEED;
                }
            }
            else if (letter > 'Z')
            {
                if (fx == 27)
                {
                    WARNING("Pattern %d, Row %d, Channel %d. Unsupported effect '#%02X'\n",
                            pattern_number, x, chan + 1, param);
                }
                else if (fx == 28)
                {
                    WARNING("Pattern %d, Row %d, Channel %d. Unsupported effect '\\%02X'\n",
                            pattern_number, x, chan + 1, param);
                }
                else
                {
                    WARNING("Pattern %d, Row %d, Channel %d. Unsupported effect '%d %02X'\n",
                            pattern_number, x, fx, chan + 1, param);
                }

                fx = 0;
                param = 0;
            }
            else if (letter == 'M')
            {
                if (param > 0x40)
                {
                    WARNING("Pattern %d, Row %d, Channel %d. Ignored 'M%02X' (max is 'M40')\n",
                            pattern_number, x, chan + 1, param);
                    fx = 0;
                    param = 0;
                }
            }
            else if (letter == 'V')
            {
                if (param > 0x80)
                {
                    WARNING("Pattern %d, Row %d, Channel %d. Ignored 'V%02X' (max is 'V80')\n",
                            pattern_number, x, chan + 1, param);
                    fx = 0;
                    param = 0;
                }
            }
            else if ((letter == 'D') || (letter == 'K') || (letter == 'L') ||
                     (letter == 'N') || (letter == 'W')) // Volume slides
            {
                Sanitize_IT_Volume_Slide(letter, param, pattern_number, x, chan + 1);
            }
            else if (letter == 'S')
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

                    case 3: // Set Vibrato Waveform
                    {
                        fx = MAS_FX_EXTENDED;
                        param = (MAS_FX_EXT_VIBRATO_WAVEFORM << 4) |
                                IT_Waveform_To_MAS(param, pattern_number, x, chan + 1);
                        supported = true;
                        break;
                    }
                    case 4: // Set Tremolo Waveform
                    {
                        fx = MAS_FX_EXTENDED;
                        param = (MAS_FX_EXT_TREMOLO_WAVEFORM << 4) |
                                IT_Waveform_To_MAS(param, pattern_number, x, chan + 1);
                        supported = true;
                        break;
                    }

                    case 7:
                    {
                        switch (param & 0xF)
                        {
                            case 3: // NNA Note Cut
                            case 4: // NNA Note Continue
                            case 5: // NNA Note Off
                            case 6: // NNA Note Fade
                            case 7: // Volume Envelope Off
                            case 8: // Volume Envelope On
                            case 9: // Panning Envelope Off
                            case 10: // Panning Envelope On
                            case 11: // Pitch Envelope Off
                            case 12: // Pitch Envelope On
                                supported = true;
                                break;

                            case 15: // Unused effect. Maxmod uses it as "Event callback"
                                VERBOSE("Pattern %d, Row %d, Channel %d. Event '0x%X'\n",
                                        pattern_number, x, chan + 1, param & 0xF);
                                supported = true;
                                break;

                            case 0: // Past Note Cut
                            case 1: // Past Note Off
                            case 2: // Past Note Fade
                            default:
                                supported = false;
                                break;
                        }
                        break;
                    }
                    default:
                        supported = true;
                        break;
                }

                if (!supported)
                {
                    WARNING("Pattern %d, Row %d, Channel %d. Unsupported effect 'S%02X'\n",
                            pattern_number, x, chan + 1, param);
                    fx = 0;
                    param = 0;
                }
            }

            old_fx[chan] = fx;
            old_param[chan] = param;
            patt->data[x * MAS_MAX_CHANNELS + chan].fx = fx;
            patt->data[x * MAS_MAX_CHANNELS + chan].param = param;
        }

        // if (maskvariable & 16), then note = lastnote for channel
        if (maskvar & 16)
            patt->data[x * MAS_MAX_CHANNELS + chan].note = old_note[chan];

        // if (maskvariable & 32), then instrument = lastinstrument for channel
        if (maskvar & 32)
            patt->data[x * MAS_MAX_CHANNELS + chan].inst = old_inst[chan];

        // if (maskvariable & 64), then volume/pan = lastvolume/pan for channel
        if (maskvar & 64)
            patt->data[x * MAS_MAX_CHANNELS + chan].vol = old_vol[chan];

        // if (maskvariable & 128), then {
        if (maskvar & 128)
        {
            // command = lastcommand for channel and
            patt->data[x * MAS_MAX_CHANNELS + chan].fx = old_fx[chan];
            // commandvalue = lastcommandvalue for channel
            patt->data[x * MAS_MAX_CHANNELS + chan].param = old_param[chan];
        }

        if (!channel_enabled[chan])
        {
            // If the channel is disabled, clear everything
            patt->data[x * MAS_MAX_CHANNELS + chan].note = 250; // special clear
            patt->data[x * MAS_MAX_CHANNELS + chan].inst = 0;
            patt->data[x * MAS_MAX_CHANNELS + chan].vol = 255; // special clear
            patt->data[x * MAS_MAX_CHANNELS + chan].fx = 0;
            patt->data[x * MAS_MAX_CHANNELS + chan].param = 0;
        }

        goto GetNextChannelMarker;
    }

    return ERR_NONE;
}

static void IT_SanitizePatterns(MAS_Module *itm)
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
                    else
                    {
                        // Jumps after the loop end or sample end are ignored
                        // and treated as jumps to offset 0. However, O00 is
                        // treated as "repeat the last 0xx", so the fix is to
                        // remove the effect.

                        Sample *samp = &itm->samples[inst - 1];

                        u32 offset = param * 256;

                        const char *point_name;
                        u32 end_point;

                        if (samp->original_loop_end != 0)
                        {
                            end_point = samp->original_loop_end;
                            point_name = "loop in sample";
                        }
                        else
                        {
                            end_point = samp->original_sample_length;
                            point_name = "sample";
                        }

                        if (offset >= end_point)
                        {
                            WARNING("Pattern %d, Row %d, Channel %d. "
                                    "'O%02X' jump after end of %s %u removed. (%u > %u)\n",
                                    p, r, c, param, point_name, inst, offset, end_point);

                            patt->data[r * MAS_MAX_CHANNELS + c].fx = 0;
                            patt->data[r * MAS_MAX_CHANNELS + c].param = 0;
                        }
                    }
                }
            }
        }
    }

    VERBOSE("────────────────────────────────────────────\n");
}

int Load_IT(MAS_Module *itm)
{
    int cc;

    memset(itm, 0, sizeof(MAS_Module));

    if (read32() != ID4('I', 'M', 'P', 'M'))
        return ERR_INVALID_MODULE;

    for (int x = 0; x < 28; x++)
        itm->title[x] = read8();

    itm->order_count = read16();
    if (itm->order_count >= MAS_MAX_PATTERN_ORDERS)
    {
        ERROR("The song has too many pattern orders, it has been clamped. %u >= %u.\n",
              itm->order_count, MAS_MAX_PATTERN_ORDERS);
        itm->order_count = MAS_MAX_PATTERN_ORDERS - 1;
    }

    itm->inst_count  = read16();
    itm->samp_count  = read16();
    itm->patt_count  = read16();

    u16 cwt = read16();  // created with tracker
    u16 cmwt = read16(); // upward compatible

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

    VERBOSE("────────────────────────────────────────────\n");
    VERBOSE("Loading IT, \"%s\"\n", itm->title);
    VERBOSE("────────────────────────────────────────────\n");
    VERBOSE("Created with 0x%04X\n", cwt);
    VERBOSE("cmwt.........0x%04X\n", cmwt);
    VERBOSE("#Orders......%i\n", itm->order_count);
    VERBOSE("#Instr.......%i\n", itm->inst_count);
    VERBOSE("#Samples.....%i\n", itm->samp_count);
    VERBOSE("#Patterns....%i\n", itm->patt_count);
    VERBOSE("Stereo.......%s\n", itm->stereo ? "Yes" : "No");
    VERBOSE("Slides.......%s\n", itm->freq_mode ? "Linear" : "Amiga");
    VERBOSE("Old Effects..%s\n", itm->old_effects ? "Yes" : "No");
    VERBOSE("Global Vol...%i%%\n", (itm->global_volume * 100) / 128);
    VERBOSE("Speed........%i\n", itm->initial_speed);
    VERBOSE("Tempo........%i\n", itm->initial_tempo);
    VERBOSE("Instruments..%s\n", instr_mode ? "Yes" : "Will be supplied");
    VERBOSE("────────────────────────────────────────────\n");

    if (itm->old_effects)
        WARNING("IT 'Old Effects' flag is enabled, but Maxmod ignores it.\n");

    bool channel_enabled[MAS_MAX_CHANNELS];

    skip8(12); // SEP, PWD, MSGLENGTH, MESSAGE OFFSET, [RESERVED]

    for (int i = 0; i < 64; i++)
    {
        u8 pan = read8();

        if (i >= MAS_MAX_CHANNELS)
            continue;

        if (pan & 128)
        {
            channel_enabled[i] = false;
            itm->channel_panning[i] = 32;
            EXTRA("Channel %u is disabled, it will be ignored.\n", i + 1);
            continue;
        }

        pan &= 127;

        channel_enabled[i] = true;

        if (pan == 100)
        {
            WARNING("Channel %d configured as surround, which isn't supported. "
                    "Panning has been set to center.\n", i + 1);
            pan = 32;
        }
        else if (pan > 64)
        {
            WARNING("Channel %d has an invalid default panning value (%u). "
                    "Panning has been set to center.\n", pan, i + 1);
            pan = 32;
        }

        if (pan == 64)
        {
            pan = 255;
        }
        else
        {
            pan = pan * 4;
        }

        itm->channel_panning[i] = pan;
    }

    for (int x = 0; x < 64; x++)
    {
        u8 b = read8();
        if (x < MAS_MAX_CHANNELS)
            itm->channel_volume[x] = b;
    }

    for (int x = 0; x < itm->order_count; x++)
        itm->orders[x] = read8();

    u32 *parap_inst = (u32*)malloc(itm->inst_count * sizeof(u32));
    u32 *parap_samp = (u32*)malloc(itm->samp_count * sizeof(u32));
    u32 *parap_patt = (u32*)malloc(itm->patt_count * sizeof(u32));

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

        VERBOSE("Loading Instruments...\n");
        VERBOSE("┌─────┬──────┬─────┬─────┬───────────────────────────┐\n");
        VERBOSE("│INDEX│VOLUME│ NNA │ ENV │            NAME           │\n");
        VERBOSE("├─────┼──────┼─────┼─────┼───────────────────────────┤\n");

        // read instruments
        for (int x = 0; x < itm->inst_count; x++)
        {
            file_seek_read(parap_inst[x], SEEK_SET);
            Load_IT_Instrument(&itm->instruments[x], x);
        }

        VERBOSE("└─────┴──────┴─────┴─────┴───────────────────────────┘\n");
    }

    VERBOSE("Loading Samples...\n");
    VERBOSE("┌─────┬──────┬───────┬──────┬─────────┬───────────────────────────┐\n");
    VERBOSE("│INDEX│VOLUME│DVOLUME│ LOOP │  MID-C  │            NAME           │\n");
    VERBOSE("├─────┼──────┼───────┼──────┼─────────┼───────────────────────────┤\n");

    // read samples
    for (int x = 0; x < itm->samp_count; x++)
    {
        file_seek_read(parap_samp[x], SEEK_SET);
        Load_IT_Sample(&itm->samples[x], x);

        VERBOSE("│%3i  │ %3i%% │ %3i%%  │ %4s │%6ihz │ %-26s│\n",
                x + 1, (itm->samples[x].global_volume * 100) / 64,
                (itm->samples[x].default_volume * 100) / 64,
                itm->samples[x].loop_type == 0 ?
                    "None" : (itm->samples[x].loop_type == 1 ? "Forw" : "BIDI"),
                itm->samples[x].frequency, itm->samples[x].name);
    }

    VERBOSE("└─────┴──────┴───────┴──────┴─────────┴───────────────────────────┘\n");

    if (!instr_mode)
    {
        VERBOSE("Adding Instrument Templates...\n");
        VERBOSE("────────────────────────────────────────────\n");

        itm->inst_count = itm->samp_count;
        itm->instruments = (Instrument*)calloc(itm->inst_count, sizeof(Instrument));
        cc = 0;

        int x;

        for (x = 0; x < itm->samp_count; x++)
        {
            VERBOSE(" * %2i", x + 1);
            cc++;
            if (cc == 15)
            {
                cc = 0;
                VERBOSE("\n");
            }

            if (itm->samples[x].sample_length > 0)
                Create_IT_Instrument(&itm->instruments[x], x + 1);
        }

        if (cc != 0)
            VERBOSE((((x + 1) % 15) == 0) ? "" : "\n");
        VERBOSE("────────────────────────────────────────────\n");
    }

    VERBOSE("Reading Patterns...\n");
    VERBOSE("────────────────────────────────────────────\n");

    // read patterns
    cc = 0;
    for (int x = 0; x < itm->patt_count; x++)
    {
        if (!Pattern_Is_Used(itm, x))
        {
            VERBOSE("Pattern %u is unused. Skipped.\n", x);
            Empty_IT_Pattern(&itm->patterns[x]);
            continue;
        }

        if (parap_patt[x] == 0)
        {
            Empty_IT_Pattern(&itm->patterns[x]);
            continue;
        }

        file_seek_read(parap_patt[x], SEEK_SET);

        VERBOSE(" * %2i\n", x + 1);

        int ret = Load_IT_Pattern(&itm->patterns[x], x, &channel_enabled[0]);
        // TODO: This leaks memory, but it's better than continuing and
        // finding the same error every time
        if (ret != ERR_NONE)
            return ret;
    }

    if (cc != 0)
        VERBOSE("\n");
    VERBOSE("────────────────────────────────────────────\n");
    VERBOSE("Loading Sample Data...\n");
    VERBOSE("────────────────────────────────────────────\n");

    // read sample data
    for (int x = 0; x < itm->samp_count; x++)
    {
        file_seek_read(itm->samples[x].datapointer, SEEK_SET);
        Load_IT_SampleData(&itm->samples[x], x);
    }

    VERBOSE("────────────────────────────────────────────\n");

    free(parap_inst);
    free(parap_samp);
    free(parap_patt);

    IT_SanitizePatterns(itm);
    Sanitize_Module(itm);

    return ERR_NONE;
}
