
/*
    pybind11/embed.h: Support for embedding the interpreter

    Copyright (c) 2017 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include "pybind11.h"
#include "eval.h"

#if defined(PYPY_VERSION)
#  error Embedding the interpreter is not supported with PyPy
#endif

#if PY_MAJOR_VERSION >= 3
#  define PYBIND11_EMBEDDED_MODULE_IMPL(name)            \
      extern "C" PyObject *pybind11_init_impl_##name() { \
          return pybind11_init_wrapper_##name();         \
      }
#else
#  define PYBIND11_EMBEDDED_MODULE_IMPL(name)            \
      extern "C" void pybind11_init_impl_##name() {      \
          pybind11_init_wrapper_##name();                \
      }
#endif

/** \rst
    Add a new module to the table of builtins for the interpreter. Must be
    defined in global scope. The first macro parameter is the name of the
    module (without quotes). The second parameter is the variable which will
    be used as the interface to add functions and classes to the module.

    .. code-block:: cpp

        PYBIND11_EMBEDDED_MODULE(example, m) {
            // ... initialize functions and classes here
            m.def("foo", []() {
                return "Hello, World!";
            });
        }
 \endrst */
#define PYBIND11_EMBEDDED_MODULE(name, variable)                              \
    static void pybind11_init_##name(pybind11::module &);                     \
    static PyObject *pybind11_init_wrapper_##name() {                         \
        auto m = pybind11::module(#name);                                     \
        try {                                                                 \
            pybind11_init_##name(m);                                          \
            return m.ptr();                                                   \
        } catch (pybind11::error_already_set &e) {                            \
            PyErr_SetString(PyExc_ImportError, e.what());                     \
            return nullptr;                                                   \
        } catch (const std::exception &e) {                                   \
            PyErr_SetString(PyExc_ImportError, e.what());                     \
            return nullptr;                                                   \
        }                                                                     \
    }                                                                         \
    PYBIND11_EMBEDDED_MODULE_IMPL(name)                                       \
    pybind11::detail::embedded_module name(#name, pybind11_init_impl_##name); \
    void pybind11_init_##name(pybind11::module &variable)


NAMESPACE_BEGIN(pybind11)
NAMESPACE_BEGIN(detail)

/// Python 2.7/3.x compatible version of `PyImport_AppendInittab` and error checks.
struct embedded_module {
#if PY_MAJOR_VERSION >= 3
    using init_t = PyObject *(*)();
#else
    using init_t = void (*)();