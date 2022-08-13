
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

    .. seealso::
        The `object` class inherits from `handle` and adds automatic reference
        counting features.
\endrst */
class handle : public detail::object_api<handle> {
public:
    /// The default constructor creates a handle with a ``nullptr``-valued pointer
    handle() = default;
    /// Creates a ``handle`` from the given raw Python object pointer
    handle(PyObject *ptr) : m_ptr(ptr) { } // Allow implicit conversion from PyObject*

    /// Return the underlying ``PyObject *`` pointer
    PyObject *ptr() const { return m_ptr; }
    PyObject *&ptr() { return m_ptr; }

    /** \rst
        Manually increase the reference count of the Python object. Usually, it is
        preferable to use the `object` class which derives from `handle` and calls
        this function automatically. Returns a reference to itself.
    \endrst */
    const handle& inc_ref() const & { Py_XINCREF(m_ptr); return *this; }

    /** \rst
        Manually decrease the reference count of the Python object. Usually, it is
        preferable to use the `object` class which derives from `handle` and calls
        this function automatically. Returns a reference to itself.
    \endrst */
    const handle& dec_ref() const & { Py_XDECREF(m_ptr); return *this; }

    /** \rst
        Attempt to cast the Python object into the given C++ type. A `cast_error`
        will be throw upon failure.
    \endrst */
    template <typename T> T cast() const;
    /// Return ``true`` when the `handle` wraps a valid Python object
    explicit operator bool() const { return m_ptr != nullptr; }
    /** \rst
        Deprecated: Check that the underlying pointers are the same.
        Equivalent to ``obj1 is obj2`` in Python.
    \endrst */
    PYBIND11_DEPRECATED("Use obj1.is(obj2) instead")
    bool operator==(const handle &h) const { return m_ptr == h.m_ptr; }
    PYBIND11_DEPRECATED("Use !obj1.is(obj2) instead")
    bool operator!=(const handle &h) const { return m_ptr != h.m_ptr; }
    PYBIND11_DEPRECATED("Use handle::operator bool() instead")
    bool check() const { return m_ptr != nullptr; }
protected:
    PyObject *m_ptr = nullptr;
};

/** \rst
    Holds a reference to a Python object (with reference counting)

    Like `handle`, the `object` class is a thin wrapper around an arbitrary Python
    object (i.e. a ``PyObject *`` in Python's C API). In contrast to `handle`, it
    optionally increases the object's reference count upon construction, and it
    *always* decreases the reference count when the `object` instance goes out of
    scope and is destructed. When using `object` instances consistently, it is much
    easier to get reference counting right at the first attempt.
\endrst */
class object : public handle {
public:
    object() = default;
    PYBIND11_DEPRECATED("Use reinterpret_borrow<object>() or reinterpret_steal<object>()")
    object(handle h, bool is_borrowed) : handle(h) { if (is_borrowed) inc_ref(); }
    /// Copy constructor; always increases the reference count
    object(const object &o) : handle(o) { inc_ref(); }
    /// Move constructor; steals the object from ``other`` and preserves its reference count
    object(object &&other) noexcept { m_ptr = other.m_ptr; other.m_ptr = nullptr; }
    /// Destructor; automatically calls `handle::dec_ref()`
    ~object() { dec_ref(); }

    /** \rst
        Resets the internal pointer to ``nullptr`` without without decreasing the
        object's reference count. The function returns a raw handle to the original
        Python object.
    \endrst */
    handle release() {
      PyObject *tmp = m_ptr;
      m_ptr = nullptr;
      return handle(tmp);
    }

    object& operator=(const object &other) {
        other.inc_ref();
        dec_ref();
        m_ptr = other.m_ptr;
        return *this;
    }

    object& operator=(object &&other) noexcept {
        if (this != &other) {
            handle temp(m_ptr);
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
            temp.dec_ref();
        }
        return *this;
    }

    // Calling cast() on an object lvalue just copies (via handle::cast)
    template <typename T> T cast() const &;
    // Calling on an object rvalue does a move, if needed and/or possible
    template <typename T> T cast() &&;

protected:
    // Tags for choosing constructors from raw PyObject *
    struct borrowed_t { };
    struct stolen_t { };

    template <typename T> friend T reinterpret_borrow(handle);
    template <typename T> friend T reinterpret_steal(handle);

public:
    // Only accessible from derived classes and the reinterpret_* functions
    object(handle h, borrowed_t) : handle(h) { inc_ref(); }
    object(handle h, stolen_t) : handle(h) { }
};

/** \rst
    Declare that a `handle` or ``PyObject *`` is a certain type and borrow the reference.
    The target type ``T`` must be `object` or one of its derived classes. The function
    doesn't do any conversions or checks. It's up to the user to make sure that the
    target type is correct.

    .. code-block:: cpp

        PyObject *p = PyList_GetItem(obj, index);
        py::object o = reinterpret_borrow<py::object>(p);
        // or
        py::tuple t = reinterpret_borrow<py::tuple>(p); // <-- `p` must be already be a `tuple`
\endrst */
template <typename T> T reinterpret_borrow(handle h) { return {h, object::borrowed_t{}}; }

/** \rst
    Like `reinterpret_borrow`, but steals the reference.

     .. code-block:: cpp

        PyObject *p = PyObject_Str(obj);
        py::str s = reinterpret_steal<py::str>(p); // <-- `p` must be already be a `str`
\endrst */
template <typename T> T reinterpret_steal(handle h) { return {h, object::stolen_t{}}; }

NAMESPACE_BEGIN(detail)
inline std::string error_string();
NAMESPACE_END(detail)

/// Fetch and hold an error which was already set in Python.  An instance of this is typically
/// thrown to propagate python-side errors back through C++ which can either be caught manually or
/// else falls back to the function dispatcher (which then raises the captured error back to
/// python).
class error_already_set : public std::runtime_error {
public:
    /// Constructs a new exception from the current Python error indicator, if any.  The current
    /// Python error indicator will be cleared.
    error_already_set() : std::runtime_error(detail::error_string()) {
        PyErr_Fetch(&type.ptr(), &value.ptr(), &trace.ptr());
    }

    inline ~error_already_set();

    /// Give the currently-held error back to Python, if any.  If there is currently a Python error
    /// already set it is cleared first.  After this call, the current object no longer stores the
    /// error variables (but the `.what()` string is still available).
    void restore() { PyErr_Restore(type.release().ptr(), value.release().ptr(), trace.release().ptr()); }

    // Does nothing; provided for backwards compatibility.
    PYBIND11_DEPRECATED("Use of error_already_set.clear() is deprecated")
    void clear() {}

    /// Check if the currently trapped error type matches the given Python exception class (or a
    /// subclass thereof).  May also be passed a tuple to search for any exception class matches in
    /// the given tuple.
    bool matches(handle ex) const { return PyErr_GivenExceptionMatches(ex.ptr(), type.ptr()); }

private:
    object type, value, trace;
};

/** \defgroup python_builtins _
    Unless stated otherwise, the following C++ functions behave the same
    as their Python counterparts.
 */

/** \ingroup python_builtins
    \rst
    Return true if ``obj`` is an instance of ``T``. Type ``T`` must be a subclass of
    `object` or a class which was exposed to Python as ``py::class_<T>``.
\endrst */
template <typename T, detail::enable_if_t<std::is_base_of<object, T>::value, int> = 0>
bool isinstance(handle obj) { return T::check_(obj); }

template <typename T, detail::enable_if_t<!std::is_base_of<object, T>::value, int> = 0>
bool isinstance(handle obj) { return detail::isinstance_generic(obj, typeid(T)); }

template <> inline bool isinstance<handle>(handle obj) = delete;
template <> inline bool isinstance<object>(handle obj) { return obj.ptr() != nullptr; }

/// \ingroup python_builtins
/// Return true if ``obj`` is an instance of the ``type``.
inline bool isinstance(handle obj, handle type) {
    const auto result = PyObject_IsInstance(obj.ptr(), type.ptr());
    if (result == -1)
        throw error_already_set();
    return result != 0;
}

/// \addtogroup python_builtins
/// @{
inline bool hasattr(handle obj, handle name) {
    return PyObject_HasAttr(obj.ptr(), name.ptr()) == 1;
}

inline bool hasattr(handle obj, const char *name) {
    return PyObject_HasAttrString(obj.ptr(), name) == 1;
}

inline object getattr(handle obj, handle name) {
    PyObject *result = PyObject_GetAttr(obj.ptr(), name.ptr());
    if (!result) { throw error_already_set(); }
    return reinterpret_steal<object>(result);
}

inline object getattr(handle obj, const char *name) {
    PyObject *result = PyObject_GetAttrString(obj.ptr(), name);
    if (!result) { throw error_already_set(); }
    return reinterpret_steal<object>(result);
}

inline object getattr(handle obj, handle name, handle default_) {
    if (PyObject *result = PyObject_GetAttr(obj.ptr(), name.ptr())) {
        return reinterpret_steal<object>(result);
    } else {
        PyErr_Clear();
        return reinterpret_borrow<object>(default_);
    }
}

inline object getattr(handle obj, const char *name, handle default_) {
    if (PyObject *result = PyObject_GetAttrString(obj.ptr(), name)) {
        return reinterpret_steal<object>(result);
    } else {
        PyErr_Clear();
        return reinterpret_borrow<object>(default_);
    }
}

inline void setattr(handle obj, handle name, handle value) {
    if (PyObject_SetAttr(obj.ptr(), name.ptr(), value.ptr()) != 0) { throw error_already_set(); }
}

inline void setattr(handle obj, const char *name, handle value) {
    if (PyObject_SetAttrString(obj.ptr(), name, value.ptr()) != 0) { throw error_already_set(); }
}
/// @} python_builtins

NAMESPACE_BEGIN(detail)
inline handle get_function(handle value) {
    if (value) {
#if PY_MAJOR_VERSION >= 3
        if (PyInstanceMethod_Check(value.ptr()))
            value = PyInstanceMethod_GET_FUNCTION(value.ptr());
        else
#endif
        if (PyMethod_Check(value.ptr()))
            value = PyMethod_GET_FUNCTION(value.ptr());
    }
    return value;
}

// Helper aliases/functions to support implicit casting of values given to python accessors/methods.
// When given a pyobject, this simply returns the pyobject as-is; for other C++ type, the value goes
// through pybind11::cast(obj) to convert it to an `object`.
template <typename T, enable_if_t<is_pyobject<T>::value, int> = 0>
auto object_or_cast(T &&o) -> decltype(std::forward<T>(o)) { return std::forward<T>(o); }
// The following casting version is implemented in cast.h:
template <typename T, enable_if_t<!is_pyobject<T>::value, int> = 0>
object object_or_cast(T &&o);
// Match a PyObject*, which we want to convert directly to handle via its converting constructor
inline handle object_or_cast(PyObject *ptr) { return ptr; }


template <typename Policy>
class accessor : public object_api<accessor<Policy>> {
    using key_type = typename Policy::key_type;

public:
    accessor(handle obj, key_type key) : obj(obj), key(std::move(key)) { }
    accessor(const accessor &a) = default;
    accessor(accessor &&a) = default;

    // accessor overload required to override default assignment operator (templates are not allowed
    // to replace default compiler-generated assignments).
    void operator=(const accessor &a) && { std::move(*this).operator=(handle(a)); }
    void operator=(const accessor &a) & { operator=(handle(a)); }

    template <typename T> void operator=(T &&value) && {
        Policy::set(obj, key, object_or_cast(std::forward<T>(value)));
    }
    template <typename T> void operator=(T &&value) & {
        get_cache() = reinterpret_borrow<object>(object_or_cast(std::forward<T>(value)));
    }

    template <typename T = Policy>
    PYBIND11_DEPRECATED("Use of obj.attr(...) as bool is deprecated in favor of pybind11::hasattr(obj, ...)")
    explicit operator enable_if_t<std::is_same<T, accessor_policies::str_attr>::value ||
            std::is_same<T, accessor_policies::obj_attr>::value, bool>() const {
        return hasattr(obj, key);
    }
    template <typename T = Policy>
    PYBIND11_DEPRECATED("Use of obj[key] as bool is deprecated in favor of obj.contains(key)")
    explicit operator enable_if_t<std::is_same<T, accessor_policies::generic_item>::value, bool>() const {
        return obj.contains(key);
    }

    operator object() const { return get_cache(); }
    PyObject *ptr() const { return get_cache().ptr(); }
    template <typename T> T cast() const { return get_cache().template cast<T>(); }
