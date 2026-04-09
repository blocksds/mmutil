// SPDX-License-Identifier: CC0-1.0
//
// SPDX-FileContributor: Antonio Niño Díaz, 2026

// This demo lets the user experiment with soundbanks that may not be known at
// build time. It needs to fetch the number of samples and modules at runtime.
// Also, it doesn't know the names of the samples and modules. It's only done
// like this so that this ROM can be used to create test ROMs with mmutil.

#include <stdio.h>

#include <filesystem.h>
#include <maxmod9.h>
#include <nds.h>

void wait_forever(void)
{
    while (1)
    {
        swiWaitForVBlank();
        scanKeys();
        if (keysHeld() & KEY_START)
            exit(0);
    }
}

void vbl_handler(void)
{
    setBackdropColor(RGB15(0, 0, 8));
    setBackdropColorSub(RGB15(0, 0, 8));
}

void hbl_handler(void)
{
    setBackdropColor(RGB15(0, 0, 8 + REG_VCOUNT / 16));
    setBackdropColorSub(RGB15(0, 0, 8 + REG_VCOUNT / 16));
}

int main(int argc, char **argv)
{
    // Make the repetition period a lot shorter than the default
    keysSetRepeat(30, 2);

    // Setup a nice gradient effect
    irqSet(IRQ_HBLANK, hbl_handler);
    irqSet(IRQ_VBLANK, vbl_handler);
    irqEnable(IRQ_HBLANK);

    PrintConsole topScreen;
    PrintConsole bottomScreen;

    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);

    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);

    consoleInit(&topScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    consoleInit(&bottomScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

    consoleSelect(&topScreen);

    printf("          Maxmod demo\n");
    printf("          -----------\n");
    printf("\n");

    printf("Initializing NitroFS... ");

    bool init_ok = nitroFSInit(NULL);
    if (!init_ok)
    {
        printf("\n");
        perror("nitroFSInit()");
        wait_forever();
    }

    printf("OK!\n");

    printf("Initializing Maxmod...  ");

    if (!mmInitDefault("nitro:/soundbank.bin"))
    {
        printf("\n");
        printf("Failed to init Maxmod!");
        wait_forever();
    }

    printf("OK!\n");

    mm_word module_count = mmGetModuleCount();
    mm_word sample_count = mmGetSampleCount();

    printf("\n");
    printf("Soundbank information:\n");
    printf("\n");
    printf("  Sample count: %u\n", sample_count);
    printf("  Module count: %u\n", module_count);

    consoleSetCursor(NULL, 0, 20);

    printf("UP/DOWN:    Move cursor\n");
    printf("LEFT/RIGHT: Change value\n");
    printf("\n");
    printf("START: Exit to loader");

    soundEnable();

    enum
    {
        MENU_START,

        MENU_MAXMOD_MODE = MENU_START,

        MENU_SFX_ID,
        MENU_SFX_RATE,
        MENU_SFX_VOLUME,
        MENU_SFX_PANNING,

        MENU_MOD_ID,
        MENU_MOD_TEMPO,
        MENU_MOD_PITCH,
        MENU_MOD_VOLUME,

        MENU_END = MENU_MOD_VOLUME,
    }
    menu_option = MENU_START;

    // Values selected by the user from the menu

    unsigned int selected_maxmod_mode = MM_MODE_A;

    unsigned int selected_sfx_id = 0;
    unsigned int selected_sfx_rate = 1024;
    unsigned int selected_sfx_volume = 255;
    unsigned int selected_sfx_panning = 128;

    unsigned int selected_module_id = 0;
    unsigned int selected_module_tempo = 1024;
    unsigned int selected_module_pitch = 1024;
    unsigned int selected_module_volume = 1024;

    // Values related to the sounds currently being played

    mm_sfxhand active_sfx_handle = MM_SFXHAND_INVALID;
    int active_sfx_id = -1;

    int active_module_id = -1;

    while (1)
    {
        swiWaitForVBlank();

        consoleSelect(&topScreen);

        consoleSetCursor(NULL, 0, 17);

        if ((menu_option == MENU_SFX_ID) || (menu_option == MENU_SFX_RATE) ||
            (menu_option == MENU_SFX_VOLUME) || (menu_option == MENU_SFX_PANNING))
        {
            printf("A: Start SFX      \n");
            printf("B: Stop SFX       ");
        }
        else if ((menu_option == MENU_MOD_ID) || (menu_option == MENU_MOD_TEMPO) ||
            (menu_option == MENU_MOD_PITCH) || (menu_option == MENU_MOD_VOLUME))
        {
            printf("A: Start module   \n");
            printf("B: Stop module    ");
        }
        else
        {
            printf("                  \n");
            printf("                  ");
        }

        consoleSelect(&bottomScreen);

        consoleClear();

        const char *mode_name[] =
        {
            "Hardware",
            "Interpolated",
            "Extended",
        };

#define SEL_OPTION(x) (menu_option == (x) ? '>' : ' ')

        consoleSetCursor(NULL, 0, 3);

        printf("          [Global]\n");
        printf("\n");
        printf("   %c Mixer Mode: %s\n", SEL_OPTION(MENU_MAXMOD_MODE), mode_name[selected_maxmod_mode]);
        printf("\n");
        printf("            [SFX]\n");
        printf("\n");
        printf("   %c Sample ID: %u\n", SEL_OPTION(MENU_SFX_ID), selected_sfx_id);
        printf("   %c Rate:      %u\n", SEL_OPTION(MENU_SFX_RATE), selected_sfx_rate);
        printf("   %c Volume:    %u\n", SEL_OPTION(MENU_SFX_VOLUME), selected_sfx_volume);
        printf("   %c Panning:   %u\n", SEL_OPTION(MENU_SFX_PANNING), selected_sfx_panning);
        printf("\n");
        printf("          [Module]\n");
        printf("\n");
        printf("   %c Module ID: %u\n", SEL_OPTION(MENU_MOD_ID), selected_module_id);
        printf("   %c Tempo:     %u\n", SEL_OPTION(MENU_MOD_TEMPO), selected_module_tempo);
        printf("   %c Pitch:     %u\n", SEL_OPTION(MENU_MOD_PITCH), selected_module_pitch);
        printf("   %c Volume:    %u\n", SEL_OPTION(MENU_MOD_VOLUME), selected_module_volume);

        scanKeys();

        uint16_t keys_down = keysDown();
        uint16_t keys_repeat = keysDownRepeat();

        if (keys_repeat & KEY_LEFT)
        {
            if (menu_option == MENU_MAXMOD_MODE)
            {
                if (selected_maxmod_mode == MM_MODE_A)
                    selected_maxmod_mode = MM_MODE_C;
                else
                    selected_maxmod_mode--;

                mmSelectMode(selected_maxmod_mode);
            }
            else if (menu_option == MENU_SFX_ID)
            {
                if (selected_sfx_id == 0)
                    selected_sfx_id = sample_count - 1;
                else
                    selected_sfx_id--;
            }
            else if (menu_option == MENU_SFX_RATE)
            {
                selected_sfx_rate -= 16;

                if (selected_sfx_rate < 512)
                    selected_sfx_rate = 512;

                if (active_sfx_handle != -1)
                    mmEffectRate(active_sfx_handle, selected_sfx_rate);
            }
            else if (menu_option == MENU_SFX_VOLUME)
            {
                if (selected_sfx_volume > 0)
                    selected_sfx_volume--;

                if (active_sfx_handle != -1)
                    mmEffectVolume(active_sfx_handle, selected_sfx_volume);
            }
            else if (menu_option == MENU_SFX_PANNING)
            {
                if (selected_sfx_panning > 0)
                    selected_sfx_panning--;

                if (active_sfx_handle != -1)
                    mmEffectPanning(active_sfx_handle, selected_sfx_panning);
            }
            else if (menu_option == MENU_MOD_ID)
            {
                if (selected_module_id == 0)
                    selected_module_id = module_count - 1;
                else
                    selected_module_id--;
            }
            else if (menu_option == MENU_MOD_TEMPO)
            {
                selected_module_tempo -= 16;

                if (selected_module_tempo < 512)
                    selected_module_tempo = 512;

                mmSetModuleTempo(selected_module_tempo);
            }
            else if (menu_option == MENU_MOD_PITCH)
            {
                selected_module_pitch -= 16;

                if (selected_module_pitch < 512)
                    selected_module_pitch = 512;

                mmSetModulePitch(selected_module_pitch);
            }
            else if (menu_option == MENU_MOD_VOLUME)
            {
                if (selected_module_volume > 4)
                    selected_module_volume -= 4;
                else
                    selected_module_volume = 0;

                mmSetModuleVolume(selected_module_volume);
            }
        }
        else if (keys_repeat & KEY_RIGHT)
        {
            if (menu_option == MENU_MAXMOD_MODE)
            {
                if (selected_maxmod_mode == MM_MODE_C)
                    selected_maxmod_mode = MM_MODE_A;
                else
                    selected_maxmod_mode++;

                mmSelectMode(selected_maxmod_mode);
            }
            else if (menu_option == MENU_SFX_ID)
            {
                selected_sfx_id++;
                if (selected_sfx_id == sample_count)
                    selected_sfx_id = 0;
            }
            else if (menu_option == MENU_SFX_RATE)
            {
                selected_sfx_rate += 16;

                if (selected_sfx_rate > 2048)
                    selected_sfx_rate = 2047;

                if (active_sfx_handle != -1)
                    mmEffectRate(active_sfx_handle, selected_sfx_rate);
            }
            else if (menu_option == MENU_SFX_VOLUME)
            {
                if (selected_sfx_volume < 255)
                    selected_sfx_volume++;

                if (active_sfx_handle != -1)
                    mmEffectVolume(active_sfx_handle, selected_sfx_volume);
            }
            else if (menu_option == MENU_SFX_PANNING)
            {
                if (selected_sfx_panning < 255)
                    selected_sfx_panning++;

                if (active_sfx_handle != -1)
                    mmEffectPanning(active_sfx_handle, selected_sfx_panning);
            }
            else if (menu_option == MENU_MOD_ID)
            {
                selected_module_id++;
                if (selected_module_id == module_count)
                    selected_module_id = 0;
            }
            else if (menu_option == MENU_MOD_TEMPO)
            {
                selected_module_tempo += 16;

                if (selected_module_tempo >= 2048)
                    selected_module_tempo = 2048;

                mmSetModuleTempo(selected_module_tempo);
            }
            else if (menu_option == MENU_MOD_PITCH)
            {
                selected_module_pitch += 16;

                if (selected_module_pitch >= 2048)
                    selected_module_pitch = 2048;

                mmSetModulePitch(selected_module_pitch);
            }
            else if (menu_option == MENU_MOD_VOLUME)
            {
                if (selected_module_volume < (1024 - 4))
                    selected_module_volume += 4;
                else
                    selected_module_volume = 1024;

                mmSetModuleVolume(selected_module_volume);
            }
        }

        if (keys_repeat & KEY_DOWN)
        {
            if (menu_option == MENU_END)
                menu_option = MENU_START;
            else
                menu_option++;
        }
        else if (keys_repeat & KEY_UP)
        {
            if (menu_option == MENU_START)
                menu_option = MENU_END;
            else
                menu_option--;
        }

        if (keys_down & KEY_A)
        {
            if ((menu_option == MENU_SFX_ID) || (menu_option == MENU_SFX_RATE) ||
                (menu_option == MENU_SFX_VOLUME) || (menu_option == MENU_SFX_PANNING))
            {
                mmEffectCancel(active_sfx_handle);

                if (active_sfx_id != -1)
                    mmUnloadEffect(active_sfx_id);

                active_sfx_id = selected_sfx_id;

                if (mmLoadEffect(active_sfx_id) != 0)
                {
                    printf("Failed to load effect %d", active_sfx_id);
                    wait_forever();
                }

                mm_sound_effect effect =
                {
                    .id = active_sfx_id,
                    .rate = selected_sfx_rate,
                    .handle = 0,
                    .volume = selected_sfx_volume,
                    .panning = selected_sfx_panning
                };

                active_sfx_handle = mmEffectEx(&effect);
                if (active_sfx_handle == MM_SFXHAND_INVALID)
                {
                    printf("Failed to play effect %d", active_sfx_id);
                    wait_forever();
                }
            }
            else if ((menu_option == MENU_MOD_ID) || (menu_option == MENU_MOD_TEMPO) ||
                     (menu_option == MENU_MOD_PITCH) || (menu_option == MENU_MOD_VOLUME))
            {
                mmStop();

                if (active_module_id != -1)
                    mmUnload(active_module_id);

                active_module_id = selected_module_id;
                if (mmLoad(active_module_id) != 0)
                {
                    printf("Failed to load module %d", active_module_id);
                    wait_forever();
                }

                mmStart(active_module_id, MM_PLAY_LOOP);
                mmSetModuleTempo(selected_module_tempo);
                mmSetModulePitch(selected_module_pitch);
                mmSetModuleVolume(selected_module_volume);
            }
        }
        else if (keys_down & KEY_B)
        {
            if ((menu_option == MENU_SFX_ID) || (menu_option == MENU_SFX_RATE) ||
                (menu_option == MENU_SFX_VOLUME) || (menu_option == MENU_SFX_PANNING))
            {
                mmEffectCancel(active_sfx_handle);

                if (active_sfx_id != -1)
                    mmUnloadEffect(active_sfx_id);

                active_sfx_id = -1;
                active_sfx_handle = MM_SFXHAND_INVALID;
            }
            else if ((menu_option == MENU_MOD_ID) || (menu_option == MENU_MOD_TEMPO) ||
                     (menu_option == MENU_MOD_PITCH) || (menu_option == MENU_MOD_VOLUME))
            {
                mmStop();

                if (active_module_id != -1)
                    mmUnload(active_module_id);

                active_module_id = -1;
            }
        }

        if (keys_down & KEY_START)
            break;
    }

    soundDisable();

    return 0;
}
