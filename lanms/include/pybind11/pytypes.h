
/*
    pybind11/typeid.h: Convenience wrapper classes for basic Python types

    Copyright (c) 2016 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include "common.h"
#include "buffer_info.h"
#include <utility>
#include <type_traits>

NAMESPACE_BEGIN(pybind11)

/* A few forward declarations */
class handle; class object;
class str; class iterator;
struct arg; struct arg_v;

NAMESPACE_BEGIN(detail)
class args_proxy;
inline bool isinstance_generic(handle obj, const std::type_info &tp);

// Accessor forward declarations
template <typename Policy> class accessor;
namespace accessor_policies {
    struct obj_attr;
    struct str_attr;
    struct generic_item;
    struct sequence_item;
    struct list_item;
    struct tuple_item;
}
using obj_attr_accessor = accessor<accessor_policies::obj_attr>;
using str_attr_accessor = accessor<accessor_policies::str_attr>;
using item_accessor = accessor<accessor_policies::generic_item>;
using sequence_accessor = accessor<accessor_policies::sequence_item>;
using list_accessor = accessor<accessor_policies::list_item>;
using tuple_accessor = accessor<accessor_policies::tuple_item>;

/// Tag and check to identify a class which implements the Python object API
class pyobject_tag { };
template <typename T> using is_pyobject = std::is_base_of<pyobject_tag, remove_reference_t<T>>;

/** \rst
    A mixin class which adds common functions to `handle`, `object` and various accessors.
    The only requirement for `Derived` is to implement ``PyObject *Derived::ptr() const``.
\endrst */
template <typename Derived>
class object_api : public pyobject_tag {
    const Derived &derived() const { return static_cast<const Derived &>(*this); }

public:
    /** \rst
        Return an iterator equivalent to calling ``iter()`` in Python. The object
        must be a collection which supports the iteration protocol.
    \endrst */
    iterator begin() const;
    /// Return a sentinel which ends iteration.
    iterator end() const;

    /** \rst
        Return an internal functor to invoke the object's sequence protocol. Casting
        the returned ``detail::item_accessor`` instance to a `handle` or `object`
        subclass causes a corresponding call to ``__getitem__``. Assigning a `handle`
        or `object` subclass causes a call to ``__setitem__``.
    \endrst */
    item_accessor operator[](handle key) const;
    /// See above (the only difference is that they key is provided as a string literal)
    item_accessor operator[](const char *key) const;

    /** \rst
        Return an internal functor to access the object's attributes. Casting the
        returned ``detail::obj_attr_accessor`` instance to a `handle` or `object`
        subclass causes a corresponding call to ``getattr``. Assigning a `handle`
        or `object` subclass causes a call to ``setattr``.
    \endrst */
    obj_attr_accessor attr(handle key) const;
    /// See above (the only difference is that they key is provided as a string literal)
    str_attr_accessor attr(const char *key) const;

    /** \rst
        Matches * unpacking in Python, e.g. to unpack arguments out of a ``tuple``
        or ``list`` for a function call. Applying another * to the result yields
        ** unpacking, e.g. to unpack a dict as function keyword arguments.
        See :ref:`calling_python_functions`.
    \endrst */
    args_proxy operator*() const;

    /// Check if the given item is contained within this object, i.e. ``item in obj``.
    template <typename T> bool contains(T &&item) const;

    /** \rst
        Assuming the Python object is a function or implements the ``__call__``
        protocol, ``operator()`` invokes the underlying function, passing an
        arbitrary set of parameters. The result is returned as a `object` and
        may need to be converted back into a Python object using `handle::cast()`.

        When some of the arguments cannot be converted to Python objects, the
        function will throw a `cast_error` exception. When the Python function
        call fails, a `error_already_set` exception is thrown.
    \endrst */
    template <return_value_policy policy = return_value_policy::automatic_reference, typename... Args>
    object operator()(Args &&...args) const;
    template <return_value_policy policy = return_value_policy::automatic_reference, typename... Args>
    PYBIND11_DEPRECATED("call(...) was deprecated in favor of operator()(...)")
        object call(Args&&... args) const;

    /// Equivalent to ``obj is other`` in Python.
    bool is(object_api const& other) const { return derived().ptr() == other.derived().ptr(); }
    /// Equivalent to ``obj is None`` in Python.
    bool is_none() const { return derived().ptr() == Py_None; }
    PYBIND11_DEPRECATED("Use py::str(obj) instead")
    pybind11::str str() const;

    /// Get or set the object's docstring, i.e. ``obj.__doc__``.
    str_attr_accessor doc() const;

    /// Return the object's current reference count
    int ref_count() const { return static_cast<int>(Py_REFCNT(derived().ptr())); }
    /// Return a handle to the Python type object underlying the instance
    handle get_type() const;
};

NAMESPACE_END(detail)

/** \rst
    Holds a reference to a Python object (no reference counting)

    The `handle` class is a thin wrapper around an arbitrary Python object (i.e. a
    ``PyObject *`` in Python's C API). It does not perform any automatic reference
    counting and merely provides a basic C++ interface to various Python API functions.
