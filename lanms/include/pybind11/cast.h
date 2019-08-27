
/*
    pybind11/cast.h: Partial template specializations to cast between
    C++ and Python types

    Copyright (c) 2016 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include "pytypes.h"
#include "typeid.h"
#include "descr.h"
#include <array>
#include <limits>
#include <tuple>

#if defined(PYBIND11_CPP17)
#  if defined(__has_include)
#    if __has_include(<string_view>)
#      define PYBIND11_HAS_STRING_VIEW
#    endif
#  elif defined(_MSC_VER)
#    define PYBIND11_HAS_STRING_VIEW
#  endif
#endif
#ifdef PYBIND11_HAS_STRING_VIEW
#include <string_view>
#endif

NAMESPACE_BEGIN(pybind11)
NAMESPACE_BEGIN(detail)
// Forward declarations:
inline PyTypeObject *make_static_property_type();
inline PyTypeObject *make_default_metaclass();
inline PyObject *make_object_base_type(PyTypeObject *metaclass);
struct value_and_holder;

/// Additional type information which does not fit into the PyTypeObject
struct type_info {
    PyTypeObject *type;
    const std::type_info *cpptype;
    size_t type_size, holder_size_in_ptrs;
    void *(*operator_new)(size_t);
    void (*init_instance)(instance *, const void *);
    void (*dealloc)(const value_and_holder &v_h);
    std::vector<PyObject *(*)(PyObject *, PyTypeObject *)> implicit_conversions;
    std::vector<std::pair<const std::type_info *, void *(*)(void *)>> implicit_casts;
    std::vector<bool (*)(PyObject *, void *&)> *direct_conversions;
    buffer_info *(*get_buffer)(PyObject *, void *) = nullptr;
    void *get_buffer_data = nullptr;
    /* A simple type never occurs as a (direct or indirect) parent
     * of a class that makes use of multiple inheritance */
    bool simple_type : 1;
    /* True if there is no multiple inheritance in this type's inheritance tree */
    bool simple_ancestors : 1;
    /* for base vs derived holder_type checks */
    bool default_holder : 1;
};

// Store the static internals pointer in a version-specific function so that we're guaranteed it
// will be distinct for modules compiled for different pybind11 versions.  Without this, some
// compilers (i.e. gcc) can use the same static pointer storage location across different .so's,
// even though the `get_internals()` function itself is local to each shared object.
template <int = PYBIND11_VERSION_MAJOR, int = PYBIND11_VERSION_MINOR>
internals *&get_internals_ptr() { static internals *internals_ptr = nullptr; return internals_ptr; }

PYBIND11_NOINLINE inline internals &get_internals() {
    internals *&internals_ptr = get_internals_ptr();
    if (internals_ptr)
        return *internals_ptr;
    handle builtins(PyEval_GetBuiltins());
    const char *id = PYBIND11_INTERNALS_ID;
    if (builtins.contains(id) && isinstance<capsule>(builtins[id])) {
        internals_ptr = *static_cast<internals **>(capsule(builtins[id]));
    } else {
        internals_ptr = new internals();
        #if defined(WITH_THREAD)
            PyEval_InitThreads();
            PyThreadState *tstate = PyThreadState_Get();
            internals_ptr->tstate = PyThread_create_key();
            PyThread_set_key_value(internals_ptr->tstate, tstate);
            internals_ptr->istate = tstate->interp;
        #endif
        builtins[id] = capsule(&internals_ptr);
        internals_ptr->registered_exception_translators.push_front(
            [](std::exception_ptr p) -> void {
                try {
                    if (p) std::rethrow_exception(p);
                } catch (error_already_set &e)           { e.restore();                                    return;
                } catch (const builtin_exception &e)     { e.set_error();                                  return;
                } catch (const std::bad_alloc &e)        { PyErr_SetString(PyExc_MemoryError,   e.what()); return;
                } catch (const std::domain_error &e)     { PyErr_SetString(PyExc_ValueError,    e.what()); return;
                } catch (const std::invalid_argument &e) { PyErr_SetString(PyExc_ValueError,    e.what()); return;
                } catch (const std::length_error &e)     { PyErr_SetString(PyExc_ValueError,    e.what()); return;
                } catch (const std::out_of_range &e)     { PyErr_SetString(PyExc_IndexError,    e.what()); return;
                } catch (const std::range_error &e)      { PyErr_SetString(PyExc_ValueError,    e.what()); return;
                } catch (const std::exception &e)        { PyErr_SetString(PyExc_RuntimeError,  e.what()); return;
                } catch (...) {
                    PyErr_SetString(PyExc_RuntimeError, "Caught an unknown exception!");
                    return;
                }
            }
        );
        internals_ptr->static_property_type = make_static_property_type();
        internals_ptr->default_metaclass = make_default_metaclass();
        internals_ptr->instance_base = make_object_base_type(internals_ptr->default_metaclass);
    }
    return *internals_ptr;
}

/// A life support system for temporary objects created by `type_caster::load()`.
/// Adding a patient will keep it alive up until the enclosing function returns.
class loader_life_support {
public:
    /// A new patient frame is created when a function is entered
    loader_life_support() {