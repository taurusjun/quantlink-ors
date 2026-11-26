/*
 * numtostring.h
 *
 *  Created on: 31-Dec-2013
 *      Author: jijith
 */

#ifndef ILLUMINATI_NUMTOSTRING_H_
#define ILLUMINATI_NUMTOSTRING_H_

#include <stdlib.h>
#include <string.h>
#include <string>
#include <iostream>
#include <cmath>
#include "stringutils.h"

#if GCC_VERSION < 40600
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

namespace illuminati
{

    const static uint64_t _1Mil = 1000000;

    // static float Pow5Table[128];
    static float Pow10Table[128];
    // static float Pown10Table[128];

    class IntegerToString
    {
    private:
        char *IntegerTable[_1Mil];
        IntegerToString()
        {
        }

    public:
        IntegerToString(const IntegerToString &) = delete;
        IntegerToString(const IntegerToString &&) = delete;

        static IntegerToString &GetInstance()
        {
            static IntegerToString its;
            return its;
        }

        void init()
        {
            /* using this table is SLOWER
              for(uint64_t i=0; i<_1Mil; i++)
              {
                  std::string str = std::to_string((long long unsigned int)i);
                  posix_memalign((void**)&IntegerTable[i], 64, str.length()+1);
                  strcpy(IntegerTable[i], str.c_str());
              }
              */
        }

        template <typename IntType>
        inline __attribute__((always_inline)) char *ToString(char *buf, const size_t len, IntType t,
                                                             size_t &finalLength, bool moveToBeg = false)
        {
#if GCC_VERSION > 40600
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
            /*
              if(t > 0 && t < _1Mil)
              {
                  strcpy(buf, IntegerTable[t]);
                  finalLength = strlen(buf);
                  return buf;
              }
              */
#if GCC_VERSION > 40600
#pragma GCC diagnostic pop
#endif

            char *output = intTochars(buf, len, t, finalLength);

            if (moveToBeg)
            {
                memmove(buf, output, finalLength);
                output = buf;
            }
            finalLength--;
            return output;
        }
    };

    class DoubleToString
    {
    private:
        static void printbits(unsigned char *buf, int size)
        {
            for (int i = size - 1; i >= 0; i--)
            {
                // print byte
                for (int j = 7; j >= 0; j--)
                {
                    std::cout << (((*(buf + i)) & (1 << j)) >> j);
                }
                std::cout << " ";
            }
            std::cout << "\n";
        }

    public:
        DoubleToString() = delete;
        DoubleToString(const DoubleToString &) = delete;
        DoubleToString(const DoubleToString &&) = delete;

        // There seems to be no difference in performance whether this table is used or not
        static void init()
        {
            IntegerToString::GetInstance().init();
            for (int i = 0; i < 80; i++)
            {
                // Pow5Table[i] = pow(5, i);
                Pow10Table[i] = pow(10, i);
                // Pown10Table[i] = pow(10, i*-1);
            }
        }

        template <typename RealType>
        inline __attribute__((always_inline)) static RealType fastPow10(RealType d, int precision = 6)
        {
            RealType dpow = d;
            while (precision >= 3)
            {
                // multiply by 1000
                dpow *= 1000;
                precision -= 3;
            }

            while (precision > 0)
            {
                // multiply by 10
                dpow *= 10;
                precision -= 1;
            }
            return dpow;
        }

        template <typename RealType>
        inline __attribute__((always_inline)) static char *ToString(RealType d, char *buf, size_t length, size_t &finalLength, int precision = 6)
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

            uint64_t ivalue = (uint64_t)d;

            uint64_t sign = 0;
            if (d < 0)
                sign = 1;

            if (sign)
            {
                buf[0] = '-';
                ivalue *= -1;
                d *= -1;
            }

            RealType dec = d - ivalue;
            // uint64_t idv = (uint64_t) std::round(dec*Pow10Table[precision]);
            if (dec >= 1)
            {
                ivalue++;
                dec = d - ivalue;
            }

            uint64_t idv = (uint64_t)std::round(fastPow10(dec, precision));

            size_t finalLen = 0;
            illuminati::IntegerToString::GetInstance().ToString(buf + sign, length - sign, ivalue, finalLen, true);
            finalLen += sign;
            if (idv == 0)
            {
                finalLength = finalLen;
                buf[finalLength] = '\0';
                return buf;
            }
            buf[finalLen] = '.';

            int counter = 0;
            // skip trailing 0s
            while (!(idv % 10) && (idv /= 10))
            {
                counter++;
            }
            char temp[1024];
            size_t tempLen;
            char *tt = illuminati::IntegerToString::GetInstance().ToString(temp, 1024, idv, tempLen);
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

        template <typename RealType>
        inline __attribute__((always_inline)) static char *ToStringWithTable(RealType d, char *buf, size_t length, size_t &finalLength, int precision = 6)
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

            uint64_t ivalue = (uint64_t)d;

            uint64_t sign = 0;
            if (d < 0)
                sign = 1;

            if (sign)
            {
                buf[0] = '-';
                ivalue *= -1;
                d *= -1;
            }

            RealType dec = d - ivalue;
            if (dec >= 1)
            {
                ivalue++;
                dec = d - ivalue;
            }

            uint64_t idv = (uint64_t)std::round(dec * Pow10Table[precision]);

            size_t finalLen = 0;
            illuminati::IntegerToString::GetInstance().ToString(buf + sign, length - sign, ivalue, finalLen, true);
            finalLen += sign;
            if (idv == 0)
            {
                finalLength = finalLen;
                buf[finalLength] = '\0';
                return buf;
            }
            buf[finalLen] = '.';

            int counter = 0;
            // skip trailing 0s
            while (!(idv % 10) && (idv /= 10))
            {
                counter++;
            }
            char temp[1024];
            size_t tempLen;
            char *tt = illuminati::IntegerToString::GetInstance().ToString(temp, 1024, idv, tempLen);
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
    };
}

#endif /* NUMTOSTRING_H_ */
