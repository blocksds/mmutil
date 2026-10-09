// SPDX-License-Identifier: ISC
//
// Copyright (c) 2008 Mukunda Johnson (mukunda@maxmod.org)
// Copyright (c) 2026 Antonio Niño Díaz

/****************************************************************************
 *                ____ ___  ____ __  ______ ___  ____  ____/ /              *
 *               / __ `__ \/ __ `/ |/ / __ `__ \/ __ \/ __  /               *
 *              / / / / / / /_/ />  </ / / / / / /_/ / /_/ /                *
 *             /_/ /_/ /_/\__,_/_/|_/_/ /_/ /_/\____/\__,_/                 *
 *                                                                          *
 ****************************************************************************/

#ifndef MAS_H__
#define MAS_H__

// Flags used to determine the characteristics of samples in an input module
// file. Not all combinations are supported in a MAS sample.

#define SAMPF_16BIT         0x001
#define SAMPF_SIGNED        0x002
#define SAMPF_COMP          0x004

#define SAMP_FORMAT_U8      (0)
#define SAMP_FORMAT_U16     (SAMPF_16BIT)
#define SAMP_FORMAT_S8      (SAMPF_SIGNED)
#define SAMP_FORMAT_S16     (SAMPF_16BIT | SAMPF_SIGNED)
#define SAMP_FORMAT_ADPCM   (SAMPF_COMP)

// MAS sample loop types

#define MM_SREPEAT_FORWARD      1 // Forward loop
#define MM_SREPEAT_OFF          2 // No loop

// MAS sample formats

#define MM_SFORMAT_8BIT         0 // Signed 8 bit
#define MM_SFORMAT_16BIT        1 // Signed 16 bit
#define MM_SFORMAT_ADPCM        2 // ADPCM (Invalid)
#define MM_SFORMAT_ERROR        3 // Invalid

// Flags to specify the type of MAS file

#define MAS_TYPE_SONG       0
#define MAS_TYPE_SAMPLE_GBA 1
#define MAS_TYPE_SAMPLE_NDS 2

typedef struct tInstrument_Envelope
{
    u8      loop_start;
    u8      loop_end;
    u8      sus_start;
    u8      sus_end;
    u8      node_count;
    u16     node_x[25];
    u8      node_y[25];
    bool    env_filter;
    bool    env_valid;
    bool    env_enabled;
}
Instrument_Envelope;

typedef struct tInstrument
{
    u32     parapointer;

    bool    is_valid;
    u8      global_volume;
    u8      setpan;
    u16     fadeout;
    u8      random_volume;
    u8      nna;
    u8      dct;
    u8      dca;
    u8      env_flags;
    u16     notemap[120];

    char    name[32];

    Instrument_Envelope     envelope_volume;
    Instrument_Envelope     envelope_pan;
    Instrument_Envelope     envelope_pitch;
}
Instrument;

#define MAS_INSTR_FLAG_VOL_ENV_EXISTS   (1 << 0) // Volume envelope exists
#define MAS_INSTR_FLAG_PAN_ENV_EXISTS   (1 << 1) // Panning envelope exists
#define MAS_INSTR_FLAG_PITCH_ENV_EXISTS (1 << 2) // Pitch envelope exists
#define MAS_INSTR_FLAG_VOL_ENV_ENABLED  (1 << 3) // Volume envelope enabled
// In XM, bits 0 and 3 are always set together. In IT, they can be set
// independently. Other formats don't use them.

typedef struct tSample
{
    u32     parapointer;

    u8      global_volume;
    u8      default_volume;
    u8      default_panning;
    u32     sample_length;
    u32     loop_start;
    u32     loop_end;
    u8      loop_type;
    u32     frequency;
    void   *data;

    u8      vibtype;
    u8      vibdepth;
    u8      vibspeed;
    u8      vibrate;
    u16     msl_index;
    u8      rsamp_index;

    u8      format;
//    bool    samp_signed;

    // file info
    u32     datapointer;
//    bool    bit16;
//    bool    samp_unsigned;
    u8      it_compression;
    char    name[32];
    char    filename[12];
}
Sample;

typedef struct tPatternEntry
{
    u8      note;
    u8      inst;
    u8      vol;
    u8      fx;
    u8      param;
}
PatternEntry;

typedef struct tPattern
{
    u32             parapointer;
    u16             nrows;
    int             clength;
    PatternEntry    data[MAX_CHANNELS*256];
    bool            cmarks[256];
}
Pattern;

typedef struct tMAS_Module
{
    char    title[32];
    u16     order_count;
    u16     inst_count;
    u16     samp_count;
    u16     patt_count;
    u16     restart_pos;
    bool    stereo;
    bool    inst_mode;
    u8      freq_mode;
    bool    old_effects;
    bool    link_gxx;
    bool    xm_mode;
    bool    old_mode;
    u8      global_volume;
    u16     initial_speed;
    u16     initial_tempo;
    u8      channel_volume[MAX_CHANNELS];
    u8      channel_panning[MAX_CHANNELS];
    u8      orders[256];
    Instrument  *instruments;
    Sample      *samples;
    Pattern     *patterns;
}
MAS_Module;

typedef enum
{
    MAS_FX_NONE = 0,

    // IT and S3M effects (S3M is a subset of IT)
    MAS_FX_SET_SPEED            = 1,
    MAS_FX_POSITION_JUMP        = 2,
    MAS_FX_PATTERN_BREAK        = 3,
    MAS_FX_VOLUME_SLIDE         = 4,
    MAS_FX_PORTAMENTO_DOWN      = 5,
    MAS_FX_PORTAMENTO_UP        = 6,
    MAS_FX_GLISSANDO            = 7, // Porta to note
    MAS_FX_VIBRATO              = 8,
    MAS_FX_TREMOR               = 9,
    MAS_FX_ARPEGGIO             = 10,
    MAS_FX_VIBRATO_VOLUME       = 11,
    MAS_FX_PORTA_VOLUME         = 12,
    MAS_FX_CHANNEL_VOLUME       = 13,
    MAS_FX_CHANNEL_VOLUME_SLIDE = 14,
    MAS_FX_SAMPLE_OFFSET        = 15,
    MAS_FX_PANNING_SLIDE        = 16,
    MAS_FX_RETRIGGER            = 17,
    MAS_FX_TREMOLO              = 18,
    MAS_FX_EXTENDED             = 19,
    MAS_FX_SET_TEMPO            = 20,
    MAS_FX_FINE_VIBRATO         = 21,
    MAS_FX_SET_GLOBAL_VOLUME    = 22,
    MAS_FX_GLOBAL_VOLUME_SLIDE  = 23,
    MAS_FX_SET_PANNING          = 24,
    MAS_FX_PANBRELLO            = 25,
    MAS_FX_SET_FILTER           = 26,

    // Most MOD/XM effects can be mapped to IT/S3M effects, but not all
    MAS_FX_XM_SET_VOLUME        = 27, // MOD/XM
    MAS_FX_XM_KEY_OFF           = 28, // XM
    MAS_FX_XM_ENVELOPE_POS      = 29, // XM
    MAS_FX_XM_TREMOR            = 30, // XM
}
MAS_PatternEffect;

typedef enum
{
    MAS_FX_EXT_FINE_VOL_SLIDE_UP    = 0,
    MAS_FX_EXT_FINE_VOL_SLIDE_DOWN  = 1,
    MAS_FX_EXT_OLD_RETRIGGER        = 2,
    MAS_FX_EXT_VIBRATO_WAVEFORM     = 3,
    MAS_FX_EXT_TREMOLO_WAVEFORM     = 4,
    MAS_FX_EXT_PANBRELLO_WAVEFORM   = 5,
    MAS_FX_EXT_FINE_PATTERN_DELAY   = 6,
    MAS_FX_EXT_INSTRUMENT_CONTROL   = 7,
    MAS_FX_EXT_SET_PANNING          = 8,
    MAS_FX_EXT_SOUND_CONTROL        = 9,
    MAS_FX_EXT_HIGH_OFFSET          = 10,
    MAS_FX_EXT_PATTERN_LOOP         = 11,
    MAS_FX_EXT_NOTE_CUT             = 12,
    MAS_FX_EXT_NOTE_DELAY           = 13,
    MAS_FX_EXT_PATTERN_DELAY        = 14,
    MAS_FX_EXT_SONG_MESSAGE         = 15,
}
MAS_PatternEffectExtended;

void Write_Instrument_Envelope(Instrument_Envelope *env);
void Write_Instrument(Instrument *inst);
void Write_SampleData(Sample *samp);
void Write_Sample(Sample *samp);
void Write_Pattern(Pattern *patt, bool xm_vol);
int Write_MAS(MAS_Module *mod, bool msl_dep);
void Delete_Module(MAS_Module *mod);

void Sanitize_Module(MAS_Module *mod);

extern u32 MAS_FILESIZE;

#endif // MAS_H__
