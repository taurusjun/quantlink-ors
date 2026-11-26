#ifndef __MACROS__H
#define __MACROS__H

#define illuminati_force_inline inline __attribute__((always_inline))
#define illuminati_noinline __attribute__((noinline))
#define illuminati_pure __attribute__((pure))
#define illuminati_const __attribute__((const))
#define illuminati_noreturn __attribute__((noreturn))
#define illuminati_malloc __attribute__((malloc))
#define illuminati_must_check __attribute__((warn_unused_result))
#define illuminati_deprecated __attribute__((deprecated))
#define illumiati_used __attribute__((used))
#define illuminat_unused __attribute__((unused))
#define illuminati_packed __attribute__((packed))
#define illuminati_align(x) __attribute__((aligned(x)))
#define illuminati_align_max __attribute__((aligned))
#define illumiati_likely_branch(x) __builtin_expect(!!(x), 1)
#define illuminati_unlikely_branch(x) __builtin_expect(!!(x), 0)

#endif
