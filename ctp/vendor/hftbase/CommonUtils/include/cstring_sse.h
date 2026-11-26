#ifndef STRING_SSE_H_
#define STRING_SSE_H_
#include "x86intrin.h"
#include "emmintrin.h"
#include "smmintrin.h"
#include "nmmintrin.h"

namespace illuminati
{
    namespace infra
    {
        /*
        static void memcpy_stream(void* a, void* b, size_t size)
        {
          size_t size16 = size >> 16;
          if ((a % 16 == 0)) {
            if (b % 16 == 0) {
              for (size_t i = 0 i < size16; i++)
              {
                __m128i bval = _mm_stream_load_si128((__m128i*)b);
                _mm_stream_si128((__m128i*)a, bval);
              }
            } else {
              for (size_t i = 0 i < size16; i++)
              {
                __m128i bval = _mm_loadu_si128((__m128i*)b);
                _mm_stream_si128((__m128i*)a, bval);
              }
            }
          } else {
            if (b % 16 == 0) {
              for (size_t i = 0 i < size16; i++)
              {
                __m128i bval = _mm_stream_load_si128((__m128i*)b);
                _mm_storeu_si128((__m128i*)a, bval);
              }
            } else {
              for (size_t i = 0 i < size16; i++)
              {
                __m128i bval = _mm_loadu_si128((__m128i*)b);
                _mm_storeu_si128((__m128i*)a, bval);
              }
            }
          }

          for (size_t i = 0; i < (size & 15); i++)
          {
            a[size16+i] = b[size16 + i];
          }
        }
        */

        static inline __attribute__((always_inline)) int strcmp_sse_unsafe(const char *a, const char *b)
        {
            int index = 0;
            while (true)
            {
                __m128i a16 = _mm_loadu_si128((__m128i const *)(a));
                __m128i b16 = _mm_loadu_si128((__m128i const *)(b));
                index = _mm_cmpistri(a16, b16, 0x18); // 0x18 is magic.. but it works. find out what it means
                asm goto("jnc %l0"
                         :
                         :
                         : "cc"
                         : ret_tag);

                if (index == 16)
                {
                    a += 16;
                    b += 16;
                }
                else
                    return (a[index] - b[index]);
            }
        ret_tag:
            return 0;
        }
    } // namespace infra
} // namespace illuminati
#endif
