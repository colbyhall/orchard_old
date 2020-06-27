#ifndef LANGUAGE_LAYER_H
#define LANGUAGE_LAYER_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN64) || defined(__x86_64__) || defined(_M_X64) || defined(__64BIT__) || defined(__powerpc64__) || defined(__ppc64__)
#define PLATFORM_64BIT 1
#define PLATFORM_32BIT 0
#else
#define PLATFORM_64BIT 0
#define PLATFORM_32BIT 1
#endif

#if defined(_WIN32) || defined(_WIN64)
#define PLATFORM_WINDOWS 1
#elif defined(__APPLE__) && defined(__MA_)
#define PLATFORM_OSX 1
#elif defined(__unix__)
#define PLATFORM_UNIX 1

#if defined(__linux__)
#define PLATFORM_LINUX 1
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#define PLATFORM_FREEBSD 1
#else
#error This UNIX operating system is not supported
#endif
#else
#error This operating system is not supported
#endif

#ifndef PLATFORM_WINDOWS
#define PLATFORM_WINDOWS 0
#endif
#ifndef PLATFORM_OSX
#define PLATFORM_OSX 0
#endif
#ifndef PLATFORM_UNIX
#define PLATFORM_UNIX 0
#endif
#ifndef PLATFORM_LINUX
#define PLATFORM_LINUX 0
#endif
#ifndef PLATFORM_FREEBSD
#define PLATFORM_FREEBSD 0
#endif

#if defined(_MSC_VER)
#define COMPILER_MSVC 1
#elif defined(__GNUC__)
#define COMPILER_GCC 1
#elif defined(__clang__)
#define COMPILER_CLANG 1
#else
#error Unknown compiler
#endif

#ifndef COMPILER_MSVC
#define COMPILER_MSVC 0
#endif
#ifndef COMPILER_GCC
#define COMPILER_GCC 0
#endif
#ifndef COMPILER_CLANG
#define COMPILER_CLANG 0
#endif

#if COMPILER_MSVC
#define DLL_EXPORT __declspec(dllexport)
#else
#error Missing dll export
#endif

typedef uint8_t   u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef float f32;
typedef double f64;

typedef size_t usize;
#if PLATFORM_64BIT
typedef s64 isize;
#else
typedef s32 isize;
#endif

typedef s32 b32;
#define true 1
#define false 0

#define U8_MIN 0u
#define U8_MAX 0xffu
#define U16_MIN 0u
#define U16_MAX 0xffffu
#define U32_MIN 0u
#define U32_MAX 0xffffffffu
#define U64_MIN 0ull
#define U64_MAX 0xffffffffffffffffull

#define S8_MIN (-0x7f - 1)
#define S8_MAX 0x7f
#define S16_MIN (-0x7fff - 1)
#define S16_MAX 0x7fff
#define S32_MIN (-0x7fffffff - 1)
#define S32_MAX 0x7fffffff
#define S64_MIN (-0x7fffffffffffffffll - 1)
#define S64_MAX 0x7fffffffffffffffll

#define F32_MIN 1.17549435e-38f
#define F32_MAX 3.40282347e+38f

#define F64_MIN 2.2250738585072014e-308
#define F64_MAX 1.7976931348623157e+308

#if COMPILER_MSVC
#if _MSC_VER < 1300 
#define debug_trap __asm int 3
#else
#define debug_trap __debugbreak()
#endif
#else
#define debug_trap __builtin_trap()
#endif

#define invalid_code_path debug_trap

#define ensure(cond) _assert_implementation(cond != 0, false, __LINE__, __FILE__)
#define assert(cond) _assert_implementation(cond != 0, true, __LINE__, __FILE__)

inline void _assert_implementation(b32 cond, b32 do_segfault, u32 line, const char* file) {
    if (cond) return;

    printf("%s thrown on %s at line %lu", do_segfault ? "assertion" : "ensure", file, line);

    if (do_segfault) {
        int* foo = 0;
        *foo = 0;
    } else {
        debug_trap;
    }
}

#define kilobyte(b) ((b) * 1024LL)
#define megabyte(b) (kilobyte(b) * 1024LL)
#define gigabyte(b) (megabyte(b) * 1024LL)

#define ARRAY_COUNT(x) (sizeof(x) / sizeof(x[0]))

// @NOTE(colby): Maybe one day we wont use the cruntime
#define mem_copy    memcpy
#define mem_move    memmove
#define str_len     strlen


#endif /* LANGUAGE_LAYER_H */