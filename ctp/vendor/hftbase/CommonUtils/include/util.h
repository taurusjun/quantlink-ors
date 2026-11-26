#ifndef UTIL_GENERAL_H_
#define UTIL_GENERAL_H_
#include <type_traits>
namespace illuminati
{
    /*
     * if x = getNextHigherPow2(n)
     * then x is a power of 2
     * and x >= n
     * and there is no power of 2 between x and n
     */
    template <typename IntType>
    static typename std::enable_if<std::is_integral<IntType>::value, IntType>::type
    getNextHigherPow2(IntType value)
    {
        if (!value & (!(value & (value - 1))))
        {
            return value;
        }

        IntType result = 1;
        while (value)
        {
            value = value >> 1;
            result = result << 1;
        }
        return result;
    }

}
#endif
