
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
        get_internals().loader_patient_stack.push_back(nullptr);
    }

    /// ... and destroyed after it returns
    ~loader_life_support() {
        auto &stack = get_internals().loader_patient_stack;
        if (stack.empty())
            pybind11_fail("loader_life_support: internal error");

        auto ptr = stack.back();
        stack.pop_back();
        Py_CLEAR(ptr);

        // A heuristic to reduce the stack's capacity (e.g. after long recursive calls)
        if (stack.capacity() > 16 && stack.size() != 0 && stack.capacity() / stack.size() > 2)
            stack.shrink_to_fit();
    }

    /// This can only be used inside a pybind11-bound function, either by `argument_loader`
    /// at argument preparation time or by `py::cast()` at execution time.
    PYBIND11_NOINLINE static void add_patient(handle h) {
        auto &stack = get_internals().loader_patient_stack;
        if (stack.empty())
            throw cast_error("When called outside a bound function, py::cast() cannot "
                             "do Python -> C++ conversions which require the creation "
                             "of temporary values");

        auto &list_ptr = stack.back();
        if (list_ptr == nullptr) {
            list_ptr = PyList_New(1);
            if (!list_ptr)
                pybind11_fail("loader_life_support: error allocating list");
            PyList_SET_ITEM(list_ptr, 0, h.inc_ref().ptr());
        } else {
            auto result = PyList_Append(list_ptr, h.ptr());
            if (result == -1)
                pybind11_fail("loader_life_support: error adding patient");
        }
    }
};

// Gets the cache entry for the given type, creating it if necessary.  The return value is the pair
// returned by emplace, i.e. an iterator for the entry and a bool set to `true` if the entry was
// just created.
inline std::pair<decltype(internals::registered_types_py)::iterator, bool> all_type_info_get_cache(PyTypeObject *type);

// Populates a just-created cache entry.
PYBIND11_NOINLINE inline void all_type_info_populate(PyTypeObject *t, std::vector<type_info *> &bases) {
    std::vector<PyTypeObject *> check;
    for (handle parent : reinterpret_borrow<tuple>(t->tp_bases))
        check.push_back((PyTypeObject *) parent.ptr());

    auto const &type_dict = get_internals().registered_types_py;
    for (size_t i = 0; i < check.size(); i++) {
        auto type = check[i];
        // Ignore Python2 old-style class super type:
        if (!PyType_Check((PyObject *) type)) continue;

        // Check `type` in the current set of registered python types:
        auto it = type_dict.find(type);
        if (it != type_dict.end()) {
            // We found a cache entry for it, so it's either pybind-registered or has pre-computed
            // pybind bases, but we have to make sure we haven't already seen the type(s) before: we
            // want to follow Python/virtual C++ rules that there should only be one instance of a
            // common base.
            for (auto *tinfo : it->second) {
                // NB: Could use a second set here, rather than doing a linear search, but since
                // having a large number of immediate pybind11-registered types seems fairly
                // unlikely, that probably isn't worthwhile.
                bool found = false;
                for (auto *known : bases) {
                    if (known == tinfo) { found = true; break; }
                }
                if (!found) bases.push_back(tinfo);
            }
        }
        else if (type->tp_bases) {
            // It's some python type, so keep follow its bases classes to look for one or more
            // registered types
            if (i + 1 == check.size()) {
                // When we're at the end, we can pop off the current element to avoid growing
                // `check` when adding just one base (which is typical--.e. when there is no
                // multiple inheritance)
                check.pop_back();
                i--;
            }
            for (handle parent : reinterpret_borrow<tuple>(type->tp_bases))
                check.push_back((PyTypeObject *) parent.ptr());
        }
    }
}

/**
 * Extracts vector of type_info pointers of pybind-registered roots of the given Python type.  Will
 * be just 1 pybind type for the Python type of a pybind-registered class, or for any Python-side
 * derived class that uses single inheritance.  Will contain as many types as required for a Python
 * class that uses multiple inheritance to inherit (directly or indirectly) from multiple
 * pybind-registered classes.  Will be empty if neither the type nor any base classes are
 * pybind-registered.
 *
 * The value is cached for the lifetime of the Python type.
 */
inline const std::vector<detail::type_info *> &all_type_info(PyTypeObject *type) {
    auto ins = all_type_info_get_cache(type);
    if (ins.second)
        // New cache entry: populate it
        all_type_info_populate(type, ins.first->second);

    return ins.first->second;
}

/**
 * Gets a single pybind11 type info for a python type.  Returns nullptr if neither the type nor any
 * ancestors are pybind11-registered.  Throws an exception if there are multiple bases--use
 * `all_type_info` instead if you want to support multiple bases.
 */
PYBIND11_NOINLINE inline detail::type_info* get_type_info(PyTypeObject *type) {
    auto &bases = all_type_info(type);
    if (bases.size() == 0)
        return nullptr;
    if (bases.size() > 1)
        pybind11_fail("pybind11::detail::get_type_info: type has multiple pybind11-registered bases");
    return bases.front();
}

PYBIND11_NOINLINE inline detail::type_info *get_type_info(const std::type_info &tp,
                                                          bool throw_if_missing = false) {
    auto &types = get_internals().registered_types_cpp;

    auto it = types.find(std::type_index(tp));
    if (it != types.end())
        return (detail::type_info *) it->second;
    if (throw_if_missing) {
        std::string tname = tp.name();
        detail::clean_type_id(tname);
        pybind11_fail("pybind11::detail::get_type_info: unable to find type info for \"" + tname + "\"");
    }
    return nullptr;
}

PYBIND11_NOINLINE inline handle get_type_handle(const std::type_info &tp, bool throw_if_missing) {
    detail::type_info *type_info = get_type_info(tp, throw_if_missing);
    return handle(type_info ? ((PyObject *) type_info->type) : nullptr);
}

struct value_and_holder {
    instance *inst;
    size_t index;
    const detail::type_info *type;
    void **vh;

    value_and_holder(instance *i, const detail::type_info *type, size_t vpos, size_t index) :
        inst{i}, index{index}, type{type},
        vh{inst->simple_layout ? inst->simple_value_holder : &inst->nonsimple.values_and_holders[vpos]}
    {}

    // Used for past-the-end iterator
    value_and_holder(size_t index) : index{index} {}

    template <typename V = void> V *&value_ptr() const {
        return reinterpret_cast<V *&>(vh[0]);
    }
    // True if this `value_and_holder` has a non-null value pointer
    explicit operator bool() const { return value_ptr(); }

    template <typename H> H &holder() const {
        return reinterpret_cast<H &>(vh[1]);
    }