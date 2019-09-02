
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
        try {                                                                  \
            pybind11_init_##name(m);                                           \
            return m.ptr();                                                    \
        } catch (pybind11::error_already_set &e) {                             \
            PyErr_SetString(PyExc_ImportError, e.what());                      \
            return nullptr;                                                    \
        } catch (const std::exception &e) {                                    \
            PyErr_SetString(PyExc_ImportError, e.what());                      \
            return nullptr;                                                    \
        }                                                                      \
    }                                                                          \
    void pybind11_init_##name(pybind11::module &variable)


NAMESPACE_BEGIN(pybind11)

using ssize_t = Py_ssize_t;
using size_t  = std::size_t;

/// Approach used to cast a previously unknown C++ instance into a Python object
enum class return_value_policy : uint8_t {
    /** This is the default return value policy, which falls back to the policy
        return_value_policy::take_ownership when the return value is a pointer.
        Otherwise, it uses return_value::move or return_value::copy for rvalue
        and lvalue references, respectively. See below for a description of what
        all of these different policies do. */
    automatic = 0,

    /** As above, but use policy return_value_policy::reference when the return
        value is a pointer. This is the default conversion policy for function
        arguments when calling Python functions manually from C++ code (i.e. via
        handle::operator()). You probably won't need to use this. */
    automatic_reference,

    /** Reference an existing object (i.e. do not create a new copy) and take
        ownership. Python will call the destructor and delete operator when the
        object’s reference count reaches zero. Undefined behavior ensues when
        the C++ side does the same.. */
    take_ownership,

    /** Create a new copy of the returned object, which will be owned by
        Python. This policy is comparably safe because the lifetimes of the two
        instances are decoupled. */
    copy,

    /** Use std::move to move the return value contents into a new instance
        that will be owned by Python. This policy is comparably safe because the
        lifetimes of the two instances (move source and destination) are
        decoupled. */
    move,

    /** Reference an existing object, but do not take ownership. The C++ side
        is responsible for managing the object’s lifetime and deallocating it
        when it is no longer used. Warning: undefined behavior will ensue when
        the C++ side deletes an object that is still referenced and used by
        Python. */
    reference,

    /** This policy only applies to methods and properties. It references the
        object without taking ownership similar to the above
        return_value_policy::reference policy. In contrast to that policy, the
        function or property’s implicit this argument (called the parent) is
        considered to be the the owner of the return value (the child).
        pybind11 then couples the lifetime of the parent to the child via a
        reference relationship that ensures that the parent cannot be garbage
        collected while Python is still using the child. More advanced
        variations of this scheme are also possible using combinations of
        return_value_policy::reference and the keep_alive call policy */
    reference_internal
};

NAMESPACE_BEGIN(detail)

inline static constexpr int log2(size_t n, int k = 0) { return (n <= 1) ? k : log2(n >> 1, k + 1); }

// Returns the size as a multiple of sizeof(void *), rounded up.
inline static constexpr size_t size_in_ptrs(size_t s) { return 1 + ((s - 1) >> log2(sizeof(void *))); }

/**
 * The space to allocate for simple layout instance holders (see below) in multiple of the size of
 * a pointer (e.g.  2 means 16 bytes on 64-bit architectures).  The default is the minimum required
 * to holder either a std::unique_ptr or std::shared_ptr (which is almost always
 * sizeof(std::shared_ptr<T>)).
 */
constexpr size_t instance_simple_holder_in_ptrs() {
    static_assert(sizeof(std::shared_ptr<int>) >= sizeof(std::unique_ptr<int>),
            "pybind assumes std::shared_ptrs are at least as big as std::unique_ptrs");
    return size_in_ptrs(sizeof(std::shared_ptr<int>));
}

// Forward declarations
struct type_info;
struct value_and_holder;

/// The 'instance' type which needs to be standard layout (need to be able to use 'offsetof')
struct instance {
    PyObject_HEAD
    /// Storage for pointers and holder; see simple_layout, below, for a description
    union {
        void *simple_value_holder[1 + instance_simple_holder_in_ptrs()];
        struct {
            void **values_and_holders;
            uint8_t *status;
        } nonsimple;
    };
    /// Weak references (needed for keep alive):
    PyObject *weakrefs;
    /// If true, the pointer is owned which means we're free to manage it with a holder.
    bool owned : 1;
    /**
     * An instance has two possible value/holder layouts.
     *
     * Simple layout (when this flag is true), means the `simple_value_holder` is set with a pointer
     * and the holder object governing that pointer, i.e. [val1*][holder].  This layout is applied
     * whenever there is no python-side multiple inheritance of bound C++ types *and* the type's
     * holder will fit in the default space (which is large enough to hold either a std::unique_ptr
     * or std::shared_ptr).
     *
     * Non-simple layout applies when using custom holders that require more space than `shared_ptr`
     * (which is typically the size of two pointers), or when multiple inheritance is used on the
     * python side.  Non-simple layout allocates the required amount of memory to have multiple
     * bound C++ classes as parents.  Under this layout, `nonsimple.values_and_holders` is set to a
     * pointer to allocated space of the required space to hold a a sequence of value pointers and
     * holders followed `status`, a set of bit flags (1 byte each), i.e.
     * [val1*][holder1][val2*][holder2]...[bb...]  where each [block] is rounded up to a multiple of
     * `sizeof(void *)`.  `nonsimple.holder_constructed` is, for convenience, a pointer to the
     * beginning of the [bb...] block (but not independently allocated).
     *
     * Status bits indicate whether the associated holder is constructed (&
     * status_holder_constructed) and whether the value pointer is registered (&
     * status_instance_registered) in `registered_instances`.
     */
    bool simple_layout : 1;
    /// For simple layout, tracks whether the holder has been constructed
    bool simple_holder_constructed : 1;
    /// For simple layout, tracks whether the instance is registered in `registered_instances`
    bool simple_instance_registered : 1;
    /// If true, get_internals().patients has an entry for this object
    bool has_patients : 1;

    /// Initializes all of the above type/values/holders data
    void allocate_layout();

    /// Destroys/deallocates all of the above
    void deallocate_layout();

    /// Returns the value_and_holder wrapper for the given type (or the first, if `find_type`
    /// omitted)
    value_and_holder get_value_and_holder(const type_info *find_type = nullptr);

    /// Bit values for the non-simple status flags
    static constexpr uint8_t status_holder_constructed  = 1;
    static constexpr uint8_t status_instance_registered = 2;
};

static_assert(std::is_standard_layout<instance>::value, "Internal error: `pybind11::detail::instance` is not standard layout!");

struct overload_hash {
    inline size_t operator()(const std::pair<const PyObject *, const char *>& v) const {
        size_t value = std::hash<const void *>()(v.first);
        value ^= std::hash<const void *>()(v.second)  + 0x9e3779b9 + (value<<6) + (value>>2);
        return value;
    }
};

// Python loads modules by default with dlopen with the RTLD_LOCAL flag; under libc++ and possibly
// other stls, this means `typeid(A)` from one module won't equal `typeid(A)` from another module
// even when `A` is the same, non-hidden-visibility type (e.g. from a common include).  Under
// stdlibc++, this doesn't happen: equality and the type_index hash are based on the type name,
// which works.  If not under a known-good stl, provide our own name-based hasher and equality
// functions that use the type name.
#if defined(__GLIBCXX__)
inline bool same_type(const std::type_info &lhs, const std::type_info &rhs) { return lhs == rhs; }
using type_hash = std::hash<std::type_index>;
using type_equal_to = std::equal_to<std::type_index>;
#else
inline bool same_type(const std::type_info &lhs, const std::type_info &rhs) {
    return lhs.name() == rhs.name() ||
        std::strcmp(lhs.name(), rhs.name()) == 0;
}
struct type_hash {
    size_t operator()(const std::type_index &t) const {