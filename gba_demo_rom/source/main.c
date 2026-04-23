// SPDX-License-Identifier: CC0-1.0
//
// SPDX-FileContributor: Antonio Niño Díaz, 2026

// This demo lets the user experiment with soundbanks that may not be known at
// build time. It needs to fetch the number of samples and modules at runtime.
// Also, it doesn't know the names of the samples and modules. It's only done
// like this so that this ROM can be used to create test ROMs with mmutil.
//
// This demo depends on the package target-gba-libtonc of wf-pacman:
//
//     wf-pacman -Syu target-gba-libtonc

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <maxmod.h>
#include <mm_msl.h>
#include <tonc.h>

extern char __ROM_END__[];
static uintptr_t ROM_END = (uintptr_t)__ROM_END__;

static void tte_printf(const char *msg, ...)
{
    char buf[128];

    va_list args;
    va_start(args, msg);
    vsnprintf(buf, sizeof(buf), msg, args);
    va_end(args);

    tte_write(buf);
}

static void wait_forever(void)
{
    while (1)
        VBlankIntrWait();
}

static void vbl_handler(void)
{
    pal_bg_bank[0][0] = RGB15(0, 0, 8);

    mmVBlank();
}

static void hbl_handler(void)
{
    pal_bg_bank[0][0] = RGB15(0, 0, 8 + REG_VCOUNT / 16);
}

typedef enum
{
    MENU_START,

    MENU_SFX_ID = MENU_START,
    MENU_SFX_RATE,
    MENU_SFX_VOLUME,
    MENU_SFX_PANNING,

    MENU_MOD_ID,
    MENU_MOD_TEMPO,
    MENU_MOD_PITCH,
    MENU_MOD_VOLUME,

    MENU_END = MENU_MOD_VOLUME,
}
menu_option_t;

static void refresh_cursor(int menu_option)
{
    tte_set_pos(0, 32);

    if ((menu_option == MENU_SFX_ID) || (menu_option == MENU_SFX_RATE) ||
        (menu_option == MENU_SFX_VOLUME) || (menu_option == MENU_SFX_PANNING))
    {
        tte_write("A/B: Start/Stop SFX   \n");
    }
    else if ((menu_option == MENU_MOD_ID) || (menu_option == MENU_MOD_TEMPO) ||
        (menu_option == MENU_MOD_PITCH) || (menu_option == MENU_MOD_VOLUME))
    {
        tte_write("A/B: Start/Stop module\n");
    }

#define SEL_OPTION(x) (menu_option == (x) ? ">" : " ")

    tte_set_pos(24, 64);
    tte_write(SEL_OPTION(MENU_SFX_ID));
    tte_set_pos(24, 72);
    tte_write(SEL_OPTION(MENU_SFX_RATE));
    tte_set_pos(24, 80);
    tte_write(SEL_OPTION(MENU_SFX_VOLUME));
    tte_set_pos(24, 88);
    tte_write(SEL_OPTION(MENU_SFX_PANNING));

    tte_set_pos(24, 120);
    tte_write(SEL_OPTION(MENU_MOD_ID));
    tte_set_pos(24, 128);
    tte_write(SEL_OPTION(MENU_MOD_TEMPO));
    tte_set_pos(24, 136);
    tte_write(SEL_OPTION(MENU_MOD_PITCH));
    tte_set_pos(24, 144);
    tte_write(SEL_OPTION(MENU_MOD_VOLUME));
}

int main(void)
{
    // Make the repetition period a lot shorter than the default
    key_repeat_limits(30, 2);

    irq_init(NULL);
    irq_add(II_VBLANK, vbl_handler);
    irq_add(II_HBLANK, hbl_handler);

    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;

    // Base TTE init for tilemaps
    tte_init_se(0,                      // Background number (0-3)
                BG_CBB(0) | BG_SBB(31), // BG control
                0,                      // Tile offset (special cattr)
                CLR_WHITE,              // Ink color
                14,                     // BitUnpack offset (on-pixel = 15)
                NULL,                   // Default font (sys8)
                NULL);                  // Default renderer (se_drawg_s)

    msl_head_data *header = (msl_head_data *)ROM_END;

    uint32_t magic_0 = ('*' << 0) | ('m' << 8) | ('a' << 16) | ('x' << 24);
    uint32_t magic_1 = ('m' << 0) | ('o' << 8) | ('d' << 16) | ('*' << 24);

    if ((header->reserved[0] != magic_0) || (header->reserved[1] != magic_1))
    {
        tte_write("Soundbank magic not found!");
        wait_forever();
    }

    if (!mmInitDefault(header, 10))
    {
        tte_write("\n");
        tte_write("Failed to init Maxmod!");
        wait_forever();
    }

    mm_word module_count = mmGetModuleCount();
    mm_word sample_count = mmGetSampleCount();

    tte_write("  Maxmod demo\n");
    tte_write("  -----------\n");

    tte_set_pos(120, 0);
    tte_printf("Samples: %u\n", sample_count);
    tte_set_pos(120, 8);
    tte_printf("Modules: %u\n", module_count);

    tte_set_pos(0, 24);
    tte_write("LEFT/RIGHT: Change value\n");
    tte_write("\n"); // Empty line for A/B

    menu_option_t menu_option = MENU_START;

    // Values selected by the user from the menu

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

    tte_write("\n");
    tte_printf("            [SFX]\n");
    tte_printf("\n");
    tte_printf("     Sample ID: %u\n", selected_sfx_id);
    tte_printf("     Rate:      %u\n", selected_sfx_rate);
    tte_printf("     Volume:    %u\n", selected_sfx_volume);
    tte_printf("     Panning:   %u\n", selected_sfx_panning);
    tte_printf("\n");
    tte_printf("          [Module]\n");
    tte_printf("\n");
    tte_printf("     Module ID: %u\n", selected_module_id);
    tte_printf("     Tempo:     %u\n", selected_module_tempo);
    tte_printf("     Pitch:     %u\n", selected_module_pitch);
    tte_printf("     Volume:    %u\n", selected_module_volume);

    refresh_cursor(menu_option);

    while (1)
    {
        VBlankIntrWait();
        mmFrame();

        key_poll();

        uint16_t keys_down = key_hit(KEY_ANY);
        uint16_t keys_repeat = keys_down | key_repeat(KEY_ANY);

        if (keys_repeat & KEY_LEFT)
        {
            if (menu_option == MENU_SFX_ID)
            {
                if (selected_sfx_id == 0)
                    selected_sfx_id = sample_count - 1;
                else
                    selected_sfx_id--;

                tte_set_pos(128, 64);
                tte_printf("%u   ", selected_sfx_id);
            }
            else if (menu_option == MENU_SFX_RATE)
            {
                selected_sfx_rate -= 16;

                if (selected_sfx_rate < 512)
                    selected_sfx_rate = 512;

                if (active_sfx_handle != -1)
                    mmEffectRate(active_sfx_handle, selected_sfx_rate);

                tte_set_pos(128, 72);
                tte_printf("%u   ", selected_sfx_rate);
            }
            else if (menu_option == MENU_SFX_VOLUME)
            {
                if (selected_sfx_volume > 0)
                    selected_sfx_volume--;

                if (active_sfx_handle != -1)
                    mmEffectVolume(active_sfx_handle, selected_sfx_volume);

                tte_set_pos(128, 80);
                tte_printf("%u   ", selected_sfx_volume);
            }
            else if (menu_option == MENU_SFX_PANNING)
            {
                if (selected_sfx_panning > 0)
                    selected_sfx_panning--;

                if (active_sfx_handle != -1)
                    mmEffectPanning(active_sfx_handle, selected_sfx_panning);

                tte_set_pos(128, 88);
                tte_printf("%u   ", selected_sfx_panning);
            }
            else if (menu_option == MENU_MOD_ID)
            {
                if (selected_module_id == 0)
                    selected_module_id = module_count - 1;
                else
                    selected_module_id--;

                tte_set_pos(128, 120);
                tte_printf("%u   ", selected_module_id);
            }
            else if (menu_option == MENU_MOD_TEMPO)
            {
                selected_module_tempo -= 16;

                if (selected_module_tempo < 512)
                    selected_module_tempo = 512;

                mmSetModuleTempo(selected_module_tempo);

                tte_set_pos(128, 128);
                tte_printf("%u   ", selected_module_tempo);
            }
            else if (menu_option == MENU_MOD_PITCH)
            {
                selected_module_pitch -= 16;

                if (selected_module_pitch < 512)
                    selected_module_pitch = 512;

                mmSetModulePitch(selected_module_pitch);

                tte_set_pos(128, 136);
                tte_printf("%u   ", selected_module_pitch);
            }
            else if (menu_option == MENU_MOD_VOLUME)
            {
                if (selected_module_volume > 4)
                    selected_module_volume -= 4;
                else
                    selected_module_volume = 0;

                mmSetModuleVolume(selected_module_volume);

                tte_set_pos(128, 144);
                tte_printf("%u   ", selected_module_volume);
            }
        }
        else if (keys_repeat & KEY_RIGHT)
        {
            if (menu_option == MENU_SFX_ID)
            {
                selected_sfx_id++;
                if (selected_sfx_id == sample_count)
                    selected_sfx_id = 0;

                tte_set_pos(128, 64);
                tte_printf("%u   ", selected_sfx_id);
            }
            else if (menu_option == MENU_SFX_RATE)
            {
                selected_sfx_rate += 16;

                if (selected_sfx_rate > 2048)
                    selected_sfx_rate = 2047;

                if (active_sfx_handle != -1)
                    mmEffectRate(active_sfx_handle, selected_sfx_rate);

                tte_set_pos(128, 72);
                tte_printf("%u   ", selected_sfx_rate);
            }
            else if (menu_option == MENU_SFX_VOLUME)
            {
                if (selected_sfx_volume < 255)
                    selected_sfx_volume++;

                if (active_sfx_handle != -1)
                    mmEffectVolume(active_sfx_handle, selected_sfx_volume);

                tte_set_pos(128, 80);
                tte_printf("%u   ", selected_sfx_volume);
            }
            else if (menu_option == MENU_SFX_PANNING)
            {
                if (selected_sfx_panning < 255)
                    selected_sfx_panning++;

                if (active_sfx_handle != -1)
                    mmEffectPanning(active_sfx_handle, selected_sfx_panning);

                tte_set_pos(128, 88);
                tte_printf("%u   ", selected_sfx_panning);
            }
            else if (menu_option == MENU_MOD_ID)
            {
                selected_module_id++;
                if (selected_module_id == module_count)
                    selected_module_id = 0;

                tte_set_pos(128, 120);
                tte_printf("%u   ", selected_module_id);
            }
            else if (menu_option == MENU_MOD_TEMPO)
            {
                selected_module_tempo += 16;

                if (selected_module_tempo >= 2048)
                    selected_module_tempo = 2048;

                mmSetModuleTempo(selected_module_tempo);

                tte_set_pos(128, 128);
                tte_printf("%u   ", selected_module_tempo);
            }
            else if (menu_option == MENU_MOD_PITCH)
            {
                selected_module_pitch += 16;

                if (selected_module_pitch >= 2048)
                    selected_module_pitch = 2048;

                mmSetModulePitch(selected_module_pitch);

                tte_set_pos(128, 136);
                tte_printf("%u   ", selected_module_pitch);
            }
            else if (menu_option == MENU_MOD_VOLUME)
            {
                if (selected_module_volume < (1024 - 4))
                    selected_module_volume += 4;
                else
                    selected_module_volume = 1024;

                mmSetModuleVolume(selected_module_volume);

                tte_set_pos(128, 144);
                tte_printf("%u   ", selected_module_volume);
            }
        }

        if (keys_repeat & KEY_DOWN)
        {
            if (menu_option == MENU_END)
                menu_option = MENU_START;
            else
                menu_option++;

            refresh_cursor(menu_option);
        }
        else if (keys_repeat & KEY_UP)
        {
            if (menu_option == MENU_START)
                menu_option = MENU_END;
            else
                menu_option--;

            refresh_cursor(menu_option);
        }

        if (keys_down & KEY_A)
        {
            if ((menu_option == MENU_SFX_ID) || (menu_option == MENU_SFX_RATE) ||
                (menu_option == MENU_SFX_VOLUME) || (menu_option == MENU_SFX_PANNING))
            {
                mmEffectCancel(active_sfx_handle);

                mm_sound_effect effect =
                {
                    .id = selected_sfx_id,
                    .rate = selected_sfx_rate,
                    .handle = 0,
                    .volume = selected_sfx_volume,
                    .panning = selected_sfx_panning
                };

                active_sfx_handle = mmEffectEx(&effect);
                if (active_sfx_handle == MM_SFXHAND_INVALID)
                {
                    tte_printf("Failed to play effect %d", selected_sfx_id);
                    wait_forever();
                }
            }
            else if ((menu_option == MENU_MOD_ID) || (menu_option == MENU_MOD_TEMPO) ||
                     (menu_option == MENU_MOD_PITCH) || (menu_option == MENU_MOD_VOLUME))
            {
                mmStop();

                mmStart(selected_module_id, MM_PLAY_LOOP);
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

                active_sfx_handle = MM_SFXHAND_INVALID;
            }
            else if ((menu_option == MENU_MOD_ID) || (menu_option == MENU_MOD_TEMPO) ||
                     (menu_option == MENU_MOD_PITCH) || (menu_option == MENU_MOD_VOLUME))
            {
                mmStop();
            }
        }
    }
}
