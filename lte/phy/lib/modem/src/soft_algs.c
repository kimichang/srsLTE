/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2014 The libLTE Developers. See the
 * COPYRIGHT file at the top-level directory of this distribution.
 *
 * \section LICENSE
 *
 * This file is part of the libLTE library.
 *
 * libLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * A copy of the GNU Lesser General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */



#include <stdio.h>
#include <math.h>
#include <complex.h>
#include <stdint.h>

#include "soft_algs.h"
#include "liblte/phy/utils/vector.h"

#define LLR_APPROX_USE_VOLK

#ifdef LLR_APPROX_USE_VOLK

typedef _Complex float cf_t; 

float d[10000][64];
float num[10000], den[10000];

static void compute_square_dist(const cf_t *in, cf_t *symbols, int N, int M) {
  int s;
  float *d_ptr; 
  for (s = 0; s < N; s++) {     
    d_ptr = d[s];
    vec_square_dist(in[s], symbols, d_ptr, M);
  }
}

static void compute_min_dist(uint32_t (*S)[6][32], int N, int B, int M) {
  int s, b, i;
  for (s = 0; s < N; s++) {     
    for (b = 0; b < B; b++) {   /* bits per symbol */
      /* initiate num[b] and den[b] */
      num[s * B + b] = 1e10;
      den[s * B + b] = 1e10;

      for (i = 0; i < M / 2; i++) {     
        if (d[s][S[0][b][i]] < num[s * B + b]) {
          num[s * B + b] = d[s][S[0][b][i]];
        }
        if (d[s][S[1][b][i]] < den[s * B + b]) {
          den[s * B + b] = d[s][S[1][b][i]];
        }
      }
    }
  }
}

static void compute_llr(int N, int B, float sigma2, float *out) {
  int s, b;
  for (s = 0; s < N; s++) {     
    for (b = 0; b < B; b++) {   /* bits per symbol */
      out[s * B + b] = (num[s * B + b]-den[s * B + b]) / sigma2;
    }
  }
}

void llr_approx(const _Complex float *in, float *out, int N, int M, int B,
           _Complex float *symbols, uint32_t(*S)[6][32], float sigma2)
{
  
  if (M <= 64) {
    compute_square_dist(in, symbols, N, M);

    compute_min_dist(S, N, B, M);
    
    compute_llr(N, B, sigma2, out);
  }
}

#else 

/**
 * @ingroup Soft Modulation Demapping based on the approximate
 * log-likelihood algorithm
 * Common algorithm that approximates the log-likelihood ratio. It takes
 * only the two closest constellation symbols into account, one with a '0'
 * and the other with a '1' at the given bit position.
 *
 * \param in input symbols (_Complex float)
 * \param out output symbols (float)
 * \param N Number of input symbols
 * \param M Number of constellation points
 * \param B Number of bits per symbol
 * \param symbols constellation symbols
 * \param S Soft demapping auxiliary matrix
 * \param sigma2 Noise vatiance
 */
void
llr_approx(const _Complex float *in, float *out, int N, int M, int B,
           _Complex float *symbols, uint32_t(*S)[6][32], float sigma2)
{
  int i, s, b;
  float num, den;
  int change_sign = -1;
  float x, y, d[64];

  for (s = 0; s < N; s++) {     /* recevied symbols */
    /* Compute the distances squared d[i] between the received symbol and all constellation points */
    for (i = 0; i < M; i++) {
      x = __real__ in[s] - __real__ symbols[i];
      y = __imag__ in[s] - __imag__ symbols[i];
      d[i] = x * x + y * y;
    }

    for (b = 0; b < B; b++) {   /* bits per symbol */
      /* initiate num[b] and den[b] */
      num = d[S[0][b][0]];
      den = d[S[1][b][0]];

      /* Minimum distance squared search between recevied symbol and a constellation point with a
         '1' and a '0' for each bit position */
      for (i = 1; i < M / 2; i++) {     /* half the constellation points have '1'|'0' at any given bit position */
        if (d[S[0][b][i]] < num) {
          num = d[S[0][b][i]];
        }
        if (d[S[1][b][i]] < den) {
          den = d[S[1][b][i]];
        }
      }
      /* Theoretical LLR and approximate LLR values are positive if
       * symbol(s) with '0' is/are closer and negative if symbol(s)
       * with '1' are closer.
       * Change sign if mapping negative to '0' and positive to '1' */
      out[s * B + b] = change_sign * (den - num) / sigma2;
      if (s<10)
        printf("out[%d]=%f=%f/%f\n",s*B+b,out[s*B+b], num,den);
    }
  }

}

#endif

/**
 * @ingroup Soft Modulation Demapping based on the approximate
 * log-likelihood ratio algorithm
 * Common algorithm that approximates the log-likelihood ratio. It takes
 * only the two closest constellation symbols into account, one with a '0'
 * and the other with a '1' at the given bit position.
 *
 * \param in input symbols (_Complex float)
 * \param out output symbols (float)
 * \param N Number of input symbols
 * \param M Number of constellation points
 * \param B Number of bits per symbol
 * \param symbols constellation symbols
 * \param S Soft demapping auxiliary matrix
 * \param sigma2 Noise vatiance
 */
void llr_exact(const _Complex float *in, float *out, int N, int M, int B,
          _Complex float *symbols, uint32_t(*S)[6][32], float sigma2)
{
  int i, s, b;
  float num, den;
  int change_sign = -1;
  float x, y, d[64];

  for (s = 0; s < N; s++) {     /* recevied symbols */
    /* Compute exp{·} of the distances squared d[i] between the received symbol and all constellation points */
    for (i = 0; i < M; i++) {
      x = __real__ in[s] - __real__ symbols[i];
      y = __imag__ in[s] - __imag__ symbols[i];
      d[i] = exp(-1 * (x * x + y * y) / sigma2);
    }

    /* Sum up the corresponding d[i]'s for each bit position */
    for (b = 0; b < B; b++) {   /* bits per symbol */
      /* initiate num[b] and den[b] */
      num = 0;
      den = 0;

      for (i = 0; i < M / 2; i++) {     /* half the constellation points have '1'|'0' at any given bit position */
        num += d[S[0][b][i]];
        den += d[S[1][b][i]];
      }
      /* Theoretical LLR and approximate LLR values are positive if
       * symbol(s) with '0' is/are closer and negative if symbol(s)
       * with '1' are closer.
       * Change sign if mapping negative to '0' and positive to '1' */
      out[s * B + b] = change_sign * log(num / den);
    }
  }
}
