#pragma once
#include <cstdint>

template <class T> class LCG;

////////////////////////////////////////////////////////////////////////////////
// AVX
#include "avx_mathfun.h"

_PS256_CONST_TYPE(lcg_a, uint32_t, 1664525);
_PS256_CONST_TYPE(lcg_b, uint32_t, 1013904223);
_PS256_CONST_TYPE(lcg_mask, uint32_t, 0x3F800000);
// AVX2_INTOP_USING_SSE2(mullo_epi32); // Actually uses SSE4.1 _mm_mullo_epi32()
// AVX2_INTOP_USING_SSE2(or_si128);

template <> class LCG<__m256> {
public:
  LCG() : x(_mm256_setr_epi32(1, 2, 3, 4, 5, 6, 7, 8)) {}
  LCG(int a, int b, int c, int d, int e, int f, int g, int h)
      : x(_mm256_setr_epi32(a, b, c, d, e, f, g, h)) {}

  __m256 operator()() {
    x = _mm256_add_epi32(_mm256_mullo_epi32(x, *(__m256i *)_ps256_lcg_a),
                         *(__m256i *)_ps256_lcg_b);
    __m256i u =
        _mm256_or_si256(_mm256_srli_epi32(x, 9), *(__m256i *)_ps256_lcg_mask);
    __m256 f = _mm256_sub_ps(_mm256_castsi256_ps(u), *(__m256 *)_ps256_1);
    return f;
  }

private:
  __m256i x __attribute__((aligned(32)));
};
