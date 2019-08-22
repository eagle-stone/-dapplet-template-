
/*
    pybind11/attr.h: Infrastructure for processing custom
    type and function attributes

    Copyright (c) 2016 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include "cast.h"

NAMESPACE_BEGIN(pybind11)

/// \addtogroup annotations
/// @{

/// Annotation for methods
struct is_method { handle class_; is_method(const handle &c) : class_(c) { } };

/// Annotation for operators
struct is_operator { };

/// Annotation for parent scope
struct scope { handle value; scope(const handle &s) : value(s) { } };

/// Annotation for documentation
struct doc { const char *value; doc(const char *value) : value(value) { } };

/// Annotation for function names
struct name { const char *value; name(const char *value) : value(value) { } };

/// Annotation indicating that a function is an overload associated with a given "sibling"
struct sibling { handle value; sibling(const handle &value) : value(value.ptr()) { } };

/// Annotation indicating that a class derives from another given type
template <typename T> struct base {
    PYBIND11_DEPRECATED("base<T>() was deprecated in favor of specifying 'T' as a template argument to class_")
    base() { }
};

/// Keep patient alive while nurse lives
template <size_t Nurse, size_t Patient> struct keep_alive { };

/// Annotation indicating that a class is involved in a multiple inheritance relationship
struct multiple_inheritance { };

/// Annotation which enables dynamic attributes, i.e. adds `__dict__` to a class
struct dynamic_attr { };

/// Annotation which enables the buffer protocol for a type
struct buffer_protocol { };

/// Annotation which requests that a special metaclass is created for a type
struct metaclass {
    handle value;

    PYBIND11_DEPRECATED("py::metaclass() is no longer required. It's turned on by default now.")
    metaclass() {}

    /// Override pybind11's default metaclass
    explicit metaclass(handle value) : value(value) { }
};

/// Annotation to mark enums as an arithmetic type
struct arithmetic { };

/** \rst
    A call policy which places one or more guard variables (``Ts...``) around the function call.

    For example, this definition:

    .. code-block:: cpp

        m.def("foo", foo, py::call_guard<T>());

    is equivalent to the following pseudocode:

    .. code-block:: cpp

        m.def("foo", [](args...) {
            T scope_guard;
            return foo(args...); // forwarded arguments
        });
 \endrst */
template <typename... Ts> struct call_guard;

template <> struct call_guard<> { using type = detail::void_type; };

template <typename T>
struct call_guard<T> {
    static_assert(std::is_default_constructible<T>::value,
                  "The guard type must be default constructible");

    using type = T;
};

template <typename T, typename... Ts>
struct call_guard<T, Ts...> {
    struct type {
        T guard{}; // Compose multiple guard types with left-to-right default-constructor order
        typename call_guard<Ts...>::type next{};
    };
};

/// @} annotations

NAMESPACE_BEGIN(detail)
/* Forward declarations */
enum op_id : int;
enum op_type : int;
struct undefined_t;
template <op_id id, op_type ot, typename L = undefined_t, typename R = undefined_t> struct op_;
template <typename... Args> struct init;
template <typename... Args> struct init_alias;
inline void keep_alive_impl(size_t Nurse, size_t Patient, function_call &call, handle ret);

/// Internal data structure which holds metadata about a keyword argument
struct argument_record {
    const char *name;  ///< Argument name
    const char *descr; ///< Human-readable version of the argument value
    handle value;      ///< Associated Python object
    bool convert : 1;  ///< True if the argument is allowed to convert when loading
    bool none : 1;     ///< True if None is allowed when loading

    argument_record(const char *name, const char *descr, handle value, bool convert, bool none)
        : name(name), descr(descr), value(value), convert(convert), none(none) { }
};

/// Internal data structure which holds metadata about a bound function (signature, overloads, etc.)
struct function_record {
    function_record()
        : is_constructor(false), is_stateless(false), is_operator(false),
          has_args(false), has_kwargs(false), is_method(false) { }

    /// Function name
    char *name = nullptr; /* why no C++ strings? They generate heavier code.. */

    // User-specified documentation string
    char *doc = nullptr;

    /// Human-readable version of the function signature
    char *signature = nullptr;

    /// List of registered keyword arguments
    std::vector<argument_record> args;

    /// Pointer to lambda function which converts arguments and performs the actual call
    handle (*impl) (function_call &) = nullptr;

    /// Storage for the wrapped function pointer and captured data, if any
    void *data[3] = { };

    /// Pointer to custom destructor for 'data' (if needed)
    void (*free_data) (function_record *ptr) = nullptr;

    /// Return value policy associated with this function
    return_value_policy policy = return_value_policy::automatic;

    /// True if name == '__init__'
    bool is_constructor : 1;

    /// True if this is a stateless function pointer
    bool is_stateless : 1;

    /// True if this is an operator (__add__), etc.
    bool is_operator : 1;

    /// True if the function has a '*args' argument
    bool has_args : 1;

    /// True if the function has a '**kwargs' argument
    bool has_kwargs : 1;

    /// True if this is a method
    bool is_method : 1;

    /// Number of arguments (including py::args and/or py::kwargs, if present)
    std::uint16_t nargs;

    /// Python method object
    PyMethodDef *def = nullptr;

    /// Python handle to the parent scope (a class or a module)
    handle scope;

    /// Python handle to the sibling function representing an overload chain
    handle sibling;

    /// Pointer to next overload
    function_record *next = nullptr;
};

/// Special data structure which (temporarily) holds metadata about a bound class
struct type_record {
    PYBIND11_NOINLINE type_record()
        : multiple_inheritance(false), dynamic_attr(false), buffer_protocol(false) { }

    /// Handle to the parent scope
    handle scope;

    /// Name of the class
    const char *name = nullptr;

    // Pointer to RTTI type_info data structure
    const std::type_info *type = nullptr;

    /// How large is the underlying C++ type?
    size_t type_size = 0;

    /// How large is the type's holder?
    size_t holder_size = 0;

    /// The global operator new can be overridden with a class-specific variant
    void *(*operator_new)(size_t) = ::operator new;

    /// Function pointer to class_<..>::init_instance
    void (*init_instance)(instance *, const void *) = nullptr;

    /// Function pointer to class_<..>::dealloc
    void (*dealloc)(const detail::value_and_holder &) = nullptr;

    /// List of base classes of the newly created type
    list bases;

    /// Optional docstring
    const char *doc = nullptr;

    /// Custom metaclass (optional)
    handle metaclass;

    /// Multiple inheritance marker
    bool multiple_inheritance : 1;

    /// Does the class manage a __dict__?
    bool dynamic_attr : 1;

    /// Does the class implement the buffer protocol?
    bool buffer_protocol : 1;

    /// Is the default (unique_ptr) holder type used?
    bool default_holder : 1;

    PYBIND11_NOINLINE void add_base(const std::type_info &base, void *(*caster)(void *)) {
        auto base_info = detail::get_type_info(base, false);
        if (!base_info) {
            std::string tname(base.name());
            detail::clean_type_id(tname);
            pybind11_fail("generic_type: type \"" + std::string(name) +
                          "\" referenced unknown base type \"" + tname + "\"");
        }

        if (default_holder != base_info->default_holder) {
            std::string tname(base.name());
            detail::clean_type_id(tname);
            pybind11_fail("generic_type: type \"" + std::string(name) + "\" " +
                    (default_holder ? "does not have" : "has") +
                    " a non-default holder type while its base \"" + tname + "\" " +
                    (base_info->default_holder ? "does not" : "does"));
        }

        bases.append((PyObject *) base_info->type);

        if (base_info->type->tp_dictoffset != 0)
            dynamic_attr = true;

        if (caster)
            base_info->implicit_casts.emplace_back(type, caster);
    }
};

inline function_call::function_call(function_record &f, handle p) :
        func(f), parent(p) {
    args.reserve(f.nargs);
    args_convert.reserve(f.nargs);
}

/**