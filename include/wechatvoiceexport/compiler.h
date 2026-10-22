#ifndef WECHATVOICEEXPORT_COMPILER_H
#define WECHATVOICEEXPORT_COMPILER_H

/*
 * Compiler and platform detection macros.
 *
 * This header provides a reference pattern for portable C code across the
 * four supported compiler families (MSVC, GCC, Clang, AppleClang) and three
 * target platforms (Windows, Linux, macOS).
 */

/* ---- Compiler family detection ---- */

#define WECHATVOICEEXPORT_COMPILER_MSVC 0
#define WECHATVOICEEXPORT_COMPILER_APPLECLANG 0
#define WECHATVOICEEXPORT_COMPILER_CLANG 0
#define WECHATVOICEEXPORT_COMPILER_GCC 0

#if defined(_MSC_VER)
#undef WECHATVOICEEXPORT_COMPILER_MSVC
#define WECHATVOICEEXPORT_COMPILER_MSVC 1
#elif defined(__clang__) && defined(__APPLE__)
#undef WECHATVOICEEXPORT_COMPILER_APPLECLANG
#define WECHATVOICEEXPORT_COMPILER_APPLECLANG 1
#elif defined(__clang__)
#undef WECHATVOICEEXPORT_COMPILER_CLANG
#define WECHATVOICEEXPORT_COMPILER_CLANG 1
#elif defined(__GNUC__)
#undef WECHATVOICEEXPORT_COMPILER_GCC
#define WECHATVOICEEXPORT_COMPILER_GCC 1
#else
#warning "Unknown compiler. Portable macros may not be available."
#endif

/* ---- Platform detection ---- */

#define WECHATVOICEEXPORT_PLATFORM_WINDOWS 0
#define WECHATVOICEEXPORT_PLATFORM_MACOS 0
#define WECHATVOICEEXPORT_PLATFORM_LINUX 0

#if defined(_WIN32)
#undef WECHATVOICEEXPORT_PLATFORM_WINDOWS
#define WECHATVOICEEXPORT_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
#undef WECHATVOICEEXPORT_PLATFORM_MACOS
#define WECHATVOICEEXPORT_PLATFORM_MACOS 1
#elif defined(__linux__)
#undef WECHATVOICEEXPORT_PLATFORM_LINUX
#define WECHATVOICEEXPORT_PLATFORM_LINUX 1
#endif

/* ---- Portable function attributes ---- */

#if WECHATVOICEEXPORT_COMPILER_MSVC
#define WECHATVOICEEXPORT_NORETURN __declspec(noreturn)
#define WECHATVOICEEXPORT_DEPRECATED(msg) __declspec(deprecated(msg))
#else
#define WECHATVOICEEXPORT_NORETURN __attribute__((__noreturn__))
#define WECHATVOICEEXPORT_DEPRECATED(msg) __attribute__((__deprecated__(msg)))
#endif

#endif /* WECHATVOICEEXPORT_COMPILER_H */
