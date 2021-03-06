/* Copyright (c) 2011 Xiph.Org Foundation
   Written by Jean-Marc Valin */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "kiss_fft.h"
#include "celt.h"
#include "modes.h"
#include "arch.h"
#include "quant_bands.h"
#include <stdio.h>
#include "analysis.h"
#include "mlp.h"

extern const MLP net;

#ifndef M_PI
#define M_PI 3.141592653
#endif

static const float dct_table[128] = {
        0.250000f, 0.250000f, 0.250000f, 0.250000f, 0.250000f, 0.250000f, 0.250000f, 0.250000f,
        0.250000f, 0.250000f, 0.250000f, 0.250000f, 0.250000f, 0.250000f, 0.250000f, 0.250000f,
        0.351851f, 0.338330f, 0.311806f, 0.273300f, 0.224292f, 0.166664f, 0.102631f, 0.034654f,
       -0.034654f,-0.102631f,-0.166664f,-0.224292f,-0.273300f,-0.311806f,-0.338330f,-0.351851f,
        0.346760f, 0.293969f, 0.196424f, 0.068975f,-0.068975f,-0.196424f,-0.293969f,-0.346760f,
       -0.346760f,-0.293969f,-0.196424f,-0.068975f, 0.068975f, 0.196424f, 0.293969f, 0.346760f,
        0.338330f, 0.224292f, 0.034654f,-0.166664f,-0.311806f,-0.351851f,-0.273300f,-0.102631f,
        0.102631f, 0.273300f, 0.351851f, 0.311806f, 0.166664f,-0.034654f,-0.224292f,-0.338330f,
        0.326641f, 0.135299f,-0.135299f,-0.326641f,-0.326641f,-0.135299f, 0.135299f, 0.326641f,
        0.326641f, 0.135299f,-0.135299f,-0.326641f,-0.326641f,-0.135299f, 0.135299f, 0.326641f,
        0.311806f, 0.034654f,-0.273300f,-0.338330f,-0.102631f, 0.224292f, 0.351851f, 0.166664f,
       -0.166664f,-0.351851f,-0.224292f, 0.102631f, 0.338330f, 0.273300f,-0.034654f,-0.311806f,
        0.293969f,-0.068975f,-0.346760f,-0.196424f, 0.196424f, 0.346760f, 0.068975f,-0.293969f,
       -0.293969f, 0.068975f, 0.346760f, 0.196424f,-0.196424f,-0.346760f,-0.068975f, 0.293969f,
        0.273300f,-0.166664f,-0.338330f, 0.034654f, 0.351851f, 0.102631f,-0.311806f,-0.224292f,
        0.224292f, 0.311806f,-0.102631f,-0.351851f,-0.034654f, 0.338330f, 0.166664f,-0.273300f,
};

static const float analysis_window[240] = {
      0.000043f, 0.000171f, 0.000385f, 0.000685f, 0.001071f, 0.001541f, 0.002098f, 0.002739f,
      0.003466f, 0.004278f, 0.005174f, 0.006156f, 0.007222f, 0.008373f, 0.009607f, 0.010926f,
      0.012329f, 0.013815f, 0.015385f, 0.017037f, 0.018772f, 0.020590f, 0.022490f, 0.024472f,
      0.026535f, 0.028679f, 0.030904f, 0.033210f, 0.035595f, 0.038060f, 0.040604f, 0.043227f,
      0.045928f, 0.048707f, 0.051564f, 0.054497f, 0.057506f, 0.060591f, 0.063752f, 0.066987f,
      0.070297f, 0.073680f, 0.077136f, 0.080665f, 0.084265f, 0.087937f, 0.091679f, 0.095492f,
      0.099373f, 0.103323f, 0.107342f, 0.111427f, 0.115579f, 0.119797f, 0.124080f, 0.128428f,
      0.132839f, 0.137313f, 0.141849f, 0.146447f, 0.151105f, 0.155823f, 0.160600f, 0.165435f,
      0.170327f, 0.175276f, 0.180280f, 0.185340f, 0.190453f, 0.195619f, 0.200838f, 0.206107f,
      0.211427f, 0.216797f, 0.222215f, 0.227680f, 0.233193f, 0.238751f, 0.244353f, 0.250000f,
      0.255689f, 0.261421f, 0.267193f, 0.273005f, 0.278856f, 0.284744f, 0.290670f, 0.296632f,
      0.302628f, 0.308658f, 0.314721f, 0.320816f, 0.326941f, 0.333097f, 0.339280f, 0.345492f,
      0.351729f, 0.357992f, 0.364280f, 0.370590f, 0.376923f, 0.383277f, 0.389651f, 0.396044f,
      0.402455f, 0.408882f, 0.415325f, 0.421783f, 0.428254f, 0.434737f, 0.441231f, 0.447736f,
      0.454249f, 0.460770f, 0.467298f, 0.473832f, 0.480370f, 0.486912f, 0.493455f, 0.500000f,
      0.506545f, 0.513088f, 0.519630f, 0.526168f, 0.532702f, 0.539230f, 0.545751f, 0.552264f,
      0.558769f, 0.565263f, 0.571746f, 0.578217f, 0.584675f, 0.591118f, 0.597545f, 0.603956f,
      0.610349f, 0.616723f, 0.623077f, 0.629410f, 0.635720f, 0.642008f, 0.648271f, 0.654508f,
      0.660720f, 0.666903f, 0.673059f, 0.679184f, 0.685279f, 0.691342f, 0.697372f, 0.703368f,
      0.709330f, 0.715256f, 0.721144f, 0.726995f, 0.732807f, 0.738579f, 0.744311f, 0.750000f,
      0.755647f, 0.761249f, 0.766807f, 0.772320f, 0.777785f, 0.783203f, 0.788573f, 0.793893f,
      0.799162f, 0.804381f, 0.809547f, 0.814660f, 0.819720f, 0.824724f, 0.829673f, 0.834565f,
      0.839400f, 0.844177f, 0.848895f, 0.853553f, 0.858151f, 0.862687f, 0.867161f, 0.871572f,
      0.875920f, 0.880203f, 0.884421f, 0.888573f, 0.892658f, 0.896677f, 0.900627f, 0.904508f,
      0.908321f, 0.912063f, 0.915735f, 0.919335f, 0.922864f, 0.926320f, 0.929703f, 0.933013f,
      0.936248f, 0.939409f, 0.942494f, 0.945503f, 0.948436f, 0.951293f, 0.954072f, 0.956773f,
      0.959396f, 0.961940f, 0.964405f, 0.966790f, 0.969096f, 0.971321f, 0.973465f, 0.975528f,
      0.977510f, 0.979410f, 0.981228f, 0.982963f, 0.984615f, 0.986185f, 0.987671f, 0.989074f,
      0.990393f, 0.991627f, 0.992778f, 0.993844f, 0.994826f, 0.995722f, 0.996534f, 0.997261f,
      0.997902f, 0.998459f, 0.998929f, 0.999315f, 0.999615f, 0.999829f, 0.999957f, 1.000000f,
};

static const int tbands[NB_TBANDS+1] = {
       2,  4,  6,  8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, 56, 68, 80, 96, 120
};

static const int extra_bands[NB_TOT_BANDS+1] = {
      0, 2,  4,  6,  8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, 56, 68, 80, 96, 120, 160, 200
};

/*static const float tweight[NB_TBANDS+1] = {
      .3, .4, .5, .6, .7, .8, .9, 1., 1., 1., 1., 1., 1., 1., .8, .7, .6, .5
};*/

#define NB_TONAL_SKIP_BANDS 9

#define cA 0.43157974f
#define cB 0.67848403f
#define cC 0.08595542f
#define cE ((float)M_PI/2)
static inline float fast_atan2f(float y, float x) {
   float x2, y2;
   /* Should avoid underflow on the values we'll get */
   if (ABS16(x)+ABS16(y)<1e-9f)
   {
      x*=1e12f;
      y*=1e12f;
   }
   x2 = x*x;
   y2 = y*y;
   if(x2<y2){
      float den = (y2 + cB*x2) * (y2 + cC*x2);
      if (den!=0)
         return -x*y*(y2 + cA*x2) / den + (y<0 ? -cE : cE);
      else
         return (y<0 ? -cE : cE);
   }else{
      float den = (x2 + cB*y2) * (x2 + cC*y2);
      if (den!=0)
         return  x*y*(x2 + cA*y2) / den + (y<0 ? -cE : cE) - (x*y<0 ? -cE : cE);
      else
         return (y<0 ? -cE : cE) - (x*y<0 ? -cE : cE);
   }
}

void tonality_analysis(TonalityAnalysisState *tonal, AnalysisInfo *info, CELTEncoder *celt_enc, const opus_val16 *x, int C, int lsb_depth)
{
    int i, b;
    const CELTMode *mode;
    const kiss_fft_state *kfft;
    kiss_fft_cpx in[480], out[480];
    int N = 480, N2=240;
    float * OPUS_RESTRICT A = tonal->angle;
    float * OPUS_RESTRICT dA = tonal->d_angle;
    float * OPUS_RESTRICT d2A = tonal->d2_angle;
    float tonality[240];
    float noisiness[240];
    float band_tonality[NB_TBANDS];
    float logE[NB_TBANDS];
    float BFCC[8];
    float features[100];
    float frame_tonality;
    float max_frame_tonality;
    /*float tw_sum=0;*/
    float frame_noisiness;
    const float pi4 = (float)(M_PI*M_PI*M_PI*M_PI);
    float slope=0;
    float frame_stationarity;
    float relativeE;
    float frame_prob;
    float alpha, alphaE, alphaE2;
    float frame_loudness;
    float bandwidth_mask;
    int bandwidth=0;
    float maxE = 0;
    float noise_floor;
    celt_encoder_ctl(celt_enc, CELT_GET_MODE(&mode));

    tonal->last_transition++;
    alpha = 1.f/IMIN(20, 1+tonal->count);
    alphaE = 1.f/IMIN(50, 1+tonal->count);
    alphaE2 = 1.f/IMIN(6000, 1+tonal->count);

    if (tonal->count<4)
       tonal->music_prob = .5;
    kfft = mode->mdct.kfft[0];
    if (C==1)
    {
       for (i=0;i<N2;i++)
       {
          float w = analysis_window[i];
          in[i].r = MULT16_16(w, x[i]);
          in[i].i = MULT16_16(w, x[N-N2+i]);
          in[N-i-1].r = MULT16_16(w, x[N-i-1]);
          in[N-i-1].i = MULT16_16(w, x[2*N-N2-i-1]);
       }
    } else {
       for (i=0;i<N2;i++)
       {
          float w = analysis_window[i];
          in[i].r = MULT16_16(w, x[2*i]+x[2*i+1]);
          in[i].i = MULT16_16(w, x[2*(N-N2+i)]+x[2*(N-N2+i)+1]);
          in[N-i-1].r = MULT16_16(w, x[2*(N-i-1)]+x[2*(N-i-1)+1]);
          in[N-i-1].i = MULT16_16(w, x[2*(2*N-N2-i-1)]+x[2*(2*N-N2-i-1)+1]);
       }
    }
    opus_fft(kfft, in, out);

    for (i=1;i<N2;i++)
    {
       float X1r, X2r, X1i, X2i;
       float angle, d_angle, d2_angle;
       float angle2, d_angle2, d2_angle2;
       float mod1, mod2, avg_mod;
       X1r = out[i].r+out[N-i].r;
       X1i = out[i].i-out[N-i].i;
       X2r = out[i].i+out[N-i].i;
       X2i = out[N-i].r-out[i].r;

       angle = (float)(.5f/M_PI)*fast_atan2f(X1i, X1r);
       d_angle = angle - A[i];
       d2_angle = d_angle - dA[i];

       angle2 = (float)(.5f/M_PI)*fast_atan2f(X2i, X2r);
       d_angle2 = angle2 - angle;
       d2_angle2 = d_angle2 - d_angle;

       mod1 = d2_angle - (float)floor(.5+d2_angle);
       noisiness[i] = ABS16(mod1);
       mod1 *= mod1;
       mod1 *= mod1;

       mod2 = d2_angle2 - (float)floor(.5+d2_angle2);
       noisiness[i] += ABS16(mod2);
       mod2 *= mod2;
       mod2 *= mod2;

       avg_mod = .25f*(d2A[i]+2.f*mod1+mod2);
       tonality[i] = 1.f/(1.f+40.f*16.f*pi4*avg_mod)-.015f;

       A[i] = angle2;
       dA[i] = d_angle2;
       d2A[i] = mod2;
    }

    frame_tonality = 0;
    max_frame_tonality = 0;
    /*tw_sum = 0;*/
    info->activity = 0;
    frame_noisiness = 0;
    frame_stationarity = 0;
    if (!tonal->count)
    {
       for (b=0;b<NB_TBANDS;b++)
       {
          tonal->lowE[b] = 1e10;
          tonal->highE[b] = -1e10;
       }
    }
    relativeE = 0;
    frame_loudness = 0;
    bandwidth_mask = 0;
    for (b=0;b<NB_TBANDS;b++)
    {
       float E=0, tE=0, nE=0;
       float L1, L2;
       float stationarity;
       for (i=tbands[b];i<tbands[b+1];i++)
       {
          float binE = out[i].r*out[i].r + out[N-i].r*out[N-i].r
                     + out[i].i*out[i].i + out[N-i].i*out[N-i].i;
          E += binE;
          tE += binE*tonality[i];
          nE += binE*2.f*(.5f-noisiness[i]);
       }
       tonal->E[tonal->E_count][b] = E;
       frame_noisiness += nE/(1e-15f+E);

       frame_loudness += celt_sqrt(E+1e-10f);
       logE[b] = (float)log(E+1e-10f);
       tonal->lowE[b] = MIN32(logE[b], tonal->lowE[b]+.01f);
       tonal->highE[b] = MAX32(logE[b], tonal->highE[b]-.1f);
       if (tonal->highE[b] < tonal->lowE[b]+1.f)
       {
          tonal->highE[b]+=.5f;
          tonal->lowE[b]-=.5f;
       }
       relativeE += (logE[b]-tonal->lowE[b])/(EPSILON+tonal->highE[b]-tonal->lowE[b]);

       L1=L2=0;
       for (i=0;i<NB_FRAMES;i++)
       {
          L1 += celt_sqrt(tonal->E[i][b]);
          L2 += tonal->E[i][b];
       }

       stationarity = MIN16(0.99f,L1/celt_sqrt(EPSILON+NB_FRAMES*L2));
       stationarity *= stationarity;
       stationarity *= stationarity;
       frame_stationarity += stationarity;
       /*band_tonality[b] = tE/(1e-15+E)*/;
       band_tonality[b] = MAX16(tE/(EPSILON+E), stationarity*tonal->prev_band_tonality[b]);
#if 0
       if (b>=NB_TONAL_SKIP_BANDS)
       {
          frame_tonality += tweight[b]*band_tonality[b];
          tw_sum += tweight[b];
       }
#else
       frame_tonality += band_tonality[b];
       if (b>=NB_TBANDS-NB_TONAL_SKIP_BANDS)
          frame_tonality -= band_tonality[b-NB_TBANDS+NB_TONAL_SKIP_BANDS];
#endif
       max_frame_tonality = MAX16(max_frame_tonality, (1.f+.03f*(b-NB_TBANDS))*frame_tonality);
       slope += band_tonality[b]*(b-8);
       /*printf("%f %f ", band_tonality[b], stationarity);*/
       tonal->prev_band_tonality[b] = band_tonality[b];
    }

    bandwidth_mask = 0;
    bandwidth = 0;
    for (b=0;b<NB_TOT_BANDS;b++)
       maxE = MAX32(maxE, tonal->meanE[b]);
    noise_floor = 5.7e-4f/(1<<(IMAX(0,lsb_depth-8)));
    noise_floor *= noise_floor;
    for (b=0;b<NB_TOT_BANDS;b++)
    {
       float E=0;
       int band_start, band_end;
       /* Keep a margin of 300 Hz for aliasing */
       band_start = extra_bands[b]+3;
       band_end = extra_bands[b+1]+3;
       for (i=band_start;i<band_end;i++)
       {
          float binE = out[i].r*out[i].r + out[N-i].r*out[N-i].r
                     + out[i].i*out[i].i + out[N-i].i*out[N-i].i;
          E += binE;
       }
       E /= (band_end-band_start);
       maxE = MAX32(maxE, E);
       if (tonal->count>2)
       {
          tonal->meanE[b] = (1-alphaE2)*tonal->meanE[b] + alphaE2*E;
       } else {
          tonal->meanE[b] = E;
       }
       E = MAX32(E, tonal->meanE[b]);
       /* 13 dB slope for spreading function */
       bandwidth_mask = MAX32(.05f*bandwidth_mask, E);
       /* Checks if band looks like stationary noise or if it's below a (trivial) masking curve */
       if (E>.1*bandwidth_mask && E*1e10f > maxE && E > noise_floor)
          bandwidth = b;
    }
    if (tonal->count<=2)
       bandwidth = 20;
    frame_loudness = 20*(float)log10(frame_loudness);
    tonal->Etracker = MAX32(tonal->Etracker-.03f, frame_loudness);
    tonal->lowECount *= (1-alphaE);
    if (frame_loudness < tonal->Etracker-30)
       tonal->lowECount += alphaE;

    for (i=0;i<8;i++)
    {
       float sum=0;
       for (b=0;b<16;b++)
          sum += dct_table[i*16+b]*logE[b];
       BFCC[i] = sum;
    }

    frame_stationarity /= NB_TBANDS;
    relativeE /= NB_TBANDS;
    if (tonal->count<10)
       relativeE = .5;
    frame_noisiness /= NB_TBANDS;
#if 1
    info->activity = frame_noisiness + (1-frame_noisiness)*relativeE;
#else
    info->activity = .5*(1+frame_noisiness-frame_stationarity);
#endif
    frame_tonality = (max_frame_tonality/(NB_TBANDS-NB_TONAL_SKIP_BANDS));
    frame_tonality = MAX16(frame_tonality, tonal->prev_tonality*.8f);
    tonal->prev_tonality = frame_tonality;

    slope /= 8*8;
    info->tonality_slope = slope;

    tonal->E_count = (tonal->E_count+1)%NB_FRAMES;
    tonal->count++;
    info->tonality = frame_tonality;

    for (i=0;i<4;i++)
       features[i] = -0.12299f*(BFCC[i]+tonal->mem[i+24]) + 0.49195f*(tonal->mem[i]+tonal->mem[i+16]) + 0.69693f*tonal->mem[i+8] - 1.4349f*tonal->cmean[i];

    for (i=0;i<4;i++)
       tonal->cmean[i] = (1-alpha)*tonal->cmean[i] + alpha*BFCC[i];

    for (i=0;i<4;i++)
        features[4+i] = 0.63246f*(BFCC[i]-tonal->mem[i+24]) + 0.31623f*(tonal->mem[i]-tonal->mem[i+16]);
    for (i=0;i<3;i++)
        features[8+i] = 0.53452f*(BFCC[i]+tonal->mem[i+24]) - 0.26726f*(tonal->mem[i]+tonal->mem[i+16]) -0.53452f*tonal->mem[i+8];

    if (tonal->count > 5)
    {
       for (i=0;i<9;i++)
          tonal->std[i] = (1-alpha)*tonal->std[i] + alpha*features[i]*features[i];
    }

    for (i=0;i<8;i++)
    {
       tonal->mem[i+24] = tonal->mem[i+16];
       tonal->mem[i+16] = tonal->mem[i+8];
       tonal->mem[i+8] = tonal->mem[i];
       tonal->mem[i] = BFCC[i];
    }
    for (i=0;i<9;i++)
       features[11+i] = celt_sqrt(tonal->std[i]);
    features[20] = info->tonality;
    features[21] = info->activity;
    features[22] = frame_stationarity;
    features[23] = info->tonality_slope;
    features[24] = tonal->lowECount;

#ifndef FIXED_POINT
    mlp_process(&net, features, &frame_prob);
    frame_prob = .5f*(frame_prob+1);
    /* Curve fitting between the MLP probability and the actual probability */
    frame_prob = .01f + 1.21f*frame_prob*frame_prob - .23f*(float)pow(frame_prob, 10);

    /*printf("%f\n", frame_prob);*/
    {
       float tau, beta;
       float p0, p1;
       float max_certainty;
       /* One transition every 3 minutes */
       tau = .00005f;
       beta = .1f;
       max_certainty = .01f+1.f/(20.f+.5f*tonal->last_transition);
       p0 = (1-tonal->music_prob)*(1-tau) +    tonal->music_prob *tau;
       p1 =    tonal->music_prob *(1-tau) + (1-tonal->music_prob)*tau;
       p0 *= (float)pow(1-frame_prob, beta);
       p1 *= (float)pow(frame_prob, beta);
       tonal->music_prob = MAX16(max_certainty, MIN16(1-max_certainty, p1/(p0+p1)));
       info->music_prob = tonal->music_prob;
       /*printf("%f %f\n", frame_prob, info->music_prob);*/
    }
    if (tonal->last_music != (tonal->music_prob>.5f))
       tonal->last_transition=0;
    tonal->last_music = tonal->music_prob>.5f;
#else
    info->music_prob = 0;
#endif
    /*for (i=0;i<25;i++)
       printf("%f ", features[i]);
    printf("\n");*/

    if (bandwidth<=12 || (bandwidth==13 && tonal->opus_bandwidth == OPUS_BANDWIDTH_NARROWBAND))
       tonal->opus_bandwidth = OPUS_BANDWIDTH_NARROWBAND;
    else if (bandwidth<=14 || (bandwidth==15 && tonal->opus_bandwidth == OPUS_BANDWIDTH_MEDIUMBAND))
       tonal->opus_bandwidth = OPUS_BANDWIDTH_MEDIUMBAND;
    else if (bandwidth<=16 || (bandwidth==17 && tonal->opus_bandwidth == OPUS_BANDWIDTH_WIDEBAND))
       tonal->opus_bandwidth = OPUS_BANDWIDTH_WIDEBAND;
    else if (bandwidth<=18)
       tonal->opus_bandwidth = OPUS_BANDWIDTH_SUPERWIDEBAND;
    else
       tonal->opus_bandwidth = OPUS_BANDWIDTH_FULLBAND;

    info->bandwidth = bandwidth;
    info->opus_bandwidth = tonal->opus_bandwidth;
    /*printf("%d %d\n", info->bandwidth, info->opus_bandwidth);*/
    info->noisiness = frame_noisiness;
    info->valid = 1;
}
