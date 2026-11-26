#ifndef STRINGCONVERSION_H_
#define STRINGCONVERSION_H_
#include <cstring>
#include <cmath>
#include <type_traits>
#include <string>

#include <smmintrin.h>
#include <x86intrin.h>
#include <emmintrin.h>

namespace illuminati
{

    // General declaration
    // Template specializations sometimes add more arguments to ToString.
    // See below
    template <typename T, typename Enable = void>
    class StringConversion
    {
    public:
        inline __attribute__((always_inline)) static char *ToString(T t, char *buf, const size_t len, size_t &finalLength, bool moveToBeg = false);

        inline __attribute__((always_inline)) static T FromString(const char *buf, size_t len);
    };

    /********************** Integer Conversion **********************************/
    /* Needs -std=c++11
    template<typename T>
    using EnableIfIntType =
            typename std::enable_if<std::is_integral<T>::value>::type;
  */
#define QWCG10to8 0xabcc77118461cefdull
#define QWCONST10to8 100000000ull
/* macro to convert input parameter of short integer "hi4" into output variable "x3" which is __m128i;
the input value "hi4" is assume to be less than 10^4;
the output is 4 single-digit integer between 0-9, located in the low byte of each dword,
most significant digit in lowest DW.
implicit overwrites: locally allocated __m128i variable "x0", "x2"
*/
#define __ParMod10to4SSSE3(x3, hi4)                                                                    \
    {                                                                                                  \
        x0 = _mm_shuffle_epi32(_mm_cvtsi32_si128((hi4)), 0);                                           \
        x2 = _mm_mulhi_epu16(x0, _mm_loadu_si128((__m128i *)quoTenThsn_mulplr_d));                     \
        x2 = _mm_srli_epi32(_mm_madd_epi16(x2, _mm_loadu_si128((__m128i *)quo4digComp_mulplr_d)), 10); \
        (x3) = _mm_insert_epi16(_mm_slli_si128(x2, 6), (int)(hi4), 1);                                 \
        (x3) = _mm_or_si128(x2, (x3));                                                                 \
        (x3) = _mm_madd_epi16((x3), _mm_loadu_si128((__m128i *)mten_mulplr_d));                        \
    }
    /* macro to convert input parameter of the 3rd dword element of "t5" ( __m128i type)
    into output variable "x3" which is __m128i;
    the third dword element "t5" is assume to be less than 10^4, the 4th dword must be 0;
    the output is 4 single-digit integer between 0-9, located in the low byte of each dword,
    MS digit in LS DW.
    implicit overwrites: locally allocated __m128i variable "x0", "x2"
    */

#define __ParMod10to4SSSE3v(x3, t5)                                                                    \
    {                                                                                                  \
        x0 = _mm_shuffle_epi32(t5, 0xaa);                                                              \
        x2 = _mm_mulhi_epu16(x0, _mm_loadu_si128((__m128i *)quoTenThsn_mulplr_d));                     \
        x2 = _mm_srli_epi32(_mm_madd_epi16(x2, _mm_loadu_si128((__m128i *)quo4digComp_mulplr_d)), 10); \
        (x3) = _mm_or_si128(_mm_slli_si128(x2, 6), _mm_srli_si128(t5, 6));                             \
        (x3) = _mm_or_si128(x2, (x3));                                                                 \
        (x3) = _mm_madd_epi16((x3), _mm_loadu_si128((__m128i *)mten_mulplr_d));                        \
    }
    static __attribute__((aligned(16))) short quo4digComp_mulplr_d[8] =
        {1024, 0, 64, 0, 8, 0, 0, 0};
    static __attribute__((aligned(16))) short quoTenThsn_mulplr_d[8] =
        {0x199a, 0, 0x28f6, 0, 0x20c5, 0, 0x1a37, 0};
    static __attribute__((aligned(16))) short mten_mulplr_d[8] =
        {-10, 1, -10, 1, -10, 1, -10, 1};
    static __attribute__((aligned(16))) unsigned short bcstpklodw[8] =
        {0x080c, 0x0004, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080};
    static __attribute__((aligned(16))) unsigned short bcstpkdw1[8] =
        {0x8080, 0x8080, 0x080c, 0x0004, 0x8080, 0x8080, 0x8080, 0x8080};
    static __attribute__((aligned(16))) unsigned short bcstpkdw2[8] =
        {0x8080, 0x8080, 0x8080, 0x8080, 0x080c, 0x0004, 0x8080, 0x8080};
    static __attribute__((aligned(16))) unsigned short bcstpkdw3[8] =
        {0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0x080c, 0x0004};
    static __attribute__((aligned(16))) int asc0bias[4] =
        {0x30, 0x30, 0x30, 0x30};
    static __attribute__((aligned(16))) int asc0reversebias[4] =
        {(int)0xd0d0d0d0, (int)0xd0d0d0d0, (int)0xd0d0d0d0, (int)0xd0d0d0d0};
    static __attribute__((aligned(16))) int pr_cg_10to4[4] =
        {0x68db8db, 0, 0x68db8db, 0};
    static __attribute__((aligned(16))) int pr_1_m10to4[4] =
        {-10000, 0, 1, 0};

    /*input value "xx" is less than 2^63-1 */
    /* In environment that does not support binary integer arithmetic on __int128_t,
    this helper can be done as asm routine
    */
    inline __attribute__((always_inline)) int64_t u64mod10to8(int64_t *pLo, int64_t xx)
    {
        __int128_t t, b = (__int128_t)QWCG10to8;
        int64_t q;
        t = b * (__int128_t)xx;
        q = t >> (64 + 26); // shift count associated with QWCG10to8
        *pLo = xx - QWCONST10to8 * q;
        return q;
    }

    /* convert input value into 4 single digits via parallel fixed-point arithmetic with each dword
    element, and pack each digit into low dword element and write to buffer without leading
    white space; input value must be < 10000 and > 9
    */
    inline __attribute__((always_inline)) int ubs_Lt10k_2s_i2(int x_Lt10k, char *ps)
    {
        int tmp;
        __m128i x0, m0, x2, x3, x4, compv;
        // Use a set of scaling constant to compensate for lack for per-element shift count
        compv = _mm_loadu_si128((__m128i *)quo4digComp_mulplr_d);
        // broadcast input value to each dword element
        x0 = _mm_shuffle_epi32(_mm_cvtsi32_si128(x_Lt10k), 0);
        // low to high dword in x0 : u16, u16, u16, u16
        m0 = _mm_loadu_si128((__m128i *)quoTenThsn_mulplr_d); // load 4 congruent consts
        x2 = _mm_mulhi_epu16(x0, m0);                         // parallel fixed-point multiply for base 10,100, 1000, 10000
        x2 = _mm_srli_epi32(_mm_madd_epi16(x2, compv), 10);
        // dword content in x2: u16/10, u16/100, u16/1000, u16/10000
        x3 = _mm_insert_epi16(_mm_slli_si128(x2, 6), (int)x_Lt10k, 1);
        // word content in x3: 0, u16, 0, u16/10, 0, u16/100, 0, u16/1000
        x4 = _mm_or_si128(x2, x3);
        // perform parallel remainder operation with each word pair to derive 4 unbiased single-digit result
        x4 = _mm_madd_epi16(x4, _mm_loadu_si128((__m128i *)mten_mulplr_d));
        x2 = _mm_add_epi32(x4, _mm_loadu_si128((__m128i *)asc0bias));
        // pack each ascii-biased digits from respective dword to the low dword element
        x3 = _mm_shuffle_epi8(x2, _mm_loadu_si128((__m128i *)bcstpklodw));

        // store ascii result to buffer without leading white space
        if (x_Lt10k > 999)
        {
            *(int *)ps = _mm_cvtsi128_si32(x3);
            return 4;
        }
        else if (x_Lt10k > 99)
        {
            tmp = _mm_cvtsi128_si32(x3);
            *ps = (char)(tmp >> 8);
            *((short *)(++ps)) = (short)(tmp >> 16);
            return 3;
        }
        else if (x_Lt10k > 9) // take advantage of reduced dynamic range > 9 to reduce branching
        {
            *((short *)ps) = (short)_mm_extract_epi16(x3, 1);
            return 2;
        }
        *ps = '0' + x_Lt10k;
        return 1;
    }

    /* convert integer between 2^63-1 and 0 to ASCII string */
    int sse4i_q2a_u63(int64_t xx, char *ps)
    {
        int j, tmp, idx = 0, cnt;
        int64_t lo8, hi8, abv16, temp;
        __m128i x0, m0, x1, x2, x3, x4, x5, x6, m1;
        long long w, u;
        if (xx < 10000)
        {
            j = ubs_Lt10k_2s_i2((unsigned)xx, ps);
            ps[j] = 0;
            return j;
        }
        if (xx < 100000000) // dynamic range of xx is less than 32-bits
        {
            m0 = _mm_cvtsi32_si128(xx);
            x1 = _mm_shuffle_epi32(m0, 0x44); // broadcast to dw0 and dw2
            x3 = _mm_mul_epu32(x1, _mm_loadu_si128((__m128i *)pr_cg_10to4));
            x3 = _mm_mullo_epi32(_mm_srli_epi64(x3, 40), _mm_loadu_si128((__m128i *)pr_1_m10to4));
            m0 = _mm_add_epi32(_mm_srli_si128(x1, 8), x3); // quotient in dw2, remainder in dw0
            __ParMod10to4SSSE3v(x3, m0);                   // pack single digit from each dword to dw0
            x4 = _mm_shuffle_epi8(x3, _mm_loadu_si128((__m128i *)bcstpklodw));
            __ParMod10to4SSSE3v(x3, _mm_slli_si128(m0, 8)); // move the remainder to dw2 first
            x5 = _mm_shuffle_epi8(x3, _mm_loadu_si128((__m128i *)bcstpkdw1));
            x4 = _mm_or_si128(x4, x5); // pack digits in bytes 0-7 with leading 0
            cnt = 8;
        }
        else
        {
            hi8 = u64mod10to8(&lo8, xx);
            if (hi8 < 10000) // decompose lo8 dword into quotient and remainder mod 10^4
            {
                m0 = _mm_cvtsi32_si128(lo8);
                x2 = _mm_shuffle_epi32(m0, 0x44);
                x3 = _mm_mul_epu32(x2, _mm_loadu_si128((__m128i *)pr_cg_10to4));
                x3 = _mm_mullo_epi32(_mm_srli_epi64(x3, 40), _mm_loadu_si128((__m128i *)pr_1_m10to4));
                m0 = _mm_add_epi32(_mm_srli_si128(x2, 8), x3); // quotient in dw0
                __ParMod10to4SSSE3(x3, hi8);                   // handle digist 11:8 first
                x4 = _mm_shuffle_epi8(x3, _mm_loadu_si128((__m128i *)bcstpklodw));
                __ParMod10to4SSSE3v(x3, m0); // handle digits 7:4
                x5 = _mm_shuffle_epi8(x3, _mm_loadu_si128((__m128i *)bcstpkdw1));
                x4 = _mm_or_si128(x4, x5);
                __ParMod10to4SSSE3v(x3, _mm_slli_si128(m0, 8));
                x5 = _mm_shuffle_epi8(x3, _mm_loadu_si128((__m128i *)bcstpkdw2));
                x4 = _mm_or_si128(x4, x5); // pack single digist in bytes 0-11 with leading 0
                cnt = 12;
            }
            else
            {
                cnt = 0;
                if (hi8 >= 100000000) // handle input greater than 10^16
                {
                    abv16 = u64mod10to8(&temp, (int64_t)hi8);
                    hi8 = temp;
                    __ParMod10to4SSSE3(x3, abv16);
                    x6 = _mm_shuffle_epi8(x3, _mm_loadu_si128((__m128i *)bcstpklodw));
                    cnt = 4;
                } // start with handling digits 15:12
                m0 = _mm_cvtsi32_si128(hi8);
                x2 = _mm_shuffle_epi32(m0, 0x44);
                x3 = _mm_mul_epu32(x2, _mm_loadu_si128((__m128i *)pr_cg_10to4));
                x3 = _mm_mullo_epi32(_mm_srli_epi64(x3, 40), _mm_loadu_si128((__m128i *)pr_1_m10to4));
                m0 = _mm_add_epi32(_mm_srli_si128(x2, 8), x3);
                m1 = _mm_cvtsi32_si128(lo8);
                x2 = _mm_shuffle_epi32(m1, 0x44);
                x3 = _mm_mul_epu32(x2, _mm_loadu_si128((__m128i *)pr_cg_10to4));
                x3 = _mm_mullo_epi32(_mm_srli_epi64(x3, 40), _mm_loadu_si128((__m128i *)pr_1_m10to4));
                m1 = _mm_add_epi32(_mm_srli_si128(x2, 8), x3);
                __ParMod10to4SSSE3v(x3, m0);
                x4 = _mm_shuffle_epi8(x3, _mm_loadu_si128((__m128i *)bcstpklodw));
                __ParMod10to4SSSE3v(x3, _mm_slli_si128(m0, 8));
                x5 = _mm_shuffle_epi8(x3, _mm_loadu_si128((__m128i *)bcstpkdw1));
                x4 = _mm_or_si128(x4, x5);
                __ParMod10to4SSSE3v(x3, m1);
                x5 = _mm_shuffle_epi8(x3, _mm_loadu_si128((__m128i *)bcstpkdw2));
                x4 = _mm_or_si128(x4, x5);
                __ParMod10to4SSSE3v(x3, _mm_slli_si128(m1, 8));
                x5 = _mm_shuffle_epi8(x3, _mm_loadu_si128((__m128i *)bcstpkdw3));
                x4 = _mm_or_si128(x4, x5);
                cnt += 16;
            }
        }
        m0 = _mm_loadu_si128((__m128i *)asc0reversebias);
        if (cnt > 16)
        {
            tmp = _mm_movemask_epi8(_mm_cmpgt_epi8(x6, _mm_setzero_si128()));
            x6 = _mm_sub_epi8(x6, m0);
        }
        else
        {
            tmp = _mm_movemask_epi8(_mm_cmpgt_epi8(x4, _mm_setzero_si128()));
        }
        __asm__("bsfl %1, %%ecx; movl %%ecx, %0;" : "=r"(idx) : "r"(tmp) : "%ecx");
        x4 = _mm_sub_epi8(x4, m0);
        cnt -= idx;
        w = _mm_cvtsi128_si64(x4);
        switch (cnt)
        {
        case 5:
            *ps++ = (char)(w >> 24);
            *(unsigned *)ps = (w >> 32);
            break;
        case 6:
            *(short *)ps = (short)(w >> 16);
            *(unsigned *)(&ps[2]) = (w >> 32);
            break;
        case 7:
            *ps = (char)(w >> 8);
            *(short *)(&ps[1]) = (short)(w >> 16);
            *(unsigned *)(&ps[3]) = (w >> 32);
            break;
        case 8:
            *(long long *)ps = w;
            break;
        case 9:
            *ps++ = (char)(w >> 24);
            *(long long *)(&ps[0]) = _mm_cvtsi128_si64(_mm_srli_si128(x4, 4));
            break;
        case 10:
            *(short *)ps = (short)(w >> 16);
            *(long long *)(&ps[2]) = _mm_cvtsi128_si64(_mm_srli_si128(x4, 4));
            break;
        case 11:
            *ps = (char)(w >> 8);
            *(short *)(&ps[1]) = (short)(w >> 16);
            *(long long *)(&ps[3]) = _mm_cvtsi128_si64(_mm_srli_si128(x4, 4));
            break;
        case 12:
            *(unsigned *)ps = w;
            *(long long *)(&ps[4]) = _mm_cvtsi128_si64(_mm_srli_si128(x4, 4));
            break;
        case 13:
            *ps++ = (char)(w >> 24);
            *(unsigned *)ps = (w >> 32);
            *(long long *)(&ps[4]) = _mm_cvtsi128_si64(_mm_srli_si128(x4, 8));
            break;
        case 14:
            *(short *)ps = (short)(w >> 16);
            *(unsigned *)(&ps[2]) = (w >> 32);
            *(long long *)(&ps[6]) = _mm_cvtsi128_si64(_mm_srli_si128(x4, 8));
            break;
        case 15:
            *ps = (char)(w >> 8);
            *(short *)(&ps[1]) = (short)(w >> 16);
            *(unsigned *)(&ps[3]) = (w >> 32);
            *(long long *)(&ps[7]) = _mm_cvtsi128_si64(_mm_srli_si128(x4, 8));
            break;
        case 16:
            _mm_storeu_si128((__m128i *)ps, x4);
            break;
        case 17:
            u = _mm_cvtsi128_si64(x6);
            *ps++ = (char)(u >> 24);
            _mm_storeu_si128((__m128i *)&ps[0], x4);
            break;
        case 18:
            u = _mm_cvtsi128_si64(x6);
            *(short *)ps = (short)(u >> 16);
            _mm_storeu_si128((__m128i *)&ps[2], x4);
            break;
        case 19:
            u = _mm_cvtsi128_si64(x6);
            *ps = (char)(u >> 8);
            *(short *)(&ps[1]) = (short)(u >> 16);
            _mm_storeu_si128((__m128i *)&ps[3], x4);
            break;
        case 20:
            u = _mm_cvtsi128_si64(x6);
            *(unsigned *)ps = (short)(u);
            _mm_storeu_si128((__m128i *)&ps[4], x4);
            break;
        }
        return cnt;
    }

    char lower_digits[] = "0123456789";
    int ltoa_sse4(const int64_t s1, char *buf)
    {
        int64_t temp;
        int j = 1, len = 0;
        const char *digits = &lower_digits[0];
        if (s1 < 0)
        {
            temp = -s1;
            len++;
            buf[0] = '-';
            if (temp < 10)
            {
                buf[1] = digits[(int)temp];
                len++;
            }
            else
                len += sse4i_q2a_u63(temp, &buf[1]); // parallel conversion in 4-digit granular operation
        }
        else
        {
            if (s1 < 10)
            {
                buf[0] = digits[(int)s1];
                len++;
            }
            else
                len += sse4i_q2a_u63(s1, &buf[0]);
        }
        buf[len] = 0;
        return len;
    }

    inline __attribute__((always_inline))
    __m128i
    __m128i_shift_right(__m128i value, int offset)
    {
        switch (offset)
        {
        case 1:
            value = _mm_srli_si128(value, 1);
            break;
        case 2:
            value = _mm_srli_si128(value, 2);
            break;
        case 3:
            value = _mm_srli_si128(value, 3);
            break;
        case 4:
            value = _mm_srli_si128(value, 4);
            break;
        case 5:
            value = _mm_srli_si128(value, 5);
            break;
        case 6:
            value = _mm_srli_si128(value, 6);
            break;
        case 7:
            value = _mm_srli_si128(value, 7);
            break;
        case 8:
            value = _mm_srli_si128(value, 8);
            break;
        case 9:
            value = _mm_srli_si128(value, 9);
            break;
        case 10:
            value = _mm_srli_si128(value, 10);
            break;
        case 11:
            value = _mm_srli_si128(value, 11);
            break;
        case 12:
            value = _mm_srli_si128(value, 12);
            break;
        case 13:
            value = _mm_srli_si128(value, 13);
            break;
        case 14:
            value = _mm_srli_si128(value, 14);
            break;
        case 15:
            value = _mm_srli_si128(value, 15);
            break;
        }
        return value;
    }

    /* Load string at S near page boundary safely. */
    inline __attribute__((always_inline))
    __m128i
    __m128i_strloadu_page_boundary(const char *s)
    {
        int offset = ((size_t)s & (16 - 1));
        if (offset)
        {
            __m128i v = _mm_load_si128((__m128i *)(s - offset));
            __m128i zero = _mm_setzero_si128();
            int bmsk = _mm_movemask_epi8(_mm_cmpeq_epi8(v, zero));
            if ((bmsk >> offset) != 0)
                return __m128i_shift_right(v, offset);
        }
        return _mm_loadu_si128((__m128i *)s);
    }

    inline __attribute__((always_inline))
    __m128i
    ShfLAlnLSByte(__m128i value, int offset)
    {
        /*now remove constant bias, so each byte element are unsigned byte int */
        value = _mm_sub_epi8(value, _mm_setr_epi32(0x30303030, 0x30303030, 0x30303030, 0x30303030));
        switch (offset)
        {
        case 1:
            value = _mm_slli_si128(value, 1);
            break;
        case 2:
            value = _mm_slli_si128(value, 2);
            break;
        case 3:
            value = _mm_slli_si128(value, 3);
            break;
        case 4:
            value = _mm_slli_si128(value, 4);
            break;
        case 5:
            value = _mm_slli_si128(value, 5);
            break;
        case 6:
            value = _mm_slli_si128(value, 6);
            break;
        case 7:
            value = _mm_slli_si128(value, 7);
            break;
        case 8:
            value = _mm_slli_si128(value, 8);
            break;
        case 9:
            value = _mm_slli_si128(value, 9);
            break;
        case 10:
            value = _mm_slli_si128(value, 10);
            break;
        case 11:
            value = _mm_slli_si128(value, 11);
            break;
        case 12:
            value = _mm_slli_si128(value, 12);
            break;
        case 13:
            value = _mm_slli_si128(value, 13);
            break;
        case 14:
            value = _mm_slli_si128(value, 14);
            break;
        case 15:
            value = _mm_slli_si128(value, 15);
            break;
        }
        return value;
    }
    // we use pmaddwd to merge two adjacent short integer pair, this is the second step of merging each pair of 2-digit integers
    static short MulplyPairBaseP2[8] =
        {100, 1, 100, 1, 100, 1, 100, 1};
    // Multiplier-pair for two adjacent short integer pair, this is the third step of merging each pair of 4-digit integers
    static short MulplyPairBaseP4[8] =
        {10000, 1, 10000, 1, 10000, 1, 10000, 1};
    // multiplier for pmulld for normalization of > 16 digits
    static int MulplyByBaseExpN[8] =
        {10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000};
    static short MulplyQuadBaseExp3To0[8] =
        {1000, 100, 10, 1, 1000, 100, 10, 1};

    template <typename IntType>
    class StringConversion<IntType, typename std::enable_if<std::is_integral<IntType>::value>::type> //;EnableIfIntType<IntType>>
    {
    public:
        inline __attribute__((always_inline)) static char *ToString(IntType t, char *buf, const size_t len, size_t &finalLength, bool moveToBeg = false)
        {
            finalLength = ltoa_sse4(t, buf);
            return buf;
        }

        /*
         * At least twice as fast as atoi. Often more
         * BUT, no error checking
         */
        static IntType FromString(const char *buf, size_t len)
        {
            if (len == 0)
                return 0;
            if (len == 1)
                return buf[0] - '0';
            if (len <= 16)
            {
                char *p = const_cast<char *>(buf);
                bool neg = false;

                if (buf[0] == '-')
                {
                    p++;
                    len--;
                    neg = true;
                }

                __m128i w1, w1_l8, w1_u8, w2 = _mm_setzero_si128();
                int64_t xxi;

                __m128i mask0 = __m128i_strloadu_page_boundary(p);
                mask0 = ShfLAlnLSByte(mask0, 16 - len);
                w1_u8 = _mm_slli_si128(mask0, 1);
                w1 = _mm_add_epi8(mask0, _mm_slli_epi16(w1_u8, 3));                           /* mul by 8 and add */
                w1 = _mm_add_epi8(w1, _mm_slli_epi16(w1_u8, 1));                              /* 7 LS bits per byte, in bytes 0, 2, 4, 6, 8, 10, 12, 14*/
                w1 = _mm_srli_epi16(w1, 8);                                                   /* clear out upper bits of each wd*/
                w2 = _mm_madd_epi16(w1, _mm_loadu_si128((__m128i *)&MulplyPairBaseP2[0]));    /* multiply base^2, add adjacent
                                                                                                 word,*/
                w1_u8 = _mm_packus_epi32(w2, w2);                                             /* pack 4 low word of each dword into 63:0 */
                w1 = _mm_madd_epi16(w1_u8, _mm_loadu_si128((__m128i *)&MulplyPairBaseP4[0])); /* multiply base^4, add
                                                                                                 adjacent word,*/
                w1 = _mm_cvtepu32_epi64(w1);                                                  /* converted dw was in 63:0, expand to qw */
                w1_l8 = _mm_mul_epu32(w1, _mm_setr_epi32(100000000, 0, 0, 0));
                w2 = _mm_add_epi64(w1_l8, _mm_srli_si128(w1, 8));
                xxi = _mm_extract_epi64(w2, 0);
                return (neg ? (IntType)-xxi : (IntType)xxi);
            }
            else
            {
                IntType result = 0;
                size_t pos = 0;
                if (buf[0] == '-')
                {
                    pos++;
                }

                for (; pos < len; pos++)
                {
                    result = result * 10 + (buf[pos] - '0');
                }
                if (buf[0] == '-')
                    result *= -1;
                return result;
            }
        }
    };

    /********************* Double conversion *********************************/

    // helper function used later
    template <typename RealType>
    inline __attribute__((always_inline))
    RealType
    fastPow10(RealType d, unsigned int power)
    {
        RealType dpow = d;
        while (power >= 3)
        {
            // multiply by 1000
            dpow *= 1000;
            power -= 3;
        }

        while (power > 0)
        {
            // multiply by 10
            dpow *= 10;
            power -= 1;
        }
        return dpow;
    }

    // helper function used later
    template <typename RealType>
    inline __attribute__((always_inline))
    int64_t
    Round(RealType d)
    {
        return std::round(d);
    }

    template <>
    inline __attribute__((always_inline))
    int64_t
    Round(double d)
    {
        //_mm_round_pd(mm_load_pd1(&d), (_MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC));
        return _mm_cvtsd_si64(_mm_load_pd1(&d));
    }

    /* Needs -std=c++11
    template<typename T>
    using EnableIfFloatType =
            typename std::enable_if<std::is_floating_point<T>::value>::type;
  */
    template <typename RealType>
    class StringConversion<RealType, typename std::enable_if<std::is_floating_point<RealType>::value>::type> // EnableIfFloatType<RealType>>
    {
    public:
        static char *ToString(RealType d, char *buf, size_t length, size_t &finalLength, bool moveToBeg = false, unsigned int precision = 6)
        {
            if (precision >= 20)
            {
                char format[10];
                format[0] = '%';
                auto len = sprintf(format + 1, ".%d", precision);
                format[len] = 'f';
                format[len + 1] = '\0';
                finalLength = sprintf(buf, format, d);
                return buf;
            }

            // truncate
            uint64_t ivalue = (uint64_t)(d);

            uint64_t sign = 0;
            if (d < 0)
                sign = 1;

            if (sign)
            {
                buf[0] = '-';
                // ivalue *= -1;
                d *= -1;
            }

            // dec is fractional part, ivalue is whole number part
            RealType dec = d - ivalue;

            // Deal with corner case of floating point subtraction
            if (dec >= 1)
            {
                ivalue++;
                dec = d - ivalue;
            }

            // multiply fractional part by 10^precision
            uint64_t idv = (uint64_t)Round(fastPow10(dec, precision));

            if (idv == 0)
            {
                // convert whole number to string
                StringConversion<uint64_t>::ToString(ivalue, buf + sign, length - sign, finalLength, true);
                finalLength += sign;
                buf[finalLength] = '\0';
                return buf;
            }

            int counter = 0;
            // skip trailing 0s
            while (!(idv % 10) && (idv /= 10))
            {
                counter++;
            }

            if (counter == precision)
            {
                ivalue++;
                // convert whole number to string
                StringConversion<uint64_t>::ToString(ivalue, buf + sign, length - sign, finalLength, true);
                finalLength += sign;
                buf[finalLength] = '\0';
                return buf;
            }

            // convert whole number to string
            size_t finalLen = 0;
            StringConversion<uint64_t>::ToString(ivalue, buf + sign, length - sign, finalLen, true);
            finalLen += sign;

            buf[finalLen] = '.';
            char temp[1024];
            size_t tempLen;
            char *tt = StringConversion<uint64_t>::ToString(idv, temp, 1024, tempLen);
            counter += tempLen;

            while (counter < precision)
            {
                finalLen++;
                buf[finalLen] = '0';
                counter++;
            }
            strcpy(buf + finalLen + 1, tt);

            finalLength = finalLen + tempLen + 1; // not counting null character
            return buf;
        }

        // TODO : check this
#define ISDIGIT(inchar) ((inchar) >= '0' && (inchar) <= '9')
#define SKIPCHAR(inchar) (((inchar) < '0' || (inchar) > '9') && !((inchar == '-') || (inchar == '+')))
        inline __attribute__((always_inline)) static RealType FromString(const char *in, size_t len)
        {
            // some cleanup, skip the chars till we get a digit or + or -
            while (SKIPCHAR(*in))
                in++;

            // get the sign
            RealType sign = 1.0;
            if (*in == '-')
            {
                sign = -1.0;
                in++;
            }
            else if (*in == '+')
            {
                in++;
            }

            // extract the firt part of double value using int data type. We want
            // to avoid repeated double multipication
            int intValPart1 = 0;
            while (ISDIGIT(*in))
            {
                intValPart1 = (intValPart1 * 10) + (*in - '0');
                in++;
            }

            // after decimal point conversion
            RealType realValue = 0.0;
            int intValPart2 = 0;
            int tenPow = 1;
            if (*in == '.')
            {
                in++; // skip .
                // get the decimal part using int data type, avoid repeated * and / on double
                while (ISDIGIT(*in))
                {
                    intValPart2 = (intValPart2 * 10) + (*in - '0');
                    tenPow *= 10; // compute pow
                    in++;
                }
            }
            realValue = ((RealType)intValPart1) + ((RealType)intValPart2 / (RealType)tenPow);

            // Handle exponent, quickfix style.
            // TODO: needs to be improved
            bool fraction = false;
            RealType scale = 1.0;
            if ((*in == 'e') || (*in == 'E'))
            {
                // Get sign of exponent, if any.
                in++;
                if (*in == '-')
                {
                    fraction = true;
                    in++;
                }
                else if (*in == '+')
                {
                    in++;
                }

                // Get digits of exponent
                unsigned int expon = 0;
                while (ISDIGIT(*in))
                {
                    expon = expon * 10 + (*in - '0');
                    in++;
                }
                if (expon > 308)
                    expon = 308;

                // Calculate scaling factor.
                while (expon >= 50)
                {
                    scale *= 1E50;
                    expon -= 50;
                }
                while (expon >= 8)
                {
                    scale *= 1E8;
                    expon -= 8;
                }
                while (expon > 0)
                {
                    scale *= 10.0;
                    expon -= 1;
                }

                return sign * (fraction ? (realValue / scale) : (realValue * scale));
            }

            return sign * realValue;
        }
    };

}
#endif
