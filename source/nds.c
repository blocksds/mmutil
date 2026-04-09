// SPDX-License-Identifier: ISC
//
// Copyright (c) 2026, Antonio Niño Díaz

/****************************************************************************
 *                ____ ___  ____ __  ______ ___  ____  ____/ /              *
 *               / __ `__ \/ __ `/ |/ / __ `__ \/ __ \/ __  /               *
 *              / / / / / / /_/ />  </ / / / / / /_/ / /_/ /                *
 *             /_/ /_/ /_/\__,_/_/|_/_/ /_/ /_/\____/\__,_/                 *
 *                                                                          *
 ****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "defs.h"
#include "files.h"
#include "mas.h"
#include "msl.h"

#ifndef PATH_MAX
#define PATH_MAX 2048
#endif

const u8 nds_arm7_elf[] = {
#embed "nds_arm7.elf"
};

const u8 nds_arm9_elf[] = {
#embed "nds_arm9.elf"
};

static void save_array_to_file(const char *path, const void *data, size_t data_size)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL)
    {
        printf("Failed to open: %s\n", path);
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    if (fwrite(data, 1, data_size, f) != data_size)
    {
        printf("Failed to write: %s\n", path);
        perror("fwrite");
        exit(EXIT_FAILURE);
    }

    if (fclose(f) != 0)
    {
        printf("Failed to close: %s\n", path);
        perror("fclose");
        exit(EXIT_FAILURE);
    }
}

#define DIR_TEMP        "mmutiltemp"
#define DIR_NITROFS     "mmutiltemp/nitrofs"

#define FILE_SOUNDBANK  "mmutiltemp/nitrofs/soundbank.bin"
#define FILE_ARM7_ELF   "mmutiltemp/nds_arm7.elf"
#define FILE_ARM9_ELF   "mmutiltemp/nds_arm9.elf"

static void remove_temporary_files(void)
{
    // We can only remove empty directories. Delete all files first, then child
    // directories, finally the top-level directory.

    remove(FILE_SOUNDBANK);
    remove(FILE_ARM7_ELF);
    remove(FILE_ARM9_ELF);

    remove(DIR_NITROFS);

    remove(DIR_TEMP);
}

static void get_paths_to_tools(char **ndstool_path, char **banner_arg, bool v_flag)
{
    // Look for ndstool and a ROM icon in a BlocksDS environment. If the user
    // doesn't have BlocksDS installed (for example, they have received a
    // standalone mmutil binary), try to fall back to a standalone ndstool
    // binary and a standalone icon file.

    // Get the path to the BlocksDS environment, if any.

    char *blocksds = getenv("BLOCKSDS");

    if (blocksds == NULL)
    {
        if (v_flag)
            printf("BLOCKSDS environment variable not found. Using default path.\n");

        blocksds = "/opt/blocksds/core";
    }

    if (v_flag)
        printf("BLOCKSDS=%s\n", blocksds);

    // Try to get the path for ndstool.
    //
    // 1. Look for it in a BlocksDS environment.
    // 2. If not, try to run it from the PATH.

    char ndstool_default_path[PATH_MAX];

    snprintf(ndstool_default_path, sizeof(ndstool_default_path),
             "%s/tools/ndstool/ndstool", blocksds);

    if (file_exists(ndstool_default_path))
        *ndstool_path = strdup(ndstool_default_path);
    else
        *ndstool_path = strdup("ndstool");

    // Try to find an icon for the ROM:
    //
    // 1. If the default BlocksDS icon file is found, use it.
    // 2. If not, if "icon.gif" exists in the current working directory, use it.
    // 3. If not, don't use an icon, only set the text.

    char icon_default_path[PATH_MAX];

    snprintf(icon_default_path, sizeof(icon_default_path),
             "%s/sys/icon.gif", blocksds);

    if (file_exists(icon_default_path))
    {
        char *cmd = NULL;
        int len = asprintf(&cmd, "-b %s \"NDS Demo;Maxmod;blocksds.skylyrac.net\"",
                           icon_default_path);
        if (len < 0)
        {
            printf("Failed to allocate banner command\n");
            remove_temporary_files();
            exit(EXIT_FAILURE);
        }

        *banner_arg = cmd;
    }
    else if (file_exists("icon.gif"))
    {
        *banner_arg = strdup("-b icon.gif \"NDS Demo;Maxmod;blocksds.skylyrac.net\"");
    }
    else
    {
        *banner_arg = strdup("-bt \"NDS Demo;Maxmod;blocksds.skylyrac.net\"");
    }
}

void Write_NDS(int argc, char *argv[], const char *out_path, bool v_flag)
{
    // Make sure that we can create the new files

    remove_temporary_files();

    // Create components of the NDS ROM

    if (mkdir(DIR_TEMP, 0777) != 0)
    {
        perror("mkdir(" DIR_TEMP ")");
        exit(EXIT_FAILURE);
    }

    if (mkdir(DIR_NITROFS, 0777) != 0)
    {
        perror("mkdir(" DIR_NITROFS ")");
        exit(EXIT_FAILURE);
    }

    MSL_Create(argv, argc, FILE_SOUNDBANK, 0, v_flag);

    save_array_to_file(FILE_ARM7_ELF, nds_arm7_elf, sizeof(nds_arm7_elf));
    save_array_to_file(FILE_ARM9_ELF, nds_arm9_elf, sizeof(nds_arm9_elf));

    // Run ndstool to generate the NDS ROM

    if (v_flag)
        printf("Generating NDS Demo ROM...\n");

    char cmd[2048];

    {
        char *ndstool_path = NULL;
        char *banner_arg = NULL;

        get_paths_to_tools(&ndstool_path, &banner_arg, v_flag);

        snprintf(cmd, sizeof(cmd),
                 "%s -c %s %s"
                 " -7 " FILE_ARM7_ELF " -9 " FILE_ARM9_ELF
                 " -d " DIR_NITROFS,
                 ndstool_path, out_path, banner_arg);

        free(ndstool_path);
        free(banner_arg);
    }

    if (v_flag)
        printf("Running [%s]\n", cmd);

    int rc = system(cmd);
    if (rc != 0)
    {
        printf("Failed while running [%s]\n", cmd);
        remove_temporary_files();
        exit(EXIT_FAILURE);
    }

    // Cleanup

    remove_temporary_files();

    printf("Success! :D\n");
}
