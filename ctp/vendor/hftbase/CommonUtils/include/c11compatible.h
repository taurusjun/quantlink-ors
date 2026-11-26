#ifndef _C11_COMPATIBLE
#define _C11_COMPATIBLE

#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)

// Task 1 portability shim: C++20 build always uses the standard atomic types.
#include <unistd.h>
#include <atomic>

#endif
