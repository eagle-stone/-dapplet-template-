
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
#  if defined(PYBIND11_DEBUG_MARKER)
#    define _DEBUG
#    undef PYBIND11_DEBUG_MARKER
#  endif
#  pragma warning(pop)
#endif

#include <cstddef>
#include <cstring>
#include <forward_list>
#include <vector>
#include <string>
#include <stdexcept>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <typeindex>
#include <type_traits>

#if PY_MAJOR_VERSION >= 3 /// Compatibility macros for various Python versions
#define PYBIND11_INSTANCE_METHOD_NEW(ptr, class_) PyInstanceMethod_New(ptr)
#define PYBIND11_INSTANCE_METHOD_CHECK PyInstanceMethod_Check
#define PYBIND11_INSTANCE_METHOD_GET_FUNCTION PyInstanceMethod_GET_FUNCTION
#define PYBIND11_BYTES_CHECK PyBytes_Check
#define PYBIND11_BYTES_FROM_STRING PyBytes_FromString
#define PYBIND11_BYTES_FROM_STRING_AND_SIZE PyBytes_FromStringAndSize
#define PYBIND11_BYTES_AS_STRING_AND_SIZE PyBytes_AsStringAndSize
#define PYBIND11_BYTES_AS_STRING PyBytes_AsString
#define PYBIND11_BYTES_SIZE PyBytes_Size
#define PYBIND11_LONG_CHECK(o) PyLong_Check(o)
#define PYBIND11_LONG_AS_LONGLONG(o) PyLong_AsLongLong(o)
#define PYBIND11_BYTES_NAME "bytes"
#define PYBIND11_STRING_NAME "str"
#define PYBIND11_SLICE_OBJECT PyObject
#define PYBIND11_FROM_STRING PyUnicode_FromString
#define PYBIND11_STR_TYPE ::pybind11::str
#define PYBIND11_BOOL_ATTR "__bool__"
#define PYBIND11_NB_BOOL(ptr) ((ptr)->nb_bool)
#define PYBIND11_PLUGIN_IMPL(name) \
    extern "C" PYBIND11_EXPORT PyObject *PyInit_##name()

#else
#define PYBIND11_INSTANCE_METHOD_NEW(ptr, class_) PyMethod_New(ptr, nullptr, class_)
#define PYBIND11_INSTANCE_METHOD_CHECK PyMethod_Check
#define PYBIND11_INSTANCE_METHOD_GET_FUNCTION PyMethod_GET_FUNCTION
#define PYBIND11_BYTES_CHECK PyString_Check
#define PYBIND11_BYTES_FROM_STRING PyString_FromString
#define PYBIND11_BYTES_FROM_STRING_AND_SIZE PyString_FromStringAndSize
#define PYBIND11_BYTES_AS_STRING_AND_SIZE PyString_AsStringAndSize
#define PYBIND11_BYTES_AS_STRING PyString_AsString
#define PYBIND11_BYTES_SIZE PyString_Size
#define PYBIND11_LONG_CHECK(o) (PyInt_Check(o) || PyLong_Check(o))
#define PYBIND11_LONG_AS_LONGLONG(o) (PyInt_Check(o) ? (long long) PyLong_AsLong(o) : PyLong_AsLongLong(o))
#define PYBIND11_BYTES_NAME "str"
#define PYBIND11_STRING_NAME "unicode"
#define PYBIND11_SLICE_OBJECT PySliceObject
#define PYBIND11_FROM_STRING PyString_FromString
#define PYBIND11_STR_TYPE ::pybind11::bytes
#define PYBIND11_BOOL_ATTR "__nonzero__"
#define PYBIND11_NB_BOOL(ptr) ((ptr)->nb_nonzero)
#define PYBIND11_PLUGIN_IMPL(name) \
    static PyObject *pybind11_init_wrapper();               \
    extern "C" PYBIND11_EXPORT void init##name() {          \
        (void)pybind11_init_wrapper();                      \
    }                                                       \
    PyObject *pybind11_init_wrapper()
#endif

#if PY_VERSION_HEX >= 0x03050000 && PY_VERSION_HEX < 0x03050200
extern "C" {
    struct _Py_atomic_address { void *value; };
    PyAPI_DATA(_Py_atomic_address) _PyThreadState_Current;
}
#endif

#define PYBIND11_TRY_NEXT_OVERLOAD ((PyObject *) 1) // special failure return code
#define PYBIND11_STRINGIFY(x) #x
#define PYBIND11_TOSTRING(x) PYBIND11_STRINGIFY(x)
#define PYBIND11_INTERNALS_ID "__pybind11_" \
    PYBIND11_TOSTRING(PYBIND11_VERSION_MAJOR) "_" PYBIND11_TOSTRING(PYBIND11_VERSION_MINOR) "__"

/** \rst
    ***Deprecated in favor of PYBIND11_MODULE***

    This macro creates the entry point that will be invoked when the Python interpreter
    imports a plugin library. Please create a `module` in the function body and return
    the pointer to its underlying Python object at the end.

    .. code-block:: cpp

        PYBIND11_PLUGIN(example) {
            pybind11::module m("example", "pybind11 example plugin");
            /// Set up bindings here
            return m.ptr();
        }
\endrst */
#define PYBIND11_PLUGIN(name)                                                  \
    PYBIND11_DEPRECATED("PYBIND11_PLUGIN is deprecated, use PYBIND11_MODULE")  \
    static PyObject *pybind11_init();                                          \
    PYBIND11_PLUGIN_IMPL(name) {                                               \
        int major, minor;                                                      \
        if (sscanf(Py_GetVersion(), "%i.%i", &major, &minor) != 2) {           \
            PyErr_SetString(PyExc_ImportError, "Can't parse Python version."); \
            return nullptr;                                                    \
        } else if (major != PY_MAJOR_VERSION || minor != PY_MINOR_VERSION) {   \
            PyErr_Format(PyExc_ImportError,                                    \
                         "Python version mismatch: module was compiled for "   \
                         "version %i.%i, while the interpreter is running "    \
                         "version %i.%i.", PY_MAJOR_VERSION, PY_MINOR_VERSION, \
                         major, minor);                                        \
            return nullptr;                                                    \
        }                                                                      \
        try {                                                                  \
            return pybind11_init();                                            \
        } catch (pybind11::error_already_set &e) {                             \
            PyErr_SetString(PyExc_ImportError, e.what());                      \
            return nullptr;                                                    \
        } catch (const std::exception &e) {                                    \
            PyErr_SetString(PyExc_ImportError, e.what());                      \
            return nullptr;                                                    \
        }                                                                      \
    }                                                                          \
    PyObject *pybind11_init()

/** \rst
    This macro creates the entry point that will be invoked when the Python interpreter
    imports an extension module. The module name is given as the fist argument and it
    should not be in quotes. The second macro argument defines a variable of type
    `py::module` which can be used to initialize the module.

    .. code-block:: cpp

        PYBIND11_MODULE(example, m) {
            m.doc() = "pybind11 example module";

            // Add bindings here
            m.def("foo", []() {
                return "Hello, World!";
            });
        }
\endrst */
#define PYBIND11_MODULE(name, variable)                                        \
    static void pybind11_init_##name(pybind11::module &);                      \
    PYBIND11_PLUGIN_IMPL(name) {                                               \
        int major, minor;                                                      \
        if (sscanf(Py_GetVersion(), "%i.%i", &major, &minor) != 2) {           \
            PyErr_SetString(PyExc_ImportError, "Can't parse Python version."); \
            return nullptr;                                                    \
        } else if (major != PY_MAJOR_VERSION || minor != PY_MINOR_VERSION) {   \
            PyErr_Format(PyExc_ImportError,                                    \
                         "Python version mismatch: module was compiled for "   \
                         "version %i.%i, while the interpreter is running "    \
                         "version %i.%i.", PY_MAJOR_VERSION, PY_MINOR_VERSION, \
                         major, minor);                                        \
            return nullptr;                                                    \
        }                                                                      \
        auto m = pybind11::module(#name);                                      \