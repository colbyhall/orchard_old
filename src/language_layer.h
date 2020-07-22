#ifndef LANGUAGE_LAYER_H
#define LANGUAGE_LAYER_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

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

inline void _assert_implementation(b32 cond, b32 do_segfault, u32 line, char* file) {
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

#define array_count(x) (sizeof(x) / sizeof(x[0]))

// @NOTE(colby): Maybe one day we wont use the cruntime
#define mem_copy    memcpy
#define mem_move    memmove
#define mem_set     memset
#define str_len     (int)strlen
#define str_cmp     strcmp

typedef struct Allocator {
    void* data;
    void* (*proc)(struct Allocator allocator, void* ptr, usize size, usize alignment);
} Allocator;

inline void* mem_alloc_aligned(Allocator allocator, usize size, usize alignment) {
    return allocator.proc(allocator, 0, size, alignment);
}

inline void* mem_alloc(Allocator allocator, usize size) { 
    return mem_alloc_aligned(allocator, size, 4);
}

#define mem_alloc_struct(allocator, type) mem_alloc(allocator, sizeof(type))
#define mem_alloc_array(allocator, type, count) mem_alloc(allocator, sizeof(type) * count)

inline void* mem_realloc_aligned(Allocator allocator, void* ptr, usize size, usize alignment) {
    return allocator.proc(allocator, ptr, size, alignment);
}

inline void* mem_realloc(Allocator allocator, void* ptr, usize size) {
    return allocator.proc(allocator, ptr, size, 4);
}

inline void mem_free(Allocator allocator, void* ptr) {
    allocator.proc(allocator, ptr, 0, 0);
}

Allocator heap_allocator(void);
Allocator null_allocator(void); // Used for like stack allocated things

typedef struct Memory_Arena {
    u8*     base;
    usize   used;
    usize   total;
} Memory_Arena;

Allocator arena_allocator_raw(void* base, usize size);
Allocator arena_allocator(Allocator allocator, usize size);
inline void reset_arena(Allocator allocator) {
    Memory_Arena* arena = allocator.data;
    arena->used = 0;
}

typedef struct Temp_Memory {
    Memory_Arena* arena;
    usize used;
} Temp_Memory;

inline Temp_Memory begin_temp_memory(Allocator allocator) {
    Memory_Arena* arena = allocator.data;
    return (Temp_Memory) { arena, arena->used, };
}

inline void end_temp_memory(Temp_Memory temp_mem) {
    Memory_Arena* arena = temp_mem.arena;
    assert(arena);
    arena->used = temp_mem.used;
}

typedef struct Pool_Bucket {
    void* allocation;
    int used;
} Pool_Bucket;

typedef struct Pool_Allocator {
    Pool_Bucket* buckets;
    int bucket_count;
    int bucket_cap;
    u8* memory;
} Pool_Allocator;

Allocator pool_allocator(Allocator allocator, int bucket_count, int bucket_size);

typedef u32 Rune;

inline b32 is_whitespace(Rune r) { 
    if (r < 0x2000) return (r >= '\t' && r <= '\r') || r == ' ';

    return r == 0x200A || r == 0x2028 || r == 0x2029 || r == 0x202f || r == 0x205f || r == 0x3000;
}

inline b32 is_letter(Rune r) {
    r |= 0x20;
    return r >= 'a' && r <= 'z';
}

inline b32 is_digit(Rune r) {
    return r >= '0' && r <= '9';
}

typedef struct String {
    u8* data;
    int len;
    Allocator allocator;
} String;

#define from_cstr(cstr) (String) { (u8*)cstr, (int)str_len(cstr), null_allocator() }
#define expand_string(str) str.data, str.len 

typedef struct Rune_Iterator {
    String the_string;

    u32 decoder_state;
    Rune rune;
    int index;
} Rune_Iterator;

Rune_Iterator make_rune_iterator(String the_string);
b32 can_step_rune_iterator(Rune_Iterator iter);
void step_rune_iterator(Rune_Iterator* iter);
Rune rune(Rune_Iterator iter);
Rune peek_rune(Rune_Iterator iter);

#define rune_iterator(the_string) Rune_Iterator iter = make_rune_iterator(the_string); can_step_rune_iterator(iter); step_rune_iterator(&iter)

int rune_count(String the_string);
String advance_string(String the_string, int amount);
int find_from_left(String the_string, Rune r);
b32 string_equal(String a, String b);
String copy_string(String a, Allocator allocator);
b32 starts_with(String a, String b);

#define FNV_OFFSET_BASIC 0xcbf29ce484222325
#define FNV_PRIME 0x100000001b3

inline u64 fnv1_hash(void* s, usize size) {
    u64 hash = FNV_OFFSET_BASIC;
    u8* casted_s = s;
    for (usize i = 0; i < size; ++i) {
        hash *= FNV_PRIME;
        hash = hash ^ casted_s[i];
    }
    return hash;
}

typedef struct Hash_Bucket {
    u64 hash;
    int index;

    struct Hash_Bucket* next;
} Hash_Bucket;

typedef u64 (Hash_Table_Func)(void* a, void* b, int size);

u64 hash_string(void* a, void* b, int size);

typedef struct Hash_Table {
    void* keys;
    int key_size;
    
    void* values;
    int value_size;

    Hash_Bucket* buckets;
    Hash_Bucket** bucket_layout;

    int pair_count;
    int pair_cap;

    Hash_Table_Func* func;
    Allocator allocator;
} Hash_Table;

Hash_Table _make_hash_table(int key_size, int value_size, Hash_Table_Func* func, Allocator allocator);
#define make_hash_table(key, value, func, allocator) _make_hash_table(sizeof(key), sizeof(value), func, allocator)

void reserve_hash_table(Hash_Table* ht, int reserve_amount);

void* _push_hash_table(Hash_Table* ht, void* key, int key_size, void* value, int value_size);
#define push_hash_table(ht, key, value) _push_hash_table(ht, &key, sizeof(key), &value, sizeof(value))

void* _find_hash_table(Hash_Table* ht, void* key, int key_size);
#define find_hash_table(ht, key) _find_hash_table(ht, &key, sizeof(key))

#endif /* LANGUAGE_LAYER_H */