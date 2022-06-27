#pragma once

/*! \file global.h
    \brief Provides basic functionality that's used all over the place. There's usually no need to manually include this.
*/

#ifdef __cplusplus

namespace Kore {
	typedef unsigned char u8;   // 1 Byte
	typedef unsigned short u16; // 2 Byte
	typedef unsigned int u32;   // 4 Byte

#if defined(__LP64__) || defined(_LP64) || defined(_WIN64)
#define KORE_64
#endif

#ifdef KORE_WINDOWS
	typedef unsigned __int64 u64; // 8 Byte
#else
	typedef unsigned long long u64;
#endif
	typedef char s8;   // 1 Byte
	typedef short s16; // 2 Byte
	typedef int s32;   // 4 Byte
#ifdef KORE_WINDOWS
	typedef __int64 s64; // 8 Byte
#else
	typedef long long s64;
#endif

	typedef u32 uint; // 4 Byte
	typedef s32 sint; // 4 Byte

#ifdef KORE_64
	typedef s64 spint;
	typedef u64 upint;
#else
	typedef s32 spint;
	typedef u32 upint;
#endif
}

// pseudo C++11
#if !defined(_MSC_VER) && __cplusplus <= 199711L
#define nullptr 0
#define override
#endif

#define Noexcept throw()

#endif

#include <stdbool.h>
#include <stdint.h>

#if defined(KORE_PPC)
#define KORE_BIG_ENDIAN
#else
#define KORE_LITTLE_ENDIAN
#endif

#ifdef KORE_WINDOWS
#if defined(KINC_DYNAMIC)
#define KINC_FUNC __declspec(dllimport)
#elif defined(KINC_DYNAMIC_COMPILE)
#define KINC_FUNC __declspec(dllexport)
#else
#define KINC_FUNC
#endif
#else
#define KINC_FUNC
#endif

#if defined(KORE_PPC)
#define KINC_BIG_ENDIAN
#else
#define KINC_LITTLE_ENDIAN
#endif

#ifdef _MSC_VER
#define KINC_INLINE __forceinline
#else
#define KINC_INLINE __attribute__((always_inline))
#endif

#ifdef _MSC_VER
#define KINC_MICROSOFT
#define KORE_MICROSOFT
#endif

#if defined(_WIN32)

#if defined(KORE_WINDOWSAPP)

#define KINC_WINDOWSAPP
#define KINC_WINRT
#define KORE_WINRT

#else

#define KINC_WINDOWS
#define KORE_WINDOWS

#endif

#elif defined(__APPLE__)

#include <TargetConditionals.h>

#if TARGET_OS_IPHONE

#if defined(KORE_TVOS)
#define KINC_TVOS
#else
#define KINC_IOS
#define KORE_IOS
#endif

#define KINC_APPLE_SOC

#else

#define KINC_MACOS
#define KORE_MACOS

#if defined(__arm64__)
#define KINC_APPLE_SOC
#endif

#endif

#define KINC_POSIX
#define KORE_POSIX

#elif defined(__linux__)

#if !defined(KORE_ANDROID)
#define KINC_LINUX
#define KORE_LINUX
#endif

#define KINC_POSIX
#define KORE_POSIX

#endif

#ifdef __cplusplus
extern "C" {
#endif
int kickstart(int argc, char **argv);
#ifdef __cplusplus
}
#endif
