#ifndef STRINGCONVERSION_H_
#define STRINGCONVERSION_H_
#include <cstring>
#include <cmath>
#include <type_traits>
#include <string>
#include "emmintrin.h"
#include "macros.h"

namespace illuminati
{

    // General declaration
    // Template specializations sometimes add more arguments to ToString.
    // See below
    template <typename T, typename Enable = void>
    class StringConversion
    {
    public:
        inline __attribute__((always_inline)) static char *ToString(T t, char *buf, const size_t len, size_t &finalLength, bool moveToBeg = false, bool shouldNullTerminate = true);

        inline __attribute__((always_inline)) static T FromString(const char *buf, size_t len, const char startchar = '0', const unsigned int base = 10);
    };

    /********************** Integer Conversion **********************************/
    /* Needs -std=c++11
    template<typename T>
    using EnableIfIntType =
            typename std::enable_if<std::is_integral<T>::value>::type;
  */
    template <typename IntType>
    class StringConversion<IntType, typename std::enable_if<std::is_integral<IntType>::value>::type> //;EnableIfIntType<IntType>>
    {
    public:
        inline __attribute__((always_inline)) static char *ToString(IntType t, char *buf, const size_t len, size_t &finalLength, bool moveToBeg = false, bool shouldNullTerminate = true)
        {
            bool neg = false;
            if (t < 0)
            {
                neg = true;
                t *= -1;
            }

            char *p = buf + len;
            if (shouldNullTerminate)
            {
                *--p = '\0';
            }
            do
            {
                *--p = '0' + (char)(t % 10);
                t /= 10;
            } while (t > 0);

            finalLength = (buf + len) - p - 1;

            if (neg)
            {
                *--p = '-';
                finalLength++;
            }

            if (moveToBeg)
            {
                memmove(buf, p, finalLength + 1);
                p = buf;
            }

            return p;
        }

        inline __attribute__((always_inline)) static char *ToStringWithBase(IntType t, char *buf, const size_t len, size_t &finalLength, const char startchar, const unsigned int base, bool moveToBeg = false, bool shouldNullTerminate = true)
        {
            bool neg = false;
            if (t < 0)
            {
                neg = true;
                t *= -1;
            }

            char *p = buf + len;
            if (shouldNullTerminate)
            {
                *--p = '\0';
            }
            do
            {
                *--p = startchar + (char)(t % base);
                t /= base;
            } while (t > 0);

            finalLength = (buf + len) - p - 1;

            if (neg)
            {
                *--p = '-';
                finalLength++;
            }

            if (moveToBeg)
            {
                memmove(buf, p, finalLength + 1);
                p = buf;
            }

            return p;
        }

        /** writes leading zeros, outputting exactly len digits **/
        /** BUF should be at least len+1 **/
        inline __attribute__((always_inline)) static char *ToPaddedString(IntType t, char *buf, size_t len)
        {
            bool neg = false;

            char *p = buf + (len + 1);

            if (t < 0)
            {
                neg = true;
                t *= -1;
                len--;
            }

            *--p = '\0';
            do
            {
                *--p = '0' + (char)(t % 10);
                t /= 10;
                len--;
            } while (t > 0);

            while (len > 0)
            {
                *--p = '0';
                len--;
            }

            if (neg)
            {
                *--p = '-';
            }

            return p;
        }

        /*
         * At least twice as fast as atoi. Often more
         * BUT, no error checking
         */
        static IntType FromString(const char *buf, size_t len, const char startchar = '0', const unsigned int base = 10)
        {
            IntType result = 0;

            size_t pos = 0;
            if (buf[0] == '-')
            {
                pos++;
            }

            for (; pos < len; pos++)
            {
                result = result * base + (buf[pos] - startchar);
            }
            if (buf[0] == '-')
                result *= -1;
            return result;
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
    Round(float f)
    {
        //_mm_round_pd(mm_load_pd1(&d), (_MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC));
        return _mm_cvtss_si64(_mm_load_ps1(&f));
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
            __builtin_prefetch(buf, 1);
            if (illuminati_unlikely_branch(precision >= 20))
            {
                char format[10];
                format[0] = '%';
                auto len = sprintf(format + 1, ".%d", precision);
                format[len] = 'f';
                format[len + 1] = '\0';
                finalLength = sprintf(buf, format, d);
                return buf;
            }

            uint64_t sign = 0;
            if (d < 0)
                sign = 1;

            if (sign)
            {
                buf[0] = '-';
                d *= -1;
            }

            // truncate
            uint64_t ivalue = (uint64_t)(d);
            // dec is fractional part, ivalue is whole number part
            RealType dec = d - ivalue;

            // Deal with corner case of floating point subtraction
            if (illuminati_unlikely_branch(dec >= 1))
            {
                ivalue++;
                dec = d - ivalue;
            }

            // multiply fractional part by 10^precision
            double pow10 = fastPow10(dec, precision);
            int64_t idv = Round(pow10);

            if (idv == 0)
            {
                // convert whole number to string
                StringConversion<uint64_t>::ToString(ivalue, buf + sign, length - sign, finalLength, true);
                finalLength += sign;
                buf[finalLength] = '\0';
                return buf;
            }

            // skip trailing 0s
            int counter = 0;
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
            precision -= counter;

            // convert whole number to string
            size_t finalLen = 0;
            StringConversion<uint64_t>::ToString(ivalue, buf + sign, length - sign, finalLen, true);
            finalLen += sign;

            buf[finalLen] = '.';

            StringConversion<uint64_t>::ToPaddedString(idv, buf + finalLen + 1, precision);

            finalLength = finalLen + precision + 1; // not counting null character
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
