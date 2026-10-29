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

#include <math.h>
#include <stddef.h>

#include "defs.h"
#include "mas.h"
#include "samples.h"

#define ABS(x) ((x) < 0 ? -(x) : (x))

/*

Resampling methods:
 -Nearest: No filtering (1-tap)
 -Linear:  Linear interpolation (2-tap)
 -Cubic:   Cubic spline interpolation (4-tap)
 -Sinc:    Hann-windowed sinc filter (arbitrary even tap count)

Sinc resampling uses the formula:
  y[n] = Sum[Sinc[(k-phi)*Fc*Pi] * WindowFunction[(k-phi) / N] * x[n+k], {k,-N/2+1,N/2}]
Notes:
* Fc=1.0 means no lowpass
* WindowFunction[] accepts a parameter x in range -0.5 to +0.5
* Sinc[x] = Sin[x]/x, with Sinc[0] = 1.0. Note that this will
  degenerate at around +/-1.0e-3 with single-precision floats,
  so we must ensure that Sinc[x]=1.0 in this vicinity.

We use a Hann window here, so the kernel formula expands as:
  kernel[k] = Sin[(k-phi)*Fc*Pi] * Cos[(k-phi)/N*Pi]^2 / ((k-phi)*Fc*Pi)

Because we step linearly through k, we can use oscillators to
minimize trig functions. We use the 'magic-circle' oscillator:
  Parameters:
    k = 2*Sin[omega/2]
  Initialization:
    u[0] = Cos[(n-0.5)*omega + phi]
    v[0] = Sin[n*omega + phi]
  Update:
    u[n+1] = u[n] - k*v[n]
    v[n+1] = v[n] + k*u[n+1]

The cubic interpolation formula is well-known, but this notice was original:
    NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE

    The following resample routine was stolen from CHIBITRACKER (http://chibitracker.berlios.de),
    thanks reduz!

    NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE
The expression was re-arranged into Horner form for overall improvement.

*/

typedef struct tSincState
{
    float sinc_omega;
    float sinc_kappa;
    float win_omega;
    float win_kappa;
} SincState;

static void InitSincState(SincState *state, double oldrate, double newrate, u8 order, double Fc)
{
    // For the case of downsampling (newrate < oldrate),
    // the cutoff must be min(Fc, newrate/oldrate).
    if (newrate < oldrate*Fc)
        Fc = (double)newrate / oldrate;
    state->sinc_omega = (float)(Fc * M_PI);
    state->sinc_kappa = (float)(2.0*sin(0.5*state->sinc_omega));
    state->win_omega  = (float)M_PI / order;
    state->win_kappa  = (float)(2.0*sin(0.5*state->win_omega));
}

static s16 ResampleNearest(const s16 *src, u8 order, float phi, void *state)
{
    (void)order;
    (void)phi;
    (void)state;
    return src[0];
}

static s16 ResampleLinear(const s16 *src, u8 order, float phi, void *state)
{
    (void)order;
    (void)state;
    return src[0] + (s16)lrintf(((int)src[1] - src[0]) * phi);
}

static s16 ResampleCubic(const s16 *src, u8 order, float phi, void *state)
{
    (void)order;
    (void)state;
    s32 a0 = -src[-1] + src[0] - src[1] + src[2];
    s32 a1 =  src[-1] - src[0] - a0;
    s32 a2 = -src[-1] + src[1];
    s32 a3 =  src[ 0];
    long v = lrintf(a3 + phi*(a2 + phi*(a1 + phi*a0)));
    if (v < -32768)
        v = -32768;
    if (v > +32767)
        v = +32767;
    return (s16)v;
}

static s16 ResampleSinc(const s16 *src, u8 order, float phi, void *state)
{
    SincState *sinc_state = (SincState*)state;
    int k = -(int)(order/2-1);
    float sinc_phi = ((float)k - phi)*sinc_state->sinc_omega;
    float sinc_u   = cosf(sinc_phi - 0.5f*sinc_state->sinc_omega);
    float sinc_v   = sinf(sinc_phi);
    float win_phi  = ((float)k - phi)*sinc_state->win_omega + (float)M_PI_2;
    float win_u    = cosf(win_phi - 0.5f*sinc_state->win_omega);
    float win_v    = sinf(win_phi);
    float sum = 0, sumw = 0;
    for (; k < (int)(order/2+1); k++)
    {
        float w = ((ABS(sinc_phi) > 1.0e-3f) ? (sinc_v / sinc_phi) : 1.0f) * (win_v*win_v);
        sum  += w*src[k];
        sumw += w;
        sinc_u -= sinc_state->sinc_kappa * sinc_v;
        sinc_v += sinc_state->sinc_kappa * sinc_u;
        win_u  -= sinc_state->win_kappa * win_v;
        win_v  += sinc_state->win_kappa * win_u;
        sinc_phi += sinc_state->sinc_omega;
    }
    long v = lrintf(sum / sumw);
    if (v < -32768)
        v = -32768;
    if (v > +32767)
        v = +32767;
    return (s16)v;
}

static void Sample_ProcessSRC_Template(
    Sample *samp,
    u32 oldrate,
    u32 newrate,
    u8 order,
    s16 (*resampleKernel)(const s16 *src, u8 order, float phi, void *state),
    void *resampleState
)
{
    // Note that we append (N/2)*newrate/oldrate samples.
    // This is to account for pre-ringing that may cause
    // the loop to no longer synchronize with the pre-loop.
    // This is not required for one-shot samples!
    u32 preringlen = samp->loop_type ? order/2 : 0;
    u32 oldlength = samp->loop_type ? samp->loop_end : samp->sample_length;
    u32 oldlooplen = samp->loop_end - samp->loop_start;
    u32 newsize = (u32)((((u64)oldlength + preringlen) * newrate - 1) / oldrate + 1);
    u32 newlooplen = (u32)(oldlooplen * newrate / oldrate);
    void *newdata = malloc((newsize + order-1 + order/2) * 2);
    s16 *src = Sample_GetFirstSample(samp->data);
    s16 *dst = Sample_GetFirstSample(newdata);

    // Silence pre-padding in output
    for (int n = 0; n < (int)(order-1)/2; n++)
        dst[-1-n] = 0;

    // Ensure that the post-padding in the source comes from
    // either the loop point, or is silence if not looped
    if (samp->loop_type)
    {
        for (u32 n = 0; n < order/2*2; n++)
            src[oldlength+n] = src[oldlength+n-oldlooplen];
    }
    else
    {
        for (u32 n = 0; n < order/2*2; n++)
            src[oldlength+n] = 0;
    }

    u32 pos = 0;
    float newrate_inv = 1.0f / (float)newrate;
    for (u32 n = 0; n < newsize; n++)
    {
        dst[n] = resampleKernel(src, order, (float)pos * newrate_inv, resampleState);
        pos += oldrate;
        while (pos >= newrate)
        {
            pos -= newrate;
            src += 1;
        }
    }

    // Swap data buffer and store new sample information
    free(samp->data);
    samp->data          = newdata;
    samp->sample_length = newsize;
    samp->loop_start    = newsize - newlooplen;
    samp->loop_end      = newsize;
    samp->frequency     = (u32)(((u64)samp->frequency * newrate + oldrate/2) / oldrate);
}

void Sample_ProcessSRC(Sample *samp, u32 oldrate, u32 newrate, u8 method, u8 order, double Fc)
{
    switch (method)
    {
        case SAMPLESRC_NEAREST:
            Sample_ProcessSRC_Template(samp, oldrate, newrate, order, ResampleNearest, NULL);
            break;
        case SAMPLESRC_LINEAR:
            Sample_ProcessSRC_Template(samp, oldrate, newrate, order, ResampleLinear, NULL);
            break;
        case SAMPLESRC_CUBIC:
            Sample_ProcessSRC_Template(samp, oldrate, newrate, order, ResampleCubic, NULL);
            break;
        case SAMPLESRC_SINC:
            {
                SincState state;
                InitSincState(&state, oldrate, newrate, order, Fc);
                Sample_ProcessSRC_Template(samp, oldrate, newrate, order, ResampleSinc, &state);
            }
            break;
    }
}
