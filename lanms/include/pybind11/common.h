
/*
    pybind11/common.h -- Basic macros

    Copyright (c) 2016 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#if !defined(NAMESPACE_BEGIN)
#  define NAMESPACE_BEGIN(name) namespace name {
#endif
#if !defined(NAMESPACE_END)
#  define NAMESPACE_END(name) }
#endif

#if !defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#  if __cplusplus >= 201402L
#    define PYBIND11_CPP14
#    if __cplusplus > 201402L /* Temporary: should be updated to >= the final C++17 value once known */
#      define PYBIND11_CPP17
#    endif
#  endif
#elif defined(_MSC_VER)
// MSVC sets _MSVC_LANG rather than __cplusplus (supposedly until the standard is fully implemented)
#  if _MSVC_LANG >= 201402L
#    define PYBIND11_CPP14
#    if _MSVC_LANG > 201402L && _MSC_VER >= 1910
#      define PYBIND11_CPP17
#    endif
#  endif
#endif

// Compiler version assertions
#if defined(__INTEL_COMPILER)
#  if __INTEL_COMPILER < 1500
#    error pybind11 requires Intel C++ compiler v15 or newer
#  endif
#elif defined(__clang__) && !defined(__apple_build_version__)
#  if __clang_major__ < 3 || (__clang_major__ == 3 && __clang_minor__ < 3)
#    error pybind11 requires clang 3.3 or newer
#  endif
#elif defined(__clang__)
// Apple changes clang version macros to its Xcode version; the first Xcode release based on
// (upstream) clang 3.3 was Xcode 5:
#  if __clang_major__ < 5
#    error pybind11 requires Xcode/clang 5.0 or newer
#  endif
#elif defined(__GNUG__)
#  if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
#    error pybind11 requires gcc 4.8 or newer
#  endif
#elif defined(_MSC_VER)
// Pybind hits various compiler bugs in 2015u2 and earlier, and also makes use of some stl features
// (e.g. std::negation) added in 2015u3:
#  if _MSC_FULL_VER < 190024210
#    error pybind11 requires MSVC 2015 update 3 or newer
#  endif
#endif

#if !defined(PYBIND11_EXPORT)
#  if defined(WIN32) || defined(_WIN32)
#    define PYBIND11_EXPORT __declspec(dllexport)
#  else
#    define PYBIND11_EXPORT __attribute__ ((visibility("default")))
#  endif
#endif

#if defined(_MSC_VER)
#  define PYBIND11_NOINLINE __declspec(noinline)
#else
#  define PYBIND11_NOINLINE __attribute__ ((noinline))
#endif

#if defined(PYBIND11_CPP14)
#  define PYBIND11_DEPRECATED(reason) [[deprecated(reason)]]
#else
#  define PYBIND11_DEPRECATED(reason) __attribute__((deprecated(reason)))
#endif

#define PYBIND11_VERSION_MAJOR 2
#define PYBIND11_VERSION_MINOR 2
#define PYBIND11_VERSION_PATCH dev0

/// Include Python header, disable linking to pythonX_d.lib on Windows in debug mode
#if defined(_MSC_VER)
#  if (PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 4)
#    define HAVE_ROUND 1
#  endif
#  pragma warning(push)
#  pragma warning(disable: 4510 4610 4512 4005)
#  if defined(_DEBUG)
#    define PYBIND11_DEBUG_MARKER
#    undef _DEBUG
#  endif
#endif

#include <Python.h>
#include <frameobject.h>
#include <pythread.h>

#if defined(_WIN32) && (defined(min) || defined(max))
#  error Macro clash with min and max -- define NOMINMAX when compiling your program on Windows
#endif

#if defined(isalnum)
#  undef isalnum
#  undef isalpha
#  undef islower
#  undef isspace
#  undef isupper
#  undef tolower
#  undef toupper
#endif

#if defined(_MSC_VER)