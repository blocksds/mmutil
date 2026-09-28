// SPDX-License-Identifier: ISC
//
// Copyright (c) 2008, Mukunda Johnson (mukunda@maxmod.org)
// Copyright (c) 2026, Antonio Niño Díaz

/****************************************************************************
 *                ____ ___  ____ __  ______ ___  ____  ____/ /              *
 *               / __ `__ \/ __ `/ |/ / __ `__ \/ __ \/ __  /               *
 *              / / / / / / /_/ />  </ / / / / / /_/ / /_/ /                *
 *             /_/ /_/ /_/\__,_/_/|_/_/ /_/ /_/\____/\__,_/                 *
 *                                                                          *
 ****************************************************************************/

// MAXMOD SOUNDBANK

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include "errors.h"
#include "defs.h"
#include "files.h"
#include "mas.h"
#include "mod.h"
#include "s3m.h"
#include "xm.h"
#include "it.h"
#include "wav.h"
#include "simple.h"
#include "version.h"
#include "systems.h"
#include "samplefix.h"

FILE *F_SAMP = NULL;
FILE *F_SONG = NULL;

FILE *F_HEADER = NULL;

u16 MSL_NSAMPS;
u16 MSL_NSONGS;

typedef struct
{
    uint32_t id;
    std::string name;
}
MSL_Name_Entry;

static bool MSL_Name_Entry_Compare(MSL_Name_Entry a, MSL_Name_Entry b)
{
    return a.id < b.id;
}

static std::vector<MSL_Name_Entry> msl_sample_names = {};
static std::vector<MSL_Name_Entry> msl_module_names = {};

char str_msl[256];

static char TMP_SAMP[] = "mm_samp_tmp.XXXXXXX";
static char TMP_SONG[] = "mm_song_tmp.XXXXXXX";

static void MSL_PrintDefinition(const char *filename, u16 id, const char *prefix);

#define SAMPLE_HEADER_SIZE (12 + ((target_system == SYSTEM_NDS) ? 4 : 0))

void MSL_Erase(void)
{
    MSL_NSAMPS = 0;
    MSL_NSONGS = 0;
    file_delete(TMP_SAMP);
    file_delete(TMP_SONG);

    msl_sample_names = {};
    msl_module_names = {};
}

u16 MSL_AddSample(Sample *samp)
{
    file_open_write_end(TMP_SAMP);

    u32 sample_length = samp->sample_length;

    write32(((samp->format & SAMPF_16BIT) ? sample_length * 2 : sample_length)
            + SAMPLE_HEADER_SIZE + 4); // +4 for sample padding
    write8((target_system == SYSTEM_GBA) ? MAS_TYPE_SAMPLE_GBA : MAS_TYPE_SAMPLE_NDS);
    write8(MAS_VERSION);
    write8(samp->filename[0] == '#' ? 1 : 0); // TODO: This isn't used by Maxmod
    write8(BYTESMASHER);

    Write_SampleData(samp);

    file_close_write();

    MSL_NSAMPS++;

    return MSL_NSAMPS - 1;
}

u16 MSL_AddSampleC(Sample *samp)
{
    int fsize = file_size(TMP_SAMP);
    if (fsize == 0)
    {
        return MSL_AddSample(samp);
    }
    F_SAMP = fopen(TMP_SAMP, "rb");
    fseek(F_SAMP, 0, SEEK_SET);

    int samp_id = 0;

    while (ftell(F_SAMP) < fsize)
    {
        u32 h_filesize = read32f(F_SAMP);
        read32f(F_SAMP);
        u32 samp_len = read32f(F_SAMP);
        u32 samp_llen = read32f(F_SAMP);
        u8 sformat = read8f(F_SAMP);
        skip8f(3, F_SAMP);

        u8 target_sformat;
        if (target_system == SYSTEM_NDS)
        {
            target_sformat = sample_dsformat(samp);
            skip8f(4, F_SAMP);
        }
        else
        {
            target_sformat = SAMP_FORMAT_U8;
        }

        u32 st;
        bool samp_match = true;

        if (samp->sample_length == samp_len &&
            (samp->loop_type ? samp->loop_end-samp->loop_start : 0xFFFFFFFF) == samp_llen &&
            sformat == target_sformat)
        {
            // verify sample data
            if (samp->format & SAMPF_16BIT)
            {
                for (st = 0; st < samp_len; st++)
                {
                    if (read16f(F_SAMP) != ((u16*)samp->data)[st])
                    {
                        samp_match = false;
                        break;
                    }
                }
            }
            else
            {
                for (st = 0; st < samp_len; st++)
                {
                    if (read8f(F_SAMP) != ((u8*)samp->data)[st])
                    {
                        samp_match = false;
                        break;
                    }
                }
            }

            if (samp_match)
            {
                fclose(F_SAMP);
                return samp_id;
            }
            else
            {
                // +4 to skip padding
                skip8f((h_filesize - SAMPLE_HEADER_SIZE) - (st + 1), F_SAMP);
            }
        }
        else
        {
            skip8f(h_filesize- SAMPLE_HEADER_SIZE, F_SAMP); // +4 to skip padding
        }
        samp_id++;
    }

    fclose(F_SAMP);
    return MSL_AddSample(samp);
}

u16 MSL_AddModule(MAS_Module *mod)
{
    // ADD SAMPLES
    for (int x = 0; x < mod->samp_count; x++)
    {
        int samp_id = MSL_AddSampleC(&mod->samples[x]);

        if (mod->samples[x].filename[0] == '#')
            MSL_PrintDefinition(mod->samples[x].filename + 1, (u16)samp_id, "SFX_");

        mod->samples[x].msl_index = samp_id;
    }

    file_open_write_end(TMP_SONG);
    Write_MAS(mod, false, true);
    file_close_write();

    MSL_NSONGS++;

    return MSL_NSONGS - 1;
}

static std::vector<u8> msl_names_dict = {};

static void MSL_CreateDictionary(void)
{
    // Sort all names by their ID

    std::sort(msl_sample_names.begin(), msl_sample_names.end(), MSL_Name_Entry_Compare);
    std::sort(msl_module_names.begin(), msl_module_names.end(), MSL_Name_Entry_Compare);

    // Sample names

    std::vector<u8> samples_dict = {};

    for (auto entry : msl_sample_names)
    {
        size_t len = entry.name.length();

        // We need to save at least one terminator, plus padding
        size_t len_with_padding = ((len + 1) + 3) & ~3;

        if (len_with_padding > 255)
        {
            printf("Sample name too long (%zu) [%s]", len, entry.name.c_str());
            exit(EXIT_FAILURE);
        }

        if (entry.id > 0xFFFFFF)
        {
            printf("Sample ID too big (%u > %u)", entry.id, 0xFFFFFF);
            exit(EXIT_FAILURE);
        }

        samples_dict.push_back((entry.id >> 0) & 0xFF);
        samples_dict.push_back((entry.id >> 8) & 0xFF);
        samples_dict.push_back((entry.id >> 16) & 0xFF);
        samples_dict.push_back(len_with_padding);

        size_t i = 0;
        for ( ; i < len; i++)
            samples_dict.push_back(entry.name[i]);
        for ( ; i < len_with_padding; i++)
            samples_dict.push_back(0);
    }

    samples_dict.push_back(0); // End list
    samples_dict.push_back(0);
    samples_dict.push_back(0);
    samples_dict.push_back(0);

    // Module names

    std::vector<u8> modules_dict = {};

    for (auto entry : msl_module_names)
    {
        size_t len = entry.name.length();

        // We need to save at least one terminator, plus padding
        size_t len_with_padding = ((len + 1) + 3) & ~3;

        if (len_with_padding > 255)
        {
            printf("Module name too long (%zu) [%s]", len, entry.name.c_str());
            exit(EXIT_FAILURE);
        }

        if (entry.id > 0xFFFFFF)
        {
            printf("Module ID too big (%u > %u)", entry.id, 0xFFFFFF);
            exit(EXIT_FAILURE);
        }

        modules_dict.push_back((entry.id >> 0) & 0xFF);
        modules_dict.push_back((entry.id >> 8) & 0xFF);
        modules_dict.push_back((entry.id >> 16) & 0xFF);
        modules_dict.push_back(len_with_padding);

        size_t i = 0;
        for ( ; i < len; i++)
            modules_dict.push_back(entry.name[i]);
        for ( ; i < len_with_padding; i++)
            modules_dict.push_back(0);
    }

    modules_dict.push_back(0); // End list
    modules_dict.push_back(0);
    modules_dict.push_back(0);
    modules_dict.push_back(0);

    // Build dictionary header

    u32 samples_dict_size = samples_dict.size();
    u32 modules_dict_size = modules_dict.size();

    std::vector<u8> header = {
        (u8)(samples_dict_size >> 0),
        (u8)(samples_dict_size >> 8),
        (u8)(samples_dict_size >> 16),
        (u8)(samples_dict_size >> 24),

        (u8)(modules_dict_size >> 0),
        (u8)(modules_dict_size >> 8),
        (u8)(modules_dict_size >> 16),
        (u8)(modules_dict_size >> 24),
    };

    // Generate final dictionary

    msl_names_dict = header;
    msl_names_dict.insert(msl_names_dict.end(), samples_dict.begin(), samples_dict.end());
    msl_names_dict.insert(msl_names_dict.end(), modules_dict.begin(), modules_dict.end());
}

static void MSL_ExportDictionary(void)
{
    for (uint8_t i : msl_names_dict)
        write8(i);
}

static void MSL_Export(const char *filename, bool export_dictionary)
{
    file_open_write(filename);
    write16(MSL_NSAMPS);
    write16(MSL_NSONGS);
    write8('*');
    write8('m');
    write8('a');
    write8('x');
    write8('m');
    write8('o');
    write8('d');
    write8('*');

    u32 *parap_samp = (u32*)malloc(MSL_NSAMPS * sizeof(u32));
    u32 *parap_song = (u32*)malloc(MSL_NSONGS * sizeof(u32));

    // reserve space for parapointers
    for (u32 x = 0; x < MSL_NSAMPS; x++) // List of samples
        write32(0xAAAAAAAA);
    for (u32 x = 0; x < MSL_NSONGS; x++) // List of modules
        write32(0xAAAAAAAA);
    write32(0xAAAAAAAA); // Dictionary of module and sample names

    // Generate dictionary of sample and module names

    u32 parap_names_dict = 0xFFFFFFFF; // 0xFFFFFFFF = not present

    if (export_dictionary)
    {
        parap_names_dict = file_tell_write();
        MSL_ExportDictionary();
    }

    // copy samples
    file_open_read(TMP_SAMP);
    for (u32 x = 0; x < MSL_NSAMPS; x++)
    {
        align32();
        parap_samp[x] = file_tell_write();

        u32 file_size = read32();
        write32(file_size);
        for (u32 y = 0; y < file_size + 4; y++)
            write8(read8());
    }
    file_close_read();

    file_open_read(TMP_SONG);
    for (u32 x = 0; x < MSL_NSONGS; x++)
    {
        align32();
        parap_song[x] = file_tell_write();

        u32 file_size = read32();
        write32(file_size);
        for (u32 y = 0; y < file_size+4; y++)
            write8(read8());
    }
    file_close_read();

    // Go back to the start to write the parapointers

    file_seek_write(0x0C, SEEK_SET);
    for (u32 x = 0; x < MSL_NSAMPS; x++)
        write32(parap_samp[x]);
    for (u32 x = 0; x < MSL_NSONGS; x++)
        write32(parap_song[x]);
    write32(parap_names_dict);

    file_close_write();

    if (parap_samp)
        free(parap_samp);
    if (parap_song)
        free(parap_song);

}

static void MSL_PrintDefinition(const char* filename, u16 id, const char* prefix)
{
    char newtitle[64];
    int x, s = 0;

    if (filename[0] == 0) // empty string
        return;

    for (x = 0; x < (int)strlen(filename); x++)
    {
        if (filename[x] == '\\' || filename[x] == '/')
            s = x + 1;
    }

    {
        MSL_Name_Entry entry;
        entry.id = id;
        entry.name = std::string(filename + s);

        if (strcmp(prefix, "SFX_") == 0)
            msl_sample_names.push_back(entry);
        else if (strcmp(prefix, "MOD_") == 0)
            msl_module_names.push_back(entry);
    }

    for (x = s; x < (int)strlen(filename); x++)
    {
        if (filename[x] != '.')
        {
            newtitle[x - s] = toupper(filename[x]);
            if (newtitle[x - s] >= ' ' && newtitle[x-s] <= '/')
                newtitle[x - s] = '_';
            if (newtitle[x - s] >= ':' && newtitle[x-s] <= '@')
                newtitle[x - s] = '_';
            if (newtitle[x - s] >= '[' && newtitle[x-s] <= '`')
                newtitle[x - s] = '_';
            if (newtitle[x - s] >= '{')
                newtitle[x - s] = '_';
        }
        else
        {
            break;
        }
    }
    newtitle[x - s] = 0;

    if (F_HEADER)
    {
        fprintf(F_HEADER, "#define %s%s    %i\r\n", prefix, newtitle, id);
    }
}

void MSL_LoadFile(char *filename, bool verbose)
{
    Sample wav;
    MAS_Module mod;

    if (file_open_read(filename))
    {
        printf("Cannot open %s for reading! Skipping.\n", filename);
        return;
    }

    int f_ext = get_ext(filename);
    switch (f_ext)
    {
        case INPUT_TYPE_MOD:
            if (Load_MOD(&mod, verbose))
                exit(EXIT_FAILURE);
            MSL_PrintDefinition(filename, MSL_AddModule(&mod), "MOD_");
            Delete_Module(&mod);
            break;
        case INPUT_TYPE_S3M:
            if (Load_S3M(&mod, verbose))
                exit(EXIT_FAILURE);
            MSL_PrintDefinition(filename, MSL_AddModule(&mod), "MOD_");
            Delete_Module(&mod);
            break;
        case INPUT_TYPE_XM:
            if (Load_XM(&mod, verbose))
                exit(EXIT_FAILURE);
            MSL_PrintDefinition(filename, MSL_AddModule(&mod), "MOD_");
            Delete_Module(&mod);
            break;
        case INPUT_TYPE_IT:
            if (Load_IT(&mod, verbose))
                exit(EXIT_FAILURE);
            MSL_PrintDefinition(filename, MSL_AddModule(&mod), "MOD_");
            Delete_Module(&mod);
            break;
        case INPUT_TYPE_WAV:
            if (Load_WAV(&wav, verbose, true))
                exit(EXIT_FAILURE);
            wav.filename[0] = '#'; // set SFX flag (for demo)
            MSL_PrintDefinition(filename, MSL_AddSample(&wav), "SFX_");
            free(wav.data);
            break;
        default:
            // print error/warning
            printf("Unknown file %s...\n", filename);
    }

    file_close_read();
}

int MSL_CreateTemporaryFiles(bool verbose)
{
    int fd_samp = mkstemp(TMP_SAMP);
    if (fd_samp == -1)
    {
        printf("Can't generate temporary file for samples\n");
        perror("mkstemp");
        return ERR_NOWRITE;
    }
    close(fd_samp);

    int fd_song = mkstemp(TMP_SONG);
    if (fd_song == -1)
    {
        printf("Can't generate temporary file for songs\n");
        perror("mkstemp");
        return ERR_NOWRITE;
    }
    close(fd_song);

    // Make sure that the files are deleted when the program ends even if it
    // ends through a call to exit().
    atexit(MSL_Erase);

    if (verbose)
    {
        printf("Temporary files: %s and %s\n", TMP_SAMP, TMP_SONG);
    }

    return ERR_NONE;
}

int MSL_Create(char *argv[], int argc, const char *output, const char *header,
               bool export_dictionary, bool verbose)
{
    MSL_Erase();

    str_msl[0] = 0;

    F_HEADER = NULL;
    if (header)
    {
        F_HEADER = fopen(header, "wb");
        if (F_HEADER == NULL)
        {
            printf("Can't open output header file: %s\n", header);
            return ERR_NOWRITE;
        }
    }

    int ret = MSL_CreateTemporaryFiles(verbose);
    if (ret != ERR_NONE)
        return ret;

    for (int x = 1; x < argc; x++)
    {
        if (argv[x][0] == '-')
        {
            // Skip anything that isn't an input file
        }
        else
        {
            MSL_LoadFile(argv[x], verbose);
        }
    }

    if (export_dictionary)
        MSL_CreateDictionary();

    MSL_Export(output, export_dictionary);

    if (F_HEADER)
    {
        fprintf(F_HEADER, "#define MSL_NSONGS    %i\r\n", MSL_NSONGS);
        fprintf(F_HEADER, "#define MSL_NSAMPS    %i\r\n", MSL_NSAMPS);
        fprintf(F_HEADER, "#define MSL_BANKSIZE    %i\r\n", MSL_NSAMPS + MSL_NSONGS);
        fclose(F_HEADER);
        F_HEADER = NULL;
    }

    return ERR_NONE;
}
