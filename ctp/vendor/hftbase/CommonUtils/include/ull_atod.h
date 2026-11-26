#ifndef _ULL_ATOF_
#define _ULL_ATOF_

#include "ull.h"

#define ISDIGIT(inchar) ((inchar) >= '0' && (inchar) <= '9')
#define SKIPCHAR(inchar) (((inchar) < '0' || (inchar) > '9') && !((inchar == '-') || (inchar == '+')))

namespace illuminati
{
    ULL_FORCE_INLINE static double atod(const char *in)
    {
        // some cleanup, skip the chars till we get a digit or + or -
        while (SKIPCHAR(*in))
            in++;

        // get the sign
        double sign = 1.0;
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
        double doubleValue = 0.0;
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
        doubleValue = double(intValPart1) + ((double)intValPart2 / (double)tenPow);

        // Handle exponent, quickfix style.
        // TODO: needs to be improved
        bool fraction = false;
        double scale = 1.0;
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

            return sign * (fraction ? (doubleValue / scale) : (doubleValue * scale));
        }

        return sign * doubleValue;
    }
}

#endif
