#pragma once

/* ##################################### */
/* ========= MACRO HELPERS: ============ */
/* ##################################### */
#define MACRO_CAT2(a, b) a##b
#define MACRO_CAT(a, b) MACRO_CAT2(a, b)

#if __cplusplus >= 201703L
#define UMBRA_MAYBE_UNUSED [[maybe_unused]]
#elif defined(__GNUC__) || defined(__clang__)
#define UMBRA_MAYBE_UNUSED __attribute__((unused))
#else
#define UMBRA_MAYBE_UNUSED
#endif

#define ARRAY_SIZE(X) (sizeof(X) / sizeof((X)[0]))
