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

// information from XM.TXT by Mr.H of Triton

#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "mas.h"
#include "xm.h"
#include "files.h"
#include "log.h"
#include "simple.h"
#include "errors.h"
#include "math.h"
#include "samples.h"

#define ID4(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))

static int Get_XM_Frequency(s8 relnote, s8 finetune)
{
    double rn = relnote;
    double ft = finetune;
    double middle_c = 8363.0f;
    double freq = middle_c * pow(2.0, (1.0 / 12.0) * rn + (1.0 / (12.0 * 128.0)) * ft);
    return (int)freq;
}

static int Load_XM_Instrument(Instrument *inst, MAS_Module *mas, u8 *p_nextsample,
                              int instrument_number)
{
    int ns = *p_nextsample;

    memset(inst, 0, sizeof(Instrument));

    int inst_headstart = file_tell_read();
    int inst_size = read32();

    for (int x = 0; x < 22; x++)
        inst->name[x] = read8(); // instrument name

    //VERBOSE("  Name=\"%s\"\n", inst->name);
    //if (read8() != 0)
    //    return ERR_UNKNOWNINST;

    read8(); // instrument type, SUPPOSED TO ALWAYS BE 0...

    int nsamples = read16();

    if (nsamples > 0)
    {
        inst->is_valid = true;

        int samp_headsize = read32();

        // read sample map
        for (int x = 0; x < 96; x++)
            inst->notemap[x + 12] = ((read8() + ns + 1) * 256) | (x + 12);

        for (int x = 0; x < 12; x++)
            inst->notemap[x] =(inst->notemap[12] & 0xFF00) | x;

        for (int x = 96; x < 120; x++)
            inst->notemap[x] =(inst->notemap[12] & 0xFF00) | x;

        for (int x = 0; x < 12; x++)
        {
            inst->envelope_volume.node_x[x] = read16();
            inst->envelope_volume.node_y[x] = (u8)read16();
        }

        for (int x = 0; x < 12; x++)
        {
            inst->envelope_pan.node_x[x] = read16();
            inst->envelope_pan.node_y[x] = (u8)read16();
        }

        inst->global_volume = 128;
        inst->envelope_volume.node_count = read8();
        inst->envelope_pan.node_count = read8();
        inst->envelope_volume.sus_start = inst->envelope_volume.sus_end = read8();
        inst->envelope_volume.loop_start = read8();
        inst->envelope_volume.loop_end = read8();
        inst->envelope_pan.sus_start = inst->envelope_pan.sus_end = read8();
        inst->envelope_pan.loop_start = read8();
        inst->envelope_pan.loop_end = read8();

        u8 volbits = read8();
        u8 panbits = read8();
        inst->env_flags = 0;
        if (volbits & 1)
            inst->env_flags |= MAS_INSTR_FLAG_VOL_ENV_EXISTS | MAS_INSTR_FLAG_VOL_ENV_ENABLED;
        if (panbits & 1)
            inst->env_flags |= MAS_INSTR_FLAG_PAN_ENV_EXISTS | MAS_INSTR_FLAG_PAN_ENV_ENABLED;

        if (!(volbits & 2))
            inst->envelope_volume.sus_start=inst->envelope_volume.sus_end = 255;
        if (!(panbits & 2))
            inst->envelope_pan.sus_start=inst->envelope_pan.sus_end = 255;

        if (!(volbits & 4))
            inst->envelope_volume.loop_start=inst->envelope_volume.loop_end = 255;
        if (!(panbits & 4))
            inst->envelope_pan.loop_start=inst->envelope_pan.loop_end = 255;

        u8 vibtype = read8();
        u16 vibsweep = 32768 / (read8() + 1);
        u8 vibdepth = read8();
        u8 vibrate = read8();
        inst->fadeout = read16() / 32; // apply scalar!
        file_seek_read(inst_headstart + inst_size, SEEK_SET);

        if (volbits & 1)
            EXTRA("  Has Volume Envelope\n");
        if (panbits & 1)
            EXTRA("  Has Panning Envelope\n");
        if (nsamples != 1)
            EXTRA("  Contains %i samples...\n", nsamples);
        else
            EXTRA("  Loading sample\n");

        // read sample headers
        for (int x = 0; x < nsamples; x++)
        {
            if (ns + x >= 256)
                return ERR_TOOMANYSAMPLES;
            int samp_headstart = file_tell_read();

            Sample *samp = &mas->samples[ns + x];

            if (nsamples != 1)
                EXTRA("  Loading sample %i...\n", x+1);

            memset(samp, 0, sizeof(Sample));
            samp->msl_index = 0xFFFF;
            samp->sample_length = read32();
            samp->loop_start = read32();
            samp->loop_end = read32() + samp->loop_start;
            samp->default_volume = read8();
            samp->global_volume = 64;

            samp->original_sample_length = samp->sample_length;
            samp->original_loop_start = samp->loop_start;
            samp->original_loop_end = samp->loop_end;

            samp->vibtype = vibtype;
            samp->vibdepth = vibdepth;
            samp->vibspeed = vibrate;
            samp->vibrate = vibsweep;

            s8 finetune = (s8)read8();
            u8 flags = read8();
            samp->default_panning = (read8() >> 1) | 128;
            s8 relnote = (s8)read8();
            read8(); // reserved

            for (int y = 0; y < 22; y++)
            {
                samp->name[y] = read8();
                if (y < 12)
                    samp->filename[y] = samp->name[y];
            }

            samp->frequency = Get_XM_Frequency(relnote, finetune);

            bool stereo = flags & (1 << 5); // Stereo
            //samp->bit16 = flags & 16 ? true : false;
            samp->format = flags & 16 ? SAMP_FORMAT_U16 : SAMP_FORMAT_U8;
            if (samp->format & SAMPF_16BIT)
            {
                samp->sample_length /= 2;
                samp->loop_start /= 2;
                samp->loop_end /= 2;
            }
            samp->loop_type = flags & 3;
            file_seek_read(samp_headstart + samp_headsize, SEEK_SET);

            if (stereo)
            {
                WARNING("Sample %d in instrument %d is stereo. "
                        "It will be converted to mono by keeping the left channel.\n",
                        x + 1, instrument_number);

                // Stereo samples have doubled sample lenghts and loop start and
                // end points.
                // https://wiki.openmpt.org/Development:_Formats/XM#Stereo_samples
                samp->sample_length /= 2;
                samp->loop_start /= 2;
                samp->loop_end /= 2;
            }

            EXTRA("    Length........%i\n", samp->sample_length);
            if (samp->loop_type == 0)
                EXTRA("    Loop..........Disabled\n");
            else if (samp->loop_type == 1)
                EXTRA("    Loop..........Forward %i->%i\n", samp->loop_start, samp->loop_end);
            else if (samp->loop_type == 2)
                EXTRA("    Loop..........BIDI %i->%i\n", samp->loop_start, samp->loop_end);
            EXTRA("    Volume........%i\n", samp->default_volume);
            EXTRA("    Panning.......%i\n", samp->default_panning & 127);
            EXTRA("    Middle C......%ihz\n", samp->frequency);
            EXTRA("    16 bit........%s\n",
                  (samp->format & SAMPF_16BIT) ? "yes (will be converted)" : "no");
            EXTRA("    Vibrato\n");
            EXTRA("      Type........%d\n", vibtype);
            EXTRA("      Depth.......%d\n", vibdepth);
            EXTRA("      Rate........%d\n", vibrate);
            EXTRA("      Sweep.......%d\n", vibsweep);
        }

        // read sample
        for (int x = 0; x < nsamples; x++)
        {
            Sample *samp = &mas->samples[ns + x];
            if (samp->sample_length == 0)
                continue;

            samp->data = Sample_AllocateData(samp->sample_length);
            Sample_ReadData_MOD(samp->data, samp->sample_length, (samp->format & SAMPF_16BIT) | SAMPF_SIGNED, 0);
            Sample_UnpackDeltaCoding(samp->data, samp->sample_length);
            FixSample(samp, INPUT_TYPE_XM);
        }

        *p_nextsample = ns + nsamples;

        VERBOSE("│%3i  │  %2i   │   %s%s   │ %-22s │\n", instrument_number,
                nsamples, (volbits & 1) ? "V" : "-",
                (panbits & 1) ? "P" : "-", inst->name);
    }
    else
    {
        inst->is_valid = false;

        file_seek_read(inst_headstart + inst_size, SEEK_SET);
        VERBOSE("│%3i  │  --   │   --   │ %-22s │\n",
                instrument_number, inst->name);
    }

    return ERR_NONE;
}

static int XM_Waveform_To_MAS(u8 param, int pattern, int row, int channel)
{
    if (param & (1 << 3))
    {
        int new_param = param & ~(1 << 3);
        WARNING("Pattern %d, Row %d, Channel %d. "
                "Invalid waveform 'E%02X' replaced by 'E%02X'.\n",
                pattern, row, channel, param, new_param);
    }

    const int result[8] =
    {
        [0] = MAS_WAVE_SINE,
        [1] = MAS_WAVE_DOWN_RAMP_BROKEN,
        [2] = MAS_WAVE_SQUARE_FULL,
        [3] = MAS_WAVE_RANDOM,

        [4] = MAS_WAVE_SINE_CONTINUE,
        [5] = MAS_WAVE_DOWN_RAMP_BROKEN_CONTINUE,
        [6] = MAS_WAVE_SQUARE_FULL_CONTINUE,
        [7] = MAS_WAVE_RANDOM,
    };

    return result[param & 7];
}

// For effects 5xy, 6xy, Axy
static u8 Sanitize_XM_Volume_Slide(u8 effect, u8 param, int pattern, int row, int channel)
{
    if (((param & 0x0F) != 0) && ((param & 0xF0) != 0))
    {
        // Up volume slide has priority over down volume slide. Clear the down
        // volume slide setting (y) if there is an up volume slide setting (x).
        // This way we can simplify the code in Maxmod.

        u8 new_param = param & 0xF0; // Clear 'y'

        WARNING("Pattern %d, Row %d, Channel %d. "
                "Invalid volume slide parameter. '%X%02X' replaced by '%X%02X'.\n",
                pattern, row, channel, effect, param, effect, new_param);

        param = new_param;
    }

    return param;
}

// XM effects are an extension of MOD effects. The MAS format expects S3M/IT
// effects, so this function converts from XM to MAS.
static void Convert_XM_Effect_To_MAS(u8 *fx, u8 *param, int pattern, int row, int channel)
{
    int wfx = *fx;
    int wpm = *param;

    switch (wfx)
    {
        case 0: // 0xy arpeggio
            if (wpm != 0)
            {
                wfx = MAS_FX_ARPEGGIO;
            }
            else
            {
                wfx = MAS_FX_NONE;
                wpm = 0;
            }
            break;

        case 1: // 1xx porta up
            wfx = MAS_FX_PORTAMENTO_UP;
            if (wpm >= 0xE0)
            {
                // 0xEx and 0xFx are used for fine and extra fine portamento
                WARNING("Pattern %d, Row %d, Channel %d. Clamped '1%02X' to '1DF'.\n",
                        pattern, row, channel, wpm);
                wpm = 0xDF;
            }
            break;

        case 2: // 2xx porta down
            wfx = MAS_FX_PORTAMENTO_DOWN;
            if (wpm >= 0xE0)
            {
                // 0xEx and 0xFx are used for fine and extra fine portamento
                WARNING("Pattern %d, Row %d, Channel %d. Clamped '2%02X' to '2DF'.\n",
                        pattern, row, channel, wpm);
                wpm = 0xDF;
            }
            break;

        case 3: // 3xx porta to note
            wfx = MAS_FX_GLISSANDO;
            break;

        case 4: // 4xy vibrato
            wfx = MAS_FX_VIBRATO;
            break;

        case 5: // 5xy volslide+glissando
            wpm = Sanitize_XM_Volume_Slide(wfx, wpm, pattern, row, channel);
            wfx = MAS_FX_PORTA_VOLUME;
            break;

        case 6: // 6xy volslide+vibrato
            wpm = Sanitize_XM_Volume_Slide(wfx, wpm, pattern, row, channel);
            wfx = MAS_FX_VIBRATO_VOLUME;
            break;

        case 7: // 7xy tremolo
            wfx = MAS_FX_TREMOLO;
            break;

        case 8: // 8xx set panning
            wfx = MAS_FX_SET_PANNING;
            break;

        case 9: // 9xx set offset
            wfx = MAS_FX_SAMPLE_OFFSET;
            break;

        case 0xA: // Axy volume slide
            wpm = Sanitize_XM_Volume_Slide(wfx, wpm, pattern, row, channel);
            wfx = MAS_FX_VOLUME_SLIDE;
            break;

        case 0xB: // Bxx position jump
            wfx = MAS_FX_POSITION_JUMP;
            break;

        case 0xC: // Cxx set volume
            wfx = MAS_FX_XM_SET_VOLUME;
            if (wpm > 0x40)
            {
                WARNING("Pattern %d, Row %d, Channel %d. Set Volume 'C%02X clamped to 'C40'.\n",
                        pattern, row, channel, wpm);
                wpm = 0x40;
            }
            break;

        case 0xD: // Dxx pattern break
            wfx = MAS_FX_PATTERN_BREAK;
            // This is stored as BCD. The top nibble can go up to 0xF, the
            // bottom nibble can go up to 0x9
            wpm = (wpm & 0xF) + (wpm >> 4) * 10;
            if (wpm > 63)
            {
                WARNING("Pattern %d, Row %d, Channel %d. Jump to row %d > 63 not compatible with FT2\n",
                        pattern, row, channel, wpm);
            }
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
                    wfx = MAS_FX_NONE;
                    wpm = 0;
                    break;

                // Fine Portamento is stored as '0xF0 | (value & 0xF)'.
                // Extra Fine Portamento is stored as '0xE0 | (value & 0xF)'.
                //
                // A value that isn't exactly 0x00 is interpreted as a new
                // value, not continue. We need to set the full param to 0 when
                // we find E10 and E20.

                case 1: // fine porta up
                    wfx = MAS_FX_PORTAMENTO_UP;
                    if ((wpm & 0xF) == 0)
                        wpm = 0; // Continue
                    else
                        wpm = 0xF0 | (wpm & 0xF); // New
                    break;

                case 2: // fine porta down
                    wfx = MAS_FX_PORTAMENTO_DOWN;
                    if ((wpm & 0xF) == 0)
                        wpm = 0; // Continue
                    else
                        wpm = 0xF0 | (wpm & 0xF); // New
                    break;

                case 4: // vibrato control
                    wfx = MAS_FX_EXTENDED;
                    wpm = (MAS_FX_EXT_VIBRATO_WAVEFORM << 4) |
                          XM_Waveform_To_MAS(wpm, pattern, row, channel);
                    break;

                case 6: // pattern loop
                    wfx = MAS_FX_EXTENDED;
                    wpm = (MAS_FX_EXT_PATTERN_LOOP << 4) | (wpm & 0xF);
                    break;

                case 7: // tremolo control
                    wfx = MAS_FX_EXTENDED;
                    wpm = (MAS_FX_EXT_TREMOLO_WAVEFORM << 4) |
                          XM_Waveform_To_MAS(wpm, pattern, row, channel);
                    break;

                case 8: // set panning
                    wfx = MAS_FX_SET_PANNING;
                    wpm = (wpm & 0xF) * 16;
                    break;

                case 9: // old retrig
                    wfx = MAS_FX_EXTENDED;
                    wpm = (MAS_FX_EXT_OLD_RETRIGGER << 4) | (wpm & 0xF);
                    break;

                case 10: // fine volslide up
                    wfx = MAS_FX_EXTENDED;
                    wpm = (MAS_FX_EXT_FINE_VOL_SLIDE_UP << 4) | (wpm & 0xF);
                    break;

                case 11: // fine volslide down
                    wfx = MAS_FX_EXTENDED;
                    wpm = (MAS_FX_EXT_FINE_VOL_SLIDE_DOWN << 4) | (wpm & 0xF);
                    break;

                case 12: // note cut
                    wfx = MAS_FX_EXTENDED;
                    wpm = (MAS_FX_EXT_NOTE_CUT << 4) | (wpm & 0xF);
                    break;

                case 13: // note delay
                    wfx = MAS_FX_EXTENDED;
                    wpm = (MAS_FX_EXT_NOTE_DELAY << 4) | (wpm & 0xF);
                    break;

                case 14: // pattern delay
                    wfx = MAS_FX_EXTENDED;
                    wpm = (MAS_FX_EXT_PATTERN_DELAY << 4) | (wpm & 0xF);
                    break;

                case 15: // Unused. Maxmod uses it as "Event callback"
                    VERBOSE("Pattern %d, Row %d, Channel %d. Event '0x%X'\n",
                            pattern, row, channel, wpm & 0xF);
                    wfx = MAS_FX_EXTENDED;
                    wpm = (MAS_FX_EXT_SONG_MESSAGE << 4) | (wpm & 0xF);
                    break;
            }
            break;

        case 0xF: // Fxx set speed
        {
            if (wpm == 0)
            {
                WARNING("Pattern %d, Row %d, Channel %d. Removed invalid effect 'F00'\n",
                        pattern, row, channel);
                wfx = 0;
                wpm = 0;
            }
            else if (wpm >= 0x20)
            {
                wfx = MAS_FX_SET_TEMPO;
            }
            else
            {
                wfx = MAS_FX_SET_SPEED;
            }
            break;
        }

        case 16: // Gxx set global volume
            wfx = MAS_FX_SET_GLOBAL_VOLUME;
            if (wpm > 0x40)
            {
                WARNING("Pattern %d, Row %d, Channel %d. Set Global Volume 'G%02X clamped to 'G40'.\n",
                        pattern, row, channel, wpm);
                wpm = 0x40;
            }
            break;

        case 17: // Hxx global volume slide
            wfx = MAS_FX_GLOBAL_VOLUME_SLIDE;
            break;

        case 18: // Ixx unused
        case 19: // Jxx unused
        case 22: // Mxx unused
        case 23: // Nxx unused
        case 24: // Oxx unused
        case 26: // Qxx unused
        case 28: // Sxx unused
        case 30: // Uxx unused
        case 31: // Vxx unused
        case 32: // Wxx unused
        case 34: // Yxx unused
        case 35: // Zxx unused
        case 37: // Unused
            WARNING("Pattern %d, Row %d, Channel %d. Unsupported effect '%c%02X'\n",
                    pattern, row, channel, wfx + 'A' - 10, wpm);
            wfx = MAS_FX_NONE;
            wpm = 0;
            break;

        case 36: // \xx Smooth MIDI Macro (ModPlug hack)
            WARNING("Pattern %d, Row %d, Channel %d. Unsupported effect '\\%02X'\n",
                    pattern, row, channel, wpm);
            wfx = MAS_FX_NONE;
            wpm = 0;
            break;

        case 38: // #xx Parameter extension
            WARNING("Pattern %d, Row %d, Channel %d. Unsupported effect '#%02X'\n",
                    pattern, row, channel, wpm);
            wfx = MAS_FX_NONE;
            wpm = 0;
            break;

        default:
            WARNING("Pattern %d, Row %d, Channel %d. Unsupported effect '%d %02X'\n",
                    pattern, row, channel, wfx, wpm);
            wfx = MAS_FX_NONE;
            wpm = 0;
            break;

        case 20: // Kxx key off
            wfx = MAS_FX_XM_KEY_OFF;
            break;

        case 21: // Lxx set envelope position
            WARNING("Pattern %d, Row %d, Channel %d. Unsupported effect 'L%02X'\n",
                    pattern, row, channel, wpm);
            // Note: Don't set this to 0 so that we remember what index 29
            // corresponds to in Maxmod.
            wfx = MAS_FX_XM_ENVELOPE_POS;
            break;

        case 25: // Pxx panning slide
            wfx = MAS_FX_PANNING_SLIDE;
            break;

        case 27: // Rxx retrigger note
            wfx = MAS_FX_RETRIGGER;
            break;

        case 29: // Txx tremor
            wfx = MAS_FX_XM_TREMOR;
            break;

        case 33: // Xxx Extended effects

            // Extra Fine Portamento is stored as '0xE0 | (value & 0xF)'.
            // A value that isn't exactly 0x00 is interpreted as a new
            // value, not continue. We need to set the full param to 0 when we
            // find X10 and X20.

            if ((wpm >> 4) == 1) // X1x extra fine portamento up
            {
                wfx = MAS_FX_PORTAMENTO_UP;
                if ((wpm & 0xF) == 0)
                    wpm = 0; // Continue
                else
                    wpm = 0xE0 | (wpm & 0xF); // New
            }
            else if ((wpm >> 4) == 2) // X2x extra fine portamento down
            {
                wfx = MAS_FX_PORTAMENTO_DOWN;
                if ((wpm & 0xF) == 0)
                    wpm = 0; // Continue
                else
                    wpm = 0xE0 | (wpm & 0xF); // New
            }
            else if ((wpm >> 4) == 6) // X6x Fine Pattern Delay
            {
                wfx = MAS_FX_EXTENDED;
                wpm = (MAS_FX_EXT_FINE_PATTERN_DELAY << 4) | (wpm & 0xF);
            }
            else // Other effects
            {
                WARNING("Pattern %d, Row %d, Channel %d. Unsupported effect 'X%02X'\n",
                        pattern, row, channel, wpm);
                wfx = MAS_FX_NONE;
                wpm = 0;
            }
            break;
    }
    *fx = wfx;
    *param = wpm;
}

static void Sanitize_XM_VolumeCommand(u8 *cmd, int pattern, int row, int channel)
{
    u8 volcmd = *cmd;

    if (volcmd < 0x10) // None
    {
        volcmd = 0;
    }
    else if (volcmd <= 0x50) // Set volume
    {
        //u32 param = volcmd - 0x10;
    }
    else if (volcmd < 0x60) // Undefined
    {
        volcmd = 0;
    }
    else if (volcmd < 0x70) // Volume slide down
    {
        //u32 param = volcmd - 0x60;
    }
    else if (volcmd < 0x80) // Volume slide up
    {
        //u32 param = volcmd - 0x70;
    }
    else if (volcmd < 0x90) // Fine volume slide down
    {
        //u32 param = volcmd - 0x80;
    }
    else if (volcmd < 0xA0) // Fine volume slide up
    {
        //u32 param = volcmd - 0x90;
    }
    else if (volcmd < 0xB0) // Set vibrato speed
    {
        //u32 param = (volcmd - 0xA0) << 2;
    }
    else if (volcmd < 0xC0) // Set vibrato depth
    {
        //u32 param = (volcmd - 0xB0) << 3;
    }
    else if (volcmd < 0xD0) // Panning
    {
        //u32 param = (volcmd - 0xC0) << 4;
    }
    else if (volcmd < 0xE0) // Panning slide left
    {
        u32 param = volcmd - 0xD0;

        if (param == 0)
        {
            // FT2 Compatibility: Pan slide left with zero parameter causes
            // panning to be set to full left on every non-row tick.
            //
            // https://github.com/OpenMPT/openmpt/blob/dc49d5638db37e157003cd606dc85165551eb9da/soundlib/Snd_fx.cpp#L3287-L3294

            WARNING("Pattern %d, row %d, channel %d. Converted l0%d to p00 to emulate FT2 bug.\n",
                    pattern, row, channel, param);
            volcmd = 0xC0;
        }
    }
    else  if (volcmd < 0xF0) // Panning slide right
    {
        u32 param = volcmd - 0xE0;

        if (param == 0)
        {
            // This effect doesn't seem to have memory. If the parameter is
            // zero, it is ignored, so simply clear the effect.

            WARNING("Pattern %d, row %d, channel %d. r00 ignores memory. Removed.\n",
                    pattern, row, channel);
            volcmd = 0;
        }
    }
    else // Tone portamento / Glissando
    {
        //u32 param = (volcmd - 0xF0) << 4;
    }

    *cmd = volcmd;
}

static void Empty_XM_Pattern(Pattern *patt)
{
    memset(patt, 0, sizeof(Pattern));
    patt->nrows = 1;

    for (u32 row = 0; row < patt->nrows * MAS_MAX_CHANNELS; row++)
        patt->data[row].note = 250;
}

static int Load_XM_Pattern(Pattern *patt, u32 nchannels, int pattern)
{
    u32 headstart = file_tell_read();
    u32 headsize = read32();

    if (read8() != 0)
        return ERR_UNKNOWNPATTERN;

    if (headsize != 9)
        WARNING("Pattern header size is unusual: %u != 9\n", headsize);

    memset(patt, 0, sizeof(Pattern));

    patt->nrows = read16();
    if (patt->nrows > MAX_ROWS)
    {
        ERROR("Pattern %u has too many rows, it has been clamped. %u > %u\n",
              pattern, patt->nrows, MAX_ROWS);
        patt->nrows = MAX_ROWS;
    }

    u16 clength = read16();

    VERBOSE("- %i rows, %.2f KB\n", patt->nrows, (float)(clength) / 1000);

    for (u32 row = 0; row < patt->nrows * MAS_MAX_CHANNELS; row++)
    {
        patt->data[row].note = 250;
        patt->data[row].vol = 0;
    }

    file_seek_read(headstart + headsize, SEEK_SET);

    if (clength == 0)
    {
        // pattern is empty
        return ERR_NONE;
    }

    bool loop_start_bug_active = false;

    // Keep track of the start of the data to ensure that we read all of it
    u32 total_data_read = 0;

    // read pattern data
    for (u32 row = 0; row < patt->nrows; row++)
    {
        for (u32 col = 0; col < nchannels; col++)
        {
            u32 e = row * MAS_MAX_CHANNELS + col;
            u8 b = read8();
            total_data_read++;

            if (b & 128) // packed
            {
                if (b & 1) // bit 0 set: Note follows
                {
                    patt->data[e].note = read8(); // (byte) Note (1-96, 1 = C-0)
                    total_data_read++;
                    if (patt->data[e].note == 97)
                        patt->data[e].note = 255;
                    else
                        patt->data[e].note += 12 - 1;
                }

                if (b & 2) // 1 set: Instrument follows
                {
                    patt->data[e].inst = read8(); // (byte) Instrument (1-128)
                    total_data_read++;
                }

                if (b & 4) // 2 set: Volume column byte follows
                {
                    u8 volcmd = read8(); // (byte) Volume column byte
                    Sanitize_XM_VolumeCommand(&volcmd, pattern, row, col);
                    patt->data[e].vol = volcmd;
                    total_data_read++;
                }

                u8 fx;

                if (b & 8) // 3 set: Effect type follows
                {
                    fx = read8(); // (byte) Effect type
                    total_data_read++;
                }
                else
                {
                    fx = 0;
                }

                u8 param;

                if (b & 16) // 4 set: Guess what!
                {
                    param = read8(); // (byte) Effect parameter
                    total_data_read++;
                }
                else
                {
                    param = 0;
                }

                if (fx != 0 || param != 0)
                {
                    Convert_XM_Effect_To_MAS(&fx, &param, pattern, row, col + 1);
                    patt->data[e].fx = fx;
                    patt->data[e].param = param;
                }
            }
            else // unpacked
            {
                patt->data[e].note = b; // (byte) Note (1-96, 1 = C-0)

                if (patt->data[e].note == 97)
                    patt->data[e].note = 255;
                else
                    patt->data[e].note += 12 - 1;

                patt->data[e].inst = read8(); // (byte) Instrument (1-128)

                u8 volcmd = read8();          // (byte) Volume column byte (see below)
                Sanitize_XM_VolumeCommand(&volcmd, pattern, row, col);
                patt->data[e].vol = volcmd;

                u8 fx = read8();              // (byte) Effect type
                u8 param = read8();           // (byte) Effect parameter

                Convert_XM_Effect_To_MAS(&fx, &param, pattern, row, col + 1);

                patt->data[e].fx = fx;
                patt->data[e].param = param;

                total_data_read += 4;
            }

            // https://wiki.openmpt.org/Manual:_Effect_Reference#XM_Effect_Commands
            //
            // > When E60 is used on pattern row x, the following pattern also
            // > starts from row x instead of row 0. This can be circumvented by
            // > using a D00 command on the last row of the same pattern.
            //
            // Check if the pattern has a Dxx after any E60.
            {
                u8 fx = patt->data[e].fx;
                u8 param = patt->data[e].param;

                if ((fx == MAS_FX_EXTENDED) && (((param >> 4) == MAS_FX_EXT_PATTERN_LOOP)))
                {
                    if ((param & 0xF) == 0)
                    {
                        loop_start_bug_active = true;
                        VERBOSE("Pattern %d, row %d, ch %d. Buggy 'E60' found.\n",
                                pattern, row, col);
                    }
                }

                if (fx == MAS_FX_PATTERN_BREAK)
                {
                    if (loop_start_bug_active)
                    {
                        VERBOSE("Pattern %d, row %d, ch %d. 'D%02X' fixes 'E60' bug.\n",
                                pattern, row, col, param);
                        loop_start_bug_active = false;
                    }
                }
            }
        }
    }

    if (loop_start_bug_active)
    {
        WARNING("Pattern %d has a buggy 'E60' not followed by a 'Dxx'\n", pattern);
    }

    // If we have read more than expected, the module is corrupt
    if (total_data_read > clength)
    {
        ERROR("Read too much pattern data: %u > %u\n", total_data_read, clength);
        return ERR_INVALID_MODULE;
    }

    // If there is a mismatch, warn the user, but don't exit. Some modules have
    // some extra bytes (zeroes) after the real data.
    if (total_data_read < clength)
    {
        WARNING("Extra data found in pattern: %u < %u\n",
                total_data_read, clength);

        // Read the remaining pattern data to move pointer to the start of the
        // next pattern.
        while (total_data_read < clength)
        {
            read8();
            total_data_read++;
        }
    }

    return ERR_NONE;
}

static void XM_SanitizePatterns(MAS_Module *itm)
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
                                "Removed ineffective '9%02X' without instrument.\n",
                                p, r, c, param);

                        patt->data[r * MAS_MAX_CHANNELS + c].fx = 0;
                        patt->data[r * MAS_MAX_CHANNELS + c].param = 0;
                    }
                    else
                    {
                        // Jumps after the loop end or sample end turn the note
                        // off. However, it may be better to set the volume to 0
                        // instead, in case the volume is set to non-zero later.

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
                                    "'9%02X' jump after end of %s %u replaced by 'C00'. (%u > %u)\n",
                                    p, r, c, param, point_name, inst, offset, end_point);

                            patt->data[r * MAS_MAX_CHANNELS + c].fx = MAS_FX_XM_SET_VOLUME;
                            patt->data[r * MAS_MAX_CHANNELS + c].param = 0;
                        }
                    }
                }
            }
        }
    }

    VERBOSE("────────────────────────────────────────────\n");
}

int Load_XM(MAS_Module *mod)
{
    memset(mod, 0, sizeof(MAS_Module));

    mod->old_effects = true;
    mod->xm_mode = true;
    mod->global_volume = 64;
    mod->old_mode = false;

    if ((read32() != ID4('E', 'x', 't', 'e')) ||
        (read32() != ID4('n', 'd', 'e', 'd')) ||
        (read32() != ID4(' ', 'M', 'o', 'd')) ||
        (read32() != ID4('u', 'l', 'e', ':')) ||
        (read8() != ' '))
    {
        return ERR_INVALID_MODULE;
    }

    for (int x = 0; x < 20; x++)
        mod->title[x] = read8();

    VERBOSE("────────────────────────────────────────────\n");
    VERBOSE("Loading XM, \"%s\"\n", mod->title);

    if (read8() != 0x1a)
        return ERR_INVALID_MODULE;

    skip8(20); // tracker name

    u16 xm_version = read16();
    u32 xm_headsize = read32();

    mod->order_count = read16();
    if (mod->order_count >= MAS_MAX_PATTERN_ORDERS)
    {
        ERROR("The file has too many pattern orders, it has been clamped. %u >= %u\n",
              mod->order_count, MAS_MAX_PATTERN_ORDERS);
        mod->order_count = MAS_MAX_PATTERN_ORDERS - 1;
    }

    mod->restart_pos = read16();
    if (mod->restart_pos >= mod->order_count)
    {
        WARNING("Restart position is after the end of the song. "
                "It has been set to 0. %u >= %u\n",
                mod->restart_pos, mod->order_count);
        mod->restart_pos = 0;
    }

    u16 xm_nchannels = read16();
    if (xm_nchannels > MAS_MAX_CHANNELS)
    {
        ERROR("The file has too many channels: %u > %u\n",
              xm_nchannels, MAS_MAX_CHANNELS - 1);
        return ERR_MANYCHANNELS;
    }

    mod->patt_count = read16();
    mod->inst_count = read16();

    mod->freq_mode = read16() & 1 ? true : false; // flags

    mod->initial_speed = read16();
    mod->initial_tempo = read16();

    VERBOSE("Version....%i.%i\n", xm_version >> 8 & 0xFF, xm_version & 0xFF);
    VERBOSE("Length.....%i\n", mod->order_count);
    VERBOSE("Restart....%i\n", mod->restart_pos);
    VERBOSE("Channels...%i\n", xm_nchannels);
    VERBOSE("#Patterns..%i\n", mod->patt_count);
    VERBOSE("#Instr.....%i\n", mod->inst_count);
    VERBOSE("Freq Mode..%s\n", mod->freq_mode ? "Linear" : "Amiga");
    VERBOSE("Speed......%i\n", mod->initial_speed);
    VERBOSE("Tempo......%i\n", mod->initial_tempo);

    for (int x = 0; x < 32; x++)
    {
        mod->channel_volume[x] = 64;
        mod->channel_panning[x] = 128;
    }

    VERBOSE("────────────────────────────────────────────\n");
    VERBOSE("Reading sequence...\n");

    int z;
    for (z = 0; z < 200; z++) // read order table
    {
        if (z < mod->order_count)
            mod->orders[z] = read8();
        else
        {
            read8();
            mod->orders[z] = 255;
        }
    }
    for ( ; z < 256; z++) // skip 200->255
        read8();

    file_seek_read(60 + xm_headsize, SEEK_SET); // or maybe 60..

    VERBOSE("────────────────────────────────────────────\n");
    VERBOSE("Loading patterns...\n");
    VERBOSE("────────────────────────────────────────────\n");

    mod->patterns = (Pattern*)calloc(mod->patt_count, sizeof(Pattern));
    for (int x = 0; x < mod->patt_count; x++)
    {
        VERBOSE(" * %2i\n", x);

        Load_XM_Pattern(&mod->patterns[x], xm_nchannels, x);

        // Load_XM_Pattern() moves the file pointer to the start of the
        // following pattern, so we can't just skip the processing step.
        if (!Pattern_Is_Used(mod, x))
        {
            VERBOSE("Pattern %u is unused. Cleared.\n", x);
            Empty_XM_Pattern(&mod->patterns[x]);
        }
    }

    mod->instruments = (Instrument*)calloc(mod->inst_count, sizeof(Instrument));
    mod->samples = (Sample *)calloc(256, sizeof(Sample));

    u8 next_sample = 0;

    VERBOSE("────────────────────────────────────────────\n");
    VERBOSE("Loading instruments...\n");
    VERBOSE("┌─────┬───────┬────────┬────────────────────────┐\n");
    VERBOSE("│INDEX│SAMPLES│ENVELOPE│          NAME          │\n");
    VERBOSE("├─────┼───────┼────────┼────────────────────────┤\n");

    for (int x = 0; x < mod->inst_count; x++)
    {
        Load_XM_Instrument(&mod->instruments[x], mod, &next_sample, x + 1);
    }

    VERBOSE("└─────┴───────┴────────┴────────────────────────┘\n");

    mod->samp_count = next_sample;

    XM_SanitizePatterns(mod);
    Sanitize_Module(mod);

    return ERR_NONE;
}
