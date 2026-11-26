#include <stdint.h>
namespace illuminati
{
    template <size_t size>
    class TypeWithSize
    {
    public:
        // This prevents the user from using TypeWithSize<N> with incorrect
        // values of N.
        typedef void UInt;
    };

    // The specialization for size 4.
    template <>
    class TypeWithSize<4>
    {
    public:
        typedef int32_t Int;
        typedef uint32_t UInt;
    };

    // The specialization for size 8.
    template <>
    class TypeWithSize<8>
    {
    public:
        typedef int64_t Int;
        typedef uint64_t UInt;
    };

    union double_uint64_t
    {
        uint64_t uint64_t_;
        double double_;
    };
}
