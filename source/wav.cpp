// SPDX-License-Identifier: ISC
//
// Copyright (c) 2008 Mukunda Johnson (mukunda@maxmod.org)

/****************************************************************************
 *                ____ ___  ____ __  ______ ___  ____  ____/ /              *
 *               / __ `__ \/ __ `/ |/ / __ `__ \/ __ \/ __  /               *
 *              / / / / / / /_/ />  </ / / / / / /_/ / /_/ /                *
 *             /_/ /_/ /_/\__,_/_/|_/_/ /_/ /_/\____/\__,_/                 *
 *                                                                          *
 ****************************************************************************/

// WAV file loader

#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "files.h"
#include "log.h"
#include "mas.h"
#include "wav.h"
#include "simple.h"
#include "samples.h"
#include "systems.h"

#define ID4(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))

int Load_WAV(Sample *samp, bool force_compress)
{
    VERBOSE("Loading WAV file...\n");

    // initialize data
    memset(samp, 0, sizeof(Sample));

    int file_size = file_tell_size();

    read32(); // "RIFF"
    read32(); // filesize-8
    read32(); // "WAVE"

    unsigned int bit_depth = 8;
    unsigned int hasformat = 0;
    unsigned int hasdata = 0;
    unsigned int num_channels = 0;
    unsigned int smpl_chunk_pos = 0;

    while (1)
    {
        // Stop if there aren't enough bytes for a new chunk
        ssize_t remaining_bytes = file_size - file_tell_read();
        if (remaining_bytes < 0)
        {
            VERBOSE("File cursor went over the limit by %d bytes\n",
                    -remaining_bytes);
            break;
        }
        else if (remaining_bytes == 0)
        {
            // Normal exit
            break;
        }
        else if (remaining_bytes < 8)
        {
            VERBOSE("File has %d padding bytes\n", remaining_bytes);
            break;
        }

        // read chunk code and length
        unsigned int chunk_code = read32();
        unsigned int chunk_size = read32();

        unsigned int next_chunk_start = file_tell_read() + chunk_size;
        next_chunk_start = (next_chunk_start + 1) & ~1; // Align to 16 bit

        // parse chunk code
        switch (chunk_code)
        {
            case ID4('f', 'm', 't', ' '): // format chunk
            {
                if (chunk_size < 0x10)
                {
                    ERROR("'fmt ' chunk is too small.\n");
                    return LOADWAV_UNKNOWN_COMP;
                }

                // Check format code (1 = PCM, 2 = ADPCM)
                u16 format = read16();

                if (format == 2) // TODO: Support ADPCM?
                {
                    ERROR("Unsupported ADPCM WAV file. Only PCM format is supported.\n");
                    return LOADWAV_UNKNOWN_COMP;
                }
                else if (format != 1)
                {
                    ERROR("Unsupported WAV format %d. Only PCM format is supported.\n",
                          format);
                    return LOADWAV_UNKNOWN_COMP;
                }

                // read # of channels
                num_channels = read16();

                // read sampling frequency
                samp->frequency = read32();

                // skip average something, wBlockAlign
                read32();
                read16();

                // get bit depth, catch unsupported values
                bit_depth = read16();
                if (bit_depth != 8 && bit_depth != 16)
                {
                    VERBOSE("Unsupported bit-depth.\n");
                    return LOADWAV_UNSUPPORTED_BD;
                }

                if (bit_depth == 16)
                    samp->format |= SAMPF_16BIT;

                // print verbose data
                VERBOSE("Sample Rate...%i\n", samp->frequency);
                VERBOSE("Bit Depth.....%i-bit\n", bit_depth);

                // skip the rest of the chunk (if any)
                if ((chunk_size - 0x10) > 0)
                    VERBOSE("Unexpected data after 'fmt ' chunk\n");

                hasformat = 1;
                break;
            }

            case ID4('d', 'a', 't', 'a'): // data chunk
            {
                if (!hasformat)
                    return LOADWAV_CORRUPT;

                VERBOSE("Loading Sample Data...\n");

                // clip chunk size against end of file (for some borked wavs...)
                {
                    size_t br = file_size - file_tell_read();
                    chunk_size = chunk_size > br ? br : chunk_size;
                }

                samp->sample_length = chunk_size / (bit_depth / 8) / num_channels;
                samp->data = Sample_AllocateData(samp->sample_length);

                size_t total_size_read = samp->sample_length * num_channels * (bit_depth / 8);
                if (total_size_read != chunk_size)
                {
                    VERBOSE("Data chunk size mismatch: Read %u (expected %u)\n",
                            total_size_read, chunk_size);
                }

                // read sample data
                Sample_ReadData_WAV(samp->data, samp->sample_length, bit_depth, num_channels);

                hasdata = 1;

                break;
            }

            case ID4('s', 'm', 'p', 'l'): // sampler chunk
            {
                smpl_chunk_pos = file_tell_read();
                break;
            }

            case ID4('c', 'u', 'e', ' '): // Some common chunks
            case ID4('C', 'S', 'E', 'T'):
            case ID4('i', 'd', '3', ' '):
            case ID4('L', 'I', 'S', 'T'):
            case ID4('x', 't', 'r', 'a'):
            {
                // TODO: Print some of the information like author/tool in verbose mode?
                VERBOSE("Ignored chunk '%c%c%c%c' (0x%08X)\n",
                        (chunk_code >> 0) & 0xFF,
                        (chunk_code >> 8) & 0xFF,
                        (chunk_code >> 16) & 0xFF,
                        (chunk_code >> 24) & 0xFF,
                        chunk_code);
                break;
            }

            default:
            {
                WARNING("Unknown chunk '%c%c%c%c' (0x%08X)\n",
                        (chunk_code >> 0) & 0xFF,
                        (chunk_code >> 8) & 0xFF,
                        (chunk_code >> 16) & 0xFF,
                        (chunk_code >> 24) & 0xFF,
                        chunk_code);
                break;
            }
        }

        // Jump to the start of next chunk regardless of what the parser has done
        file_seek_read(next_chunk_start, SEEK_SET);
    }

    // sampler chunk is processed last because it depends on the sample length being known.
    if (smpl_chunk_pos)
    {
        file_seek_read(smpl_chunk_pos, SEEK_SET);

        skip8(4   // manufacturer
              + 4 // product
              + 4 // sample period
              + 4 // midi unity note
              + 4 // midi pitch fraction
              + 4 // smpte format
              + 4 // smpte offset
        );

        int num_sample_loops = read32();

        read32(); // sample data

        // check for sample looping data
        if (num_sample_loops)
        {
            read32(); // cue point ID
            int loop_type = read32();

            if (loop_type < 2)
            {
                // sample    | internal
                // 0=forward | 1
                // 1=bidi    | 2
                samp->loop_type = loop_type + 1;
                samp->loop_start = read32();
                samp->loop_end = read32();

                // clip loop start against sample length
                if (samp->loop_end > samp->sample_length)
                {
                    samp->loop_end = samp->sample_length;
                }

                // disable tiny loop
                // catch invalid loop
                if ((samp->loop_start > samp->sample_length) ||
                    (samp->loop_end - samp->loop_start < 16))
                {
                    samp->loop_type = 0;
                    samp->loop_start = 0;
                    samp->loop_end = 0;
                }

                samp->original_sample_length = samp->sample_length;
                samp->original_loop_start = samp->loop_start;
                samp->original_loop_end = samp->loop_end;

                // ignore fractional
                // ignore play count

                const char *loop_type_str = "Unknown";
                if (samp->loop_type == 0)
                    loop_type_str = "normal";
                else if (samp->loop_type == 1)
                    loop_type_str = "ping-pong";

                VERBOSE("Loop information found: Type %s (%u -> %d)\n",
                        loop_type_str, samp->loop_start, samp->loop_end);
            }
            else
            {
                // TODO: Support type 2 (backward loop)
                ERROR("Loop information found: Unsupported type: %u\n", loop_type);
            }
        }
    }

    if (hasformat && hasdata)
    {
        if (force_compress)
            samp->format |= SAMPF_COMP;
        FixSample(samp, INPUT_TYPE_WAV);
        return LOADWAV_OK;
    }
    else
    {
        return LOADWAV_CORRUPT;
    }
}
