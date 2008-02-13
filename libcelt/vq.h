/* (C) 2007 Jean-Marc Valin, CSIRO
*/
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   
   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   
   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
   
   - Neither the name of the Xiph.org Foundation nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.
   
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

#ifndef VQ_H
#define VQ_H

#include "entenc.h"
#include "entdec.h"


/** Algebraic pulse-based quantiser. The signal x is replaced by the sum of the
 * pitch a combination of pulses such that its norm is still equal to 1. The
 * only difference with the quantiser above is that the search is more complete. 
 * @param x Residual signal to quantise/encode (returns quantised version)
 * @param W Perceptual weight
 * @param N Number of samples to encode
 * @param K Number of pulses to use
 * @param p Pitch vector (it is assumed that p+x is a unit vector)
 * @param alpha compression factor in the pitch direction (magic!)
 * @param enc Entropy encoder state
*/
void alg_quant(float *x, float *W, int N, int K, float *p, float alpha, ec_enc *enc);

/** Algebraic pulse decoder
 * @param x Decoded normalised spectrum (returned)
 * @param N Number of samples to decode
 * @param K Number of pulses to use
 * @param p Pitch vector (automatically added to x)
 * @param alpha compression factor in the pitch direction (magic!)
 * @param dec Entropy decoder state
 */
void alg_unquant(float *x, int N, int K, float *p, float alpha, ec_dec *dec);

/** Intra-frame predictor that matches a section of the current frame (at lower
 * frequencies) to encode the current band.
 * @param x Residual signal to quantise/encode (returns quantised version)
 * @param W Perceptual weight
 * @param N Number of samples to encode
 * @param K Number of pulses to use
 * @param Y Lower frequency spectrum to use, normalised to the same standard deviation
 * @param p Pitch vector (it is assumed that p+x is a unit vector)
 * @param B Stride (number of channels multiplied by the number of MDCTs per frame)
 * @param N0 Number of valid offsets
 * @param enc Entropy encoder state
 */
void intra_prediction(float *x, float *W, int N, int K, float *Y, float *P, int B, int N0, ec_enc *enc);

void intra_unquant(float *x, int N, int K, float *Y, float *P, int B, int N0, ec_dec *dec);

/** Encode the entire band as a "fold" from other parts of the spectrum. No bits required (only use is case of an emergency!) */
void intra_fold(float *x, int N, float *Y, float *P, int B, int N0, int Nmax);

#endif /* VQ_H */
