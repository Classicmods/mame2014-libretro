/* libFLAC - Free Lossless Audio Codec library
 * Copyright (C) 2000-2009  Josh Coalson
 * Copyright (C) 2011-2014  Xiph.Org Foundation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of the Xiph.org Foundation nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef FLAC__INTEGER_ONLY_LIBRARY
#ifndef FLAC__NO_ASM
#if (defined FLAC__CPU_IA32 || defined FLAC__CPU_X86_64) && defined FLAC__HAS_X86INTRIN
#include "private/fixed.h"
#ifdef FLAC__SSE2_SUPPORTED

#include <emmintrin.h> /* SSE2 */
#include <math.h>
#include "private/macros.h"
#include "share/compat.h"
#include "FLAC/assert.h"

#ifdef FLAC__CPU_IA32
union zz_cvt
{
	int64_t i64;
	int32_t i32[2];
};

FLAC__SSE_TARGET("sse2")
static inline int64_t _zz_cvtsi128_si64(__m128i a)
{
	union zz_cvt z;
	z.i32[0] = _mm_cvtsi128_si32(a);
	z.i32[1] = _mm_cvtsi128_si32(_mm_srli_si128(a, 4));
	return z.i64;
}

#define _mm_cvtsi128_si64 _zz_cvtsi128_si64
#endif

FLAC__SSE_TARGET("sse2")
unsigned FLAC__fixed_compute_best_predictor_intrin_sse2(const FLAC__int32 data[], unsigned data_len, FLAC__float residual_bits_per_sample[FLAC__MAX_FIXED_ORDER + 1])
{
	FLAC__uint32 total_error_0, total_error_1, total_error_2, total_error_3, total_error_4;
	unsigned i, order;

	__m128i total_err0, total_err1, total_err2;

	{
		FLAC__int32 itmp;
		__m128i last_error;

		last_error = _mm_cvtsi32_si128(data[-1]);							// 0   0   0   le0
		itmp = data[-2];
		last_error = _mm_shuffle_epi32(last_error, _MM_SHUFFLE(2,1,0,0));
		last_error = _mm_sub_epi32(last_error, _mm_cvtsi32_si128(itmp));	// 0   0   le0 le1
		itmp -= data[-3];
		last_error = _mm_shuffle_epi32(last_error, _MM_SHUFFLE(2,1,0,0));
		last_error = _mm_sub_epi32(last_error, _mm_cvtsi32_si128(itmp));	// 0   le0 le1 le2
		itmp -= data[-3] - data[-4];
		last_error = _mm_shuffle_epi32(last_error, _MM_SHUFFLE(2,1,0,0));
		last_error = _mm_sub_epi32(last_error, _mm_cvtsi32_si128(itmp));	// le0 le1 le2 le3

		total_err0 = total_err1 = _mm_setzero_si128();
		for(i = 0; i < data_len; i++) {
			__m128i err0, err1, tmp;
			err0 = _mm_cvtsi32_si128(data[i]);								// 0   0   0   e0
			err1 = _mm_shuffle_epi32(err0, _MM_SHUFFLE(0,0,0,0));			// e0  e0  e0  e0
			err1 = _mm_sub_epi32(err1, last_error);
			last_error = _mm_srli_si128(last_error, 4);						// 0   le0 le1 le2
			err1 = _mm_sub_epi32(err1, last_error);
			last_error = _mm_srli_si128(last_error, 4);						// 0   0   le0 le1
			err1 = _mm_sub_epi32(err1, last_error);
			last_error = _mm_srli_si128(last_error, 4);						// 0   0   0   le0
			err1 = _mm_sub_epi32(err1, last_error);							// e1  e2  e3  e4

			tmp = _mm_slli_si128(err0, 12);									// e0   0   0   0
			last_error = _mm_srli_si128(err1, 4);							//  0  e0  e1  e2
			last_error = _mm_or_si128(last_error, tmp);						// e0  e1  e2  e3

			tmp = _mm_srai_epi32(err0, 31);
			err0 = _mm_xor_si128(err0, tmp);
			err0 = _mm_sub_epi32(err0, tmp);
			tmp = _mm_srai_epi32(err1, 31);
			err1 = _mm_xor_si128(err1, tmp);
			err1 = _mm_sub_epi32(err1, tmp);

			total_err0 = _mm_add_epi32(total_err0, err0);					// 0   0   0   te0
			total_err1 = _mm_add_epi32(total_err1, err1);					// te1 te2 te3 te4
		}
	}
	
	total_error_0 = _mm_cvtsi128_si32(total_err0);
	total_err2 = total_err1;											// te1  te2  te3  te4
	total_err1 = _mm_srli_si128(total_err1, 8);							//  0    0   te1  te2
	total_error_4 = _mm_cvtsi128_si32(total_err2);
	total_error_2 = _mm_cvtsi128_si32(total_err1);
	total_err2 = _mm_srli_si128(total_err2,	4);							//  0   te1  te2  te3
	total_err1 = _mm_srli_si128(total_err1, 4);							//  0    0    0   te1
	total_error_3 = _mm_cvtsi128_si32(total_err2);
	total_error_1 = _mm_cvtsi128_si32(total_err1);

	/* prefer higher order */
	if(total_error_0 < flac_min(flac_min(flac_min(total_error_1, total_error_2), total_error_3), total_error_4))
		order = 0;
	else if(total_error_1 < flac_min(flac_min(total_error_2, total_error_3), total_error_4))
		order = 1;
	else if(total_error_2 < flac_min(total_error_3, total_error_4))
		order = 2;
	else if(total_error_3 < total_error_4)
		order = 3;
	else
		order = 4;

	/* Estimate the expected number of bits per residual signal sample. */
	/* 'total_error*' is linearly related to the variance of the residual */
	/* signal, so we use it directly to compute E(|x|) */
	FLAC__ASSERT(data_len > 0 || total_error_0 == 0);
	FLAC__ASSERT(data_len > 0 || total_error_1 == 0);
	FLAC__ASSERT(data_len > 0 || total_error_2 == 0);
	FLAC__ASSERT(data_len > 0 || total_error_3 == 0);
	FLAC__ASSERT(data_len > 0 || total_error_4 == 0);

	residual_bits_per_sample[0] = (FLAC__float)((total_error_0 > 0) ? log(M_LN2 * (FLAC__double)total_error_0 / (FLAC__double)data_len) / M_LN2 : 0.0);
	residual_bits_per_sample[1] = (FLAC__float)((total_error_1 > 0) ? log(M_LN2 * (FLAC__double)total_error_1 / (FLAC__double)data_len) / M_LN2 : 0.0);
	residual_bits_per_sample[2] = (FLAC__float)((total_error_2 > 0) ? log(M_LN2 * (FLAC__double)total_error_2 / (FLAC__double)data_len) / M_LN2 : 0.0);
	residual_bits_per_sample[3] = (FLAC__float)((total_error_3 > 0) ? log(M_LN2 * (FLAC__double)total_error_3 / (FLAC__double)data_len) / M_LN2 : 0.0);
	residual_bits_per_sample[4] = (FLAC__float)((total_error_4 > 0) ? log(M_LN2 * (FLAC__double)total_error_4 / (FLAC__double)data_len) / M_LN2 : 0.0);

	return order;
}

FLAC__SSE_TARGET("sse2")
unsigned FLAC__fixed_compute_best_predictor_wide_intrin_sse2(const FLAC__int32 data[], unsigned data_len, FLAC__float residual_bits_per_sample[FLAC__MAX_FIXED_ORDER + 1])
{
	FLAC__uint64 total_error_0, total_error_1, total_error_2, total_error_3, total_error_4;
	unsigned i, order;

	__m128i total_err0, total_err1, total_err3;

	{
		FLAC__int32 itmp;
		__m128i last_error, zero = _mm_setzero_si128();
		
		last_error = _mm_cvtsi32_si128(data[-1]);							// 0   0   0   le0
		itmp = data[-2];
		last_error = _mm_shuffle_epi32(last_error, _MM_SHUFFLE(2,1,0,0));
		last_error = _mm_sub_epi32(last_error, _mm_cvtsi32_si128(itmp));	// 0   0   le0 le1
		itmp -= data[-3];
		last_error = _mm_shuffle_epi32(last_error, _MM_SHUFFLE(2,1,0,0));
		last_error = _mm_sub_epi32(last_error, _mm_cvtsi32_si128(itmp));	// 0   le0 le1 le2
		itmp -= data[-3] - data[-4];
		last_error = _mm_shuffle_epi32(last_error, _MM_SHUFFLE(2,1,0,0));
		last_error = _mm_sub_epi32(last_error, _mm_cvtsi32_si128(itmp));	// le0 le1 le2 le3

		total_err0 = total_err1 = total_err3 = _mm_setzero_si128();
		for(i = 0; i < data_len; i++) {
			__m128i err0, err1, tmp;
			err0 = _mm_cvtsi32_si128(data[i]);								// 0   0   0   e0
			err1 = _mm_shuffle_epi32(err0, _MM_SHUFFLE(0,0,0,0));			// e0  e0  e0  e0
			err1 = _mm_sub_epi32(err1, last_error);
			last_error = _mm_srli_si128(last_error, 4);						// 0   le0 le1 le2
			err1 = _mm_sub_epi32(err1, last_error);
			last_error = _mm_srli_si128(last_error, 4);						// 0   0   le0 le1
			err1 = _mm_sub_epi32(err1, last_error);
			last_error = _mm_srli_si128(last_error, 4);						// 0   0   0   le0
			err1 = _mm_sub_epi32(err1, last_error);							// e1  e2  e3  e4

			tmp = _mm_slli_si128(err0, 12);									// e0   0   0   0
			last_error = _mm_srli_si128(err1, 4);							//  0  e0  e1  e2
			last_error = _mm_or_si128(last_error, tmp);						// e0  e1  e2  e3

			tmp = _mm_srai_epi32(err0, 31);
			err0 = _mm_xor_si128(err0, tmp);
			err0 = _mm_sub_epi32(err0, tmp);
			tmp = _mm_srai_epi32(err1, 31);
			err1 = _mm_xor_si128(err1, tmp);
			err1 = _mm_sub_epi32(err1, tmp);

			total_err0 = _mm_add_epi64(total_err0, err0);					//        0       te0
			err0 = _mm_unpacklo_epi32(err1, zero);							//   0  |e3|   0  |e4|
			err1 = _mm_unpackhi_epi32(err1, zero);							//   0  |e1|   0  |e2|
			total_err3 = _mm_add_epi64(total_err3, err0);					//       te3      te4
			total_err1 = _mm_add_epi64(total_err1, err1);					//       te1      te2
		}
	}
	
	total_error_0 = _mm_cvtsi128_si64(total_err0);
	total_error_4 = _mm_cvtsi128_si64(total_err3);
	total_error_2 = _mm_cvtsi128_si64(total_err1);
	total_err3 = _mm_srli_si128(total_err3,	8);							//         0      te3
	total_err1 = _mm_srli_si128(total_err1, 8);							//         0      te1
	total_error_3 = _mm_cvtsi128_si64(total_err3);
	total_error_1 = _mm_cvtsi128_si64(total_err1);

	/* prefer higher order */
	if(total_error_0 < flac_min(flac_min(flac_min(total_error_1, total_error_2), total_error_3), total_error_4))
		order = 0;
	else if(total_error_1 < flac_min(flac_min(total_error_2, total_error_3), total_error_4))
		order = 1;
	else if(total_error_2 < flac_min(total_error_3, total_error_4))
		order = 2;
	else if(total_error_3 < total_error_4)
		order = 3;
	else
		order = 4;

	/* Estimate the expected number of bits per residual signal sample. */
	/* 'total_error*' is linearly related to the variance of the residual */
	/* signal, so we use it directly to compute E(|x|) */
	FLAC__ASSERT(data_len > 0 || total_error_0 == 0);
	FLAC__ASSERT(data_len > 0 || total_error_1 == 0);
	FLAC__ASSERT(data_len > 0 || total_error_2 == 0);
	FLAC__ASSERT(data_len > 0 || total_error_3 == 0);
	FLAC__ASSERT(data_len > 0 || total_error_4 == 0);

	residual_bits_per_sample[0] = (FLAC__float)((total_error_0 > 0) ? log(M_LN2 * (FLAC__double)total_error_0 / (FLAC__double)data_len) / M_LN2 : 0.0);
	residual_bits_per_sample[1] = (FLAC__float)((total_error_1 > 0) ? log(M_LN2 * (FLAC__double)total_error_1 / (FLAC__double)data_len) / M_LN2 : 0.0);
	residual_bits_per_sample[2] = (FLAC__float)((total_error_2 > 0) ? log(M_LN2 * (FLAC__double)total_error_2 / (FLAC__double)data_len) / M_LN2 : 0.0);
	residual_bits_per_sample[3] = (FLAC__float)((total_error_3 > 0) ? log(M_LN2 * (FLAC__double)total_error_3 / (FLAC__double)data_len) / M_LN2 : 0.0);
	residual_bits_per_sample[4] = (FLAC__float)((total_error_4 > 0) ? log(M_LN2 * (FLAC__double)total_error_4 / (FLAC__double)data_len) / M_LN2 : 0.0);

	return order;
}

#endif /* FLAC__SSE2_SUPPORTED */
#endif /* (FLAC__CPU_IA32 || FLAC__CPU_X86_64) && FLAC__HAS_X86INTRIN */
#endif /* FLAC__NO_ASM */
#endif /* FLAC__INTEGER_ONLY_LIBRARY */
