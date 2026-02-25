#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef _MSC_VER
#include <immintrin.h>
#endif

#ifdef __cplusplus
extern "C" {
#else
#define noexcept
#endif

/* x86/x64: SSE2 (MSVC: _M_IX86/_M_X64; GCC/Clang: __i386__/__x86_64__) */
#if (defined(_M_IX86) || defined(_M_X64) || defined(_M_AMD64) || \
     defined(__i386__) || defined(__i686__) || defined(__x86_64__) || defined(__amd64__))
#ifndef CHARIS_SIMD_SSE2
#define CHARIS_SIMD_SSE2
#endif
#endif

/* ARM: NEON (AArch64 mandatory; ARM32 when enabled) */
#if (defined(__aarch64__) || defined(_M_ARM64) || defined(__ARM_NEON__) || defined(__ARM_NEON))
#ifndef CHARIS_SIMD_NEON
#define CHARIS_SIMD_NEON
#endif
#endif

// Count leading zeros
static inline uint32_t
charis_leading_zeros_u32(uint32_t v) noexcept {
#ifdef _MSC_VER
    return _lzcnt_u32(v);
#else
    return v ? __builtin_clz(v) : 32;
#endif
}

// Count trailing zeros (index of lowest set bit; returns 32 if v == 0)
static inline uint32_t
charis_trailing_zeros_u32(uint32_t v) noexcept {
#ifdef _MSC_VER
    unsigned long idx;
    return _BitScanForward(&idx, v) ? (uint32_t)idx : 32u;
#else
    return v ? (uint32_t)__builtin_ctz(v) : 32u;
#endif
}

static inline void* charis_malloc(size_t size) noexcept {
    return malloc(size);
}

static inline void charis_free(void* ptr) noexcept {
    free(ptr);
}

static inline void* charis_realloc(void* ptr, size_t size) noexcept {
    return realloc(ptr, size);
}

//#ifdef _MSC_VER
//static inline void* charis_malloc_a(size_t size, size_t alignment) noexcept {
//    return _aligned_malloc(size, alignment);
//}
//static inline void charis_free_a(void* ptr) noexcept {
//    _aligned_free(ptr);
//}
//#else
//static inline void* charis_malloc_a(size_t size, size_t alignment) noexcept {
//    return aligned_alloc(alignment, size);
//}
//static inline void charis_free_a(void* ptr) noexcept {
//    free(ptr);
//}
//#endif

#ifdef __cplusplus
}
#endif
