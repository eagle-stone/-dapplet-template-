/*
    pybind11/numpy.h: Basic NumPy support, vectorize() wrapper

    Copyright (c) 2016 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include "pybind11.h"
#include "complex.h"
#include <numeric>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <initializer_list>
#include <functional>
#include <utility>
#include <typeindex>

#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4127) // warning C4127: Conditional expression is constant
#endif

/* This will be true on all flat address space platforms and allows us to reduce the
   whole npy_intp / ssize_t / Py_intptr_t business down to just ssize_t for all size
   and dimension types (e.g. shape, strides, indexing), instead of inflicting this
   upon the library user. */
static_assert(sizeof(ssize_t) == sizeof(Py_intptr_t), "ssize_t != Py_intptr_t");

NAMESPACE_BEGIN(pybind11)

class array; // Forward declaration

NAMESPACE_BEGIN(detail)
template <typename type, typename SFINAE = void> struct npy_format_descriptor;

struct PyArrayDescr_Proxy {
    PyObject_HEAD
    PyObject *typeobj;
    char kind;
    char type;
    char byteorder;
    char flags;
    int type_num;
    int elsize;
    int alignment;
    char *subarray;
    PyObject *fields;
    PyObject *names;
};

struct PyArray_Proxy {
    PyObject_HEAD
    char *data;
    int nd;
    ssize_t *dimensions;
    ssize_t *strides;
    PyObject *base;
    PyObject *descr;
    int flags;
};

struct PyVoidScalarObject_Proxy {
    PyObject_VAR_HEAD
    char *obval;
    PyArrayDescr_Proxy *descr;
    int flags;
    PyObject *base;
};

struct numpy_type_info {
    PyObject* dtype_ptr;
    std::string format_str;
};

struct numpy_internals {
    std::unordered_map<std::type_index, numpy_type_info> registered_dtypes;

    numpy_type_info *get_type_info(const std::type_info& tinfo, bool throw_if_missing = true) {
        auto it = registered_dtypes.find(std::type_index(tinfo));
        if (it != registered_dtypes.end())
            return &(it->second);
        if (throw_if_missing)
            pybind11_fail(std::string("NumPy type info missing for ") + tinfo.name());
        return nullptr;
    }

    template<typename T> numpy_type_info *get_type_info(bool throw_if_missing = true) {
        return get_type_info(typeid(typename std::remove_cv<T>::type), throw_if_missing);
    }
};

inline PYBIND11_NOINLINE void load_numpy_internals(numpy_internals* &ptr) {
    ptr = &get_or_create_shared_data<numpy_internals>("_numpy_internals");
}

inline numpy_internals& get_numpy_internals() {
    static numpy_internals* ptr = nullptr;
    if (!ptr)
        load_numpy_internals(ptr);
    return *ptr;
}

struct npy_api {
    enum constants {
        NPY_ARRAY_C_CONTIGUOUS_ = 0x0001,
        NPY_ARRAY_F_CONTIGUOUS_ = 0x0002,
        NPY_ARRAY_OWNDATA_ = 0x0004,
        NPY_ARRAY_FORCECAST_ = 0x0010,
        NPY_ARRAY_ENSUREARRAY_ = 0x0040,
        NPY_ARRAY_ALIGNED_ = 0x0100,
        NPY_ARRAY_WRITEABLE_ = 0x0400,
        NPY_BOOL_ = 0,
        NPY_BYTE_, NPY_UBYTE_,
        NPY_SHORT_, NPY_USHORT_,
        NPY_INT_, NPY_UINT_,
        NPY_LONG_, NPY_ULONG_,
        NPY_LONGLONG_, NPY_ULONGLONG_,
        NPY_FLOAT_, NPY_DOUBLE_, NPY_LONGDOUBLE_,
        NPY_CFLOAT_, NPY_CDOUBLE_, NPY_CLONGDOUBLE_,
        NPY_OBJECT_ = 17,
        NPY_STRING_, NPY_UNICODE_, NPY_VOID_
    };

    typedef struct {
        Py_intptr_t *ptr;
        int len;
    } PyArray_Dims;

    static npy_api& get() {
        static npy_api api = lookup();
        return api;
    }

    bool PyArray_Check_(PyObject *obj) const {
        return (bool) PyObject_TypeCheck(obj, PyArray_Type_);
    }
    bool PyArrayDescr_Check_(PyObject *obj) const {
        return (bool) PyObject_TypeCheck(obj, PyArrayDescr_Type_);
    }

    unsigned int (*PyArray_GetNDArrayCFeatureVersion_)();
    PyObject *(*PyArray_DescrFromType_)(int);
    PyObject *(*PyArray_NewFromDescr_)
        (PyTypeObject *, PyObject *, int, Py_intptr_t *,
         Py_intptr_t *, void *, int, PyObject *);
    PyObject *(*PyArray_DescrNewFromType_)(int);
    int (*PyArray_CopyInto_)(PyObject *, PyObject *);
    PyObject *(*PyArray_NewCopy_)(PyObject *, int);
    PyTypeObject *PyArray_Type_;
    PyTypeObject *PyVoidArrType_Type_;
    PyTypeObject *PyArrayDescr_Type_;
    PyObject *(*PyArray_DescrFromScalar_)(PyObject *);
    PyObject *(*PyArray_FromAny_) (PyObject *, PyObject *, int, int, int, PyObject *);
    int (*PyArray_DescrConverter_) (PyObject *, PyObject **);
    bool (*PyArray_EquivTypes_) (PyObject *, PyObject *);
    int (*PyArray_GetArrayParamsFromObject_)(PyObject *, PyObject *, char, PyObject **, int *,
                                             Py_ssize_t *, PyObject **, PyObject *);
    PyObject *(*PyArray_Squeeze_)(PyObject *);
    int (*PyArray_SetBaseObject_)(PyObject *, PyObject *);
    PyObject* (*PyArray_Resize_)(PyObject*, PyArray_Dims*, int, int);
private:
    enum functions {
        API_PyArray_GetNDArrayCFeatureVersion = 211,
        API_PyArray_Type = 2,
        API_PyArrayDescr_Type = 3,
        API_PyVoidArrType_Type = 39,
        API_PyArray_DescrFromType = 45,
        API_PyArray_DescrFromScalar = 57,
        API_PyArray_FromAny = 69,
        API_PyArray_Resize = 80,
        API_PyArray_CopyInto = 82,
        API_PyArray_NewCopy = 85,
        API_PyArray_NewFromDescr = 94,
        API_PyArray_DescrNewFromType = 9,
        API_PyArray_DescrConverter = 174,
        API_PyArray_EquivTypes = 182,
        API_PyArray_GetArrayParamsFromObject = 278,
        API_PyArray_Squeeze = 136,
        API_PyArray_SetBaseObject = 282
    };

    static npy_api lookup() {
        module m = module::import("numpy.core.multiarray");
        auto c = m.attr("_ARRAY_API");
#if PY_MAJOR_VERSION >= 3
        void **api_ptr = (void **) PyCapsule_GetPointer(c.ptr(), NULL);
#else
        void **api_ptr = (void **) PyCObject_AsVoidPtr(c.ptr());
#endif
        npy_api api;
#define DECL_NPY_API(Func) api.Func##_ = (decltype(api.Func##_)) api_ptr[API_##Func];
        DECL_NPY_API(PyArray_GetNDArrayCFeatureVersion);
        if (api.PyArray_GetNDArrayCFeatureVersion_() < 0x7)
            pybind11_fail("pybind11 numpy support requires numpy >= 1.7.0");
        DECL_NPY_API(PyArray_Type);
        DECL_NPY_API(PyVoidArrType_Type);
        DECL_NPY_API(PyArrayDescr_Type);
        DECL_NPY_API(PyArray_DescrFromType);
        DECL_NPY_API(PyArray_DescrFromScalar);
        DECL_NPY_API(PyArray_FromAny);
        DECL_NPY_API(PyArray_Resize);
        DECL_NPY_API(PyArray_CopyInto);
        DECL_NPY_API(PyArray_NewCopy);
        DECL_NPY_API(PyArray_NewFromDescr);
        DECL_NPY_API(PyArray_DescrNewFromType);
        DECL_NPY_API(PyArray_DescrConverter);
        DECL_NPY_API(PyArray_EquivTypes);
        DECL_NPY_API(PyArray_GetArrayParamsFromObject);
        DECL_NPY_API(PyArray_Squeeze);
        DECL_NPY_API(PyArray_SetBaseObject);
#undef DECL_NPY_API
        return api;
    }
};

inline PyArray_Proxy* array_proxy(void* ptr) {
    return reinterpret_cast<PyArray_Proxy*>(ptr);
}

inline const PyArray_Proxy* array_proxy(const void* ptr) {
    return reinterpret_cast<const PyArray_Proxy*>(ptr);
}

inline PyArrayDescr_Proxy* array_descriptor_proxy(PyObject* ptr) {
   return reinterpret_cast<PyArrayDescr_Proxy*>(ptr);
}

inline const PyArrayDescr_Proxy* array_descriptor_proxy(const PyObject* ptr) {
   return reinterpret_cast<const PyArrayDescr_Proxy*>(ptr);
}

inline bool check_flags(const void* ptr, int flag) {
    return (flag == (array_proxy(ptr)->flags & flag));
}

template <typename T> struct is_std_array : std::false_type { };
template <typename T, size_t N> struct is_std_array<std::array<T, N>> : std::true_type { };
template <typename T> struct is_complex : std::false_type { };
template <typename T> struct is_complex<std::complex<T>> : std::true_type { };

template <typename T> struct array_info_scalar {
    typedef T type;
    static constexpr bool is_array = false;
    static constexpr bool is_empty = false;
    static PYBIND11_DESCR extents() { return _(""); }
    static void append_extents(list& /* shape */) { }
};
// Computes underlying type and a comma-separated list of extents for array
// types (any mix of std::array and built-in arrays). An array of char is
// treated as scalar because it gets special handling.
template <typename T> struct array_info : array_info_scalar<T> { };
template <typename T, size_t N> struct array_info<std::array<T, N>> {
    using type = typename array_info<T>::type;
    static constexpr bool is_array = true;
    static constexpr bool is_empty = (N == 0) || array_info<T>::is_empty;
    static constexpr size_t extent = N;

    // appends the extents to shape
    static void append_extents(list& shape) {
        shape.append(N);
        array_info<T>::append_extents(shape);
    }

    template<typename T2 = T, enable_if_t<!array_info<T2>::is_array, int> = 0>
    static PYBIND11_DESCR extents() {
        return _<N>();
    }

    template<typename T2 = T, enable_if_t<array_info<T2>::is_array, int> = 0>
    static PYBIND11_DESCR extents() {
        return concat(_<N>(), array_info<T>::extents());
    }
};
// For numpy we have special handling for arrays of characters, so we don't include
// the size in the array extents.
template <size_t N> struct array_info<char[N]> : array_info_scalar<char[N]> { };
template <size_t N> struct array_info<std::array<char, N>> : array_info_scalar<std::array<char, N>> { };
template <typename T, size_t N> struct array_info<T[N]> : array_info<std::array<T, N>> { };
template <typename T> using remove_all_extents_t = typename array_info<T>::type;

template <typename T> using is_pod_struct = all_of<
    std::is_standard_layout<T>,     // since we're accessing directly in memory we need a standard layout type
#if !defined(__GNUG__) || defined(__clang__) || __GNUC__ >= 5
    std::is_trivially_copyable<T>,
#else
    // GCC 4 doesn't implement is_trivially_copyable, so approximate it
    std::is_trivially_destructible<T>,
    satisfies_any_of<T, std::has_trivial_copy_constructor, std::has_trivial_copy_assign>,
#endif
    satisfies_none_of<T, std::is_reference, std::is_array, is_std_array, std::is_arithmetic, is_complex, std::is_enum>
>;

template <ssize_t Dim = 0, typename Strides> ssize_t byte_offset_unsafe(const Strides &) { return 0; }
template <ssize_t Dim = 0, typename Strides, typename... Ix>
ssize_t byte_offset_unsafe(const Strides &strides, ssize_t i, Ix... index) {
    return i * strides[Dim] + byte_offset_unsafe<Dim + 1>(strides, index...);
}

/**
 * Proxy class providing unsafe, unchecked const access to array data.  This is constructed through
 * the `unchecked<T, N>()` method of `array` or the `unchecked<N>()` method of `array_t<T>`.  `Dims`
 * will be -1 for dimensions determined at runtime.
 */
template <typename T, ssize_t Dims>
class unchecked_reference {
protected:
    static constexpr bool Dynamic = Dims < 0;
    const unsigned char *data_;
    // Storing the shape & strides in local variables (i.e. these arrays) allows the compiler to
    // make large performance gains on big, nested loops, but requires compile-time dimensions
    conditional_t<Dynamic, const ssize_t *, std::array<ssize_t, (size_t) Dims>>
            shape_, strides_;
    const ssize_t dims_;

    friend class pybind11::array;
    // Constructor for compile-time dimensions:
    template <bool Dyn = Dynamic>
    unchecked_reference(const void *data, const ssize_t *shape, const ssize_t *strides, enable_if_t<!Dyn, ssize_t>)
    : data_{reinterpret_cast<const unsigned char *>(data)}, dims_{Dims} {
        for (size_t i = 0; i < (size_t) dims_; i++) {
            shape_[i] = shape[i];
            strides_[i] = strides[i];
        }
    }
    // Constructor for runtime dimensions:
    template <bool Dyn = Dynamic>
    unchecked_reference(const void *data, const ssize_t *shape, const ssize_t *strides, enable_if_t<Dyn, ssize_t> dims)
    : data_{reinterpret_cast<const unsigned char *>(data)}, shape_{shape}, strides_{strides}, dims_{dims} {}

public:
    /**
     * Unchecked const reference access to data at the given indices.  For a compile-time known
     * number of dimensions, this requires the correct number of arguments; for run-time
     * dimensionality, this is not checked (and so is up to the caller to use safely).
     */
    template <typename... Ix> const T &operator()(Ix... index) const {
        static_assert(sizeof...(Ix) == Dims || Dynamic,
                "Invalid number of indices for unchecked array reference");
        return *reinterpret_cast<const T *>(data_ + byte_offset_unsafe(strides_, ssize_t(index)...));
    }
    /**
     * Unchecked const reference access to data; this operator only participates if the reference
     * is to a 1-dimensional array.  When present, this is exactly equivalent to `obj(index)`.
     */
    template <ssize_t D = Dims, typename = enable_if_t<D == 1 || Dynamic>>
    const T &operator[](ssize_t index) const { return operator()(index); }

    /// Pointer access to the data at the given indices.
    template <typename... Ix> const T *data(Ix... ix) const { return &operator()(ssize_t(ix)...); }

    /// Returns the item size, i.e. sizeof(T)
    constexpr static ssize_t itemsize() { return sizeof(T); }

    /// Returns the shape (i.e. size) of dimension `dim`
    ssize_t shape(ssize_t dim) const { return shape_[(size_t) dim]; }

    /// Returns the number of dimensions of the array
    ssize_t ndim() const { return dims_; }

    /// Returns the total number of elements in the referenced array, i.e. the product of the shapes
    template <bool Dyn = Dynamic>
    enable_if_t<!Dyn, ssize_t> size() const {
        return std::accumulate(shape_.begin(), shape_.end(), (ssize_t) 1, std::multiplies<ssize_t>());
    }
    template <bool Dyn = Dynamic>
    enable_if_t<Dyn, ssize_t> size() const {
        return std::accumulate(shape_, shape_ + ndim(), (ssize_t) 1, std::multiplies<ssize_t>());
    }

    /// Returns the total number of bytes used by the referenced data.  Note that the actual span in
    /// memory may be larger if the referenced array has non-contiguous strides (e.g. for a slice).
    ssize_t nbytes() const {
        return size() * itemsize();
    }
};

template <typename T, ssize_t Dims>
class unchecked_mutable_reference : public unchecked_reference<T, Dims> {
    friend class pybind11::array;
    using ConstBase = unchecked_reference<T, Dims>;
    using ConstBase::ConstBase;
    using ConstBase::Dynamic;
public:
    /// Mutable, unchecked access to data at the given indices.
    template <typename... Ix> T& operator()(Ix... index) {
        static_assert(sizeof...(Ix) == Dims || Dynamic,
                "Invalid number of indices for unchecked array reference");
        return const_cast<T &>(ConstBase::operator()(index...));
    }
    /**
     * Mutable, unchecked access data at the given index; this operator only participates if the
     * reference is to a 1-dimensional array (or has runtime dimensions).  When present, this is
     * exactly equivalent to `obj(index)`.
     */
    template <ssize_t D = Dims, typename = enable_if_t<D == 1 || Dynamic>>
    T &operator[](ssize_t index) { return operator()(index); }

    /// Mutable pointer access to the data at the given indices.
    template <typename... Ix> T *mutable_data(Ix... ix) { return &operator()(ssize_t(ix)...); }
};

template <typename T, ssize_t Dim>
struct type_caster<unchecked_reference<T, Dim>> {
    static_assert(Dim == 0 && Dim > 0 /* always fail */, "unchecked array proxy object is not castable");
};
template <typename T, ssize_t Dim>
struct type_caster<unchecked_mutable_reference<T, Dim>> : type_caster<unchecked_reference<T, Dim>> {};

NAMESPACE_END(detail)

class dtype : public object {
public:
    PYBIND11_OBJECT_DEFAULT(dtype, object, detail::npy_api::get().PyArrayDescr_Check_);

    explicit dtype(const buffer_info &info) {
        dtype descr(_dtype_from_pep3118()(PYBIND11_STR_TYPE(info.format)));
        // If info.itemsize == 0, use the value calculated from the format string
        m_ptr = descr.strip_padding(info.itemsize ? info.itemsize : descr.itemsize()).release().ptr();
    }

    explicit dtype(const std::string &format) {
        m_ptr = from_args(pybind11::str(format)).release().ptr();
    }

    dtype(const char *format) : dtype(std::string(format)) { }

    dtype(list names, list formats, list offsets, ssize_t itemsize) {
        dict args;
        args["names"] = names;
        args["formats"] = formats;
        args["offsets"] = offsets;
        args["itemsize"] = pybind11::int_(itemsize);
        m_ptr = from_args(args).release().ptr();
    }

    /// This is essentially the same as calling numpy.dtype(args) in Python.
    static dtype from_args(object args) {
        PyObject *ptr = nullptr;
        if (!detail::npy_api::get().PyArray_DescrConverter_(args.release().ptr(), &ptr) || !ptr)
            throw error_already_set();
        return reinterpret_steal<dtype>(ptr);
    }

    /// Return dtype associated with a C++ type.
    template <typename T> static dtype of() {
        return detail::npy_format_descriptor<typename std::remove_cv<T>::type>::dtype();
    }

    /// Size of the data type in bytes.
    ssize_t itemsize() const {
        return detail::array_descriptor_proxy(m_ptr)->elsize;
    }

    /// Returns true for structured data types.
    bool has_fields() const {
        return detail::array_descriptor_proxy(m_ptr)->names != nullptr;
    }

    /// Single-character type code.
    char kind() const {
        return detail::array_descriptor_proxy(m_ptr)->kind;
    }

private:
    static object _dtype_from_pep3118() {
        static PyObject *obj = module::import("numpy.core._internal")
            .attr("_dtype_from_pep3118").cast<object>().release().ptr();
        return reinterpret_borrow<object>(obj);
    }

    dtype strip_padding(ssize_t itemsize) {
        // Recursively strip all void fields with empty names that are generated for
        // padding fields (as of NumPy v1.11).
        if (!has_fields())
            return *this;

        struct field_descr { PYBIND11_STR_TYPE name; object format; pybind11::int_ offset; };
        std::vector<field_descr> field_descriptors;

        for (auto field : attr("fields").attr("items")()) {
            auto spec = field.cast<tuple>();
            auto name = spec[0].cast<pybind11::str>();
            auto format = spec[1].cast<tuple>()[0].cast<dtype>();
            auto offset = spec[1].cast<tuple>()[1].cast<pybind11::int_>();
            if (!len(name) && format.kind() == 'V')
                continue;
            field_descriptors.push_back({(PYBIND11_STR_TYPE) name, format.strip_padding(format.itemsize()), offset});
        }

        std::sort(field_descriptors.begin(), field_descriptors.end(),
                  [](const field_descr& a, const field_descr& b) {
                      return a.offset.cast<int>() < b.offset.cast<int>();
                  });

        list names, formats, offsets;
        for (auto& descr : field_descriptors) {
            names.append(descr.name);
            formats.append(descr.format);
            offsets.append(descr.offset);
        }
        return dtype(names, formats, offsets, itemsize);
    }
};

class array : public buffer {
public:
    PYBIND11_OBJECT_CVT(array, buffer, detail::npy_api::get().PyArray_Check_, raw_array)

    enum {
        c_style = detail::npy_api::NPY_ARRAY_C_CONTIGUOUS_,
        f_style = detail::npy_api::NPY_ARRAY_F_CONTIGUOUS_,
        forcecast = detail::npy_api::NPY_ARRAY_FORCECAST_
    };

    array() : array({{0}}, static_cast<const double *>(nullptr)) {}

    using ShapeContainer = detail::any_container<ssize_t>;
    using StridesContainer = detail::any_container<ssize_t>;

    // Constructs an array taking shape/strides from arbitrary container types
    array(const pybind11::dtype &dt, ShapeContainer shape, StridesContainer strides,
          const void *ptr = nullptr, handle base = handle()) {

        if (strides->empty())
            *strides = c_strides(*shape, dt.itemsize());

        auto ndim = shape->size();
        if (ndim != strides->size())
            pybind11_fail("NumPy: shape ndim doesn't match strides ndim");
        auto descr = dt;

        int flags = 0;
        if (base && ptr) {
            if (isinstance<array>(base))
                /* Copy flags from base (except ownership bit) */
                flags = reinterpret_borrow<array>(base).flags() & ~detail::npy_api::NPY_ARRAY_OWNDATA_;
            else
                /* Writable by default, easy to downgrade later on if needed */
                flags = detail::npy_api::NPY_ARRAY_WRITEABLE_;
        }

        auto &api = detail::npy_api::get();
        auto tmp = reinterpret_steal<object>(api.PyArray_NewFromDescr_(
            api.PyArray_Type_, descr.release().ptr(), (int) ndim, shape->data(), strides->data(),
            const_cast<void *>(ptr), flags, nullptr));
        if (!tmp)
            throw error_already_set();
        if (ptr) {
            if (base) {
                api.PyArray_SetBaseObject_(tmp.ptr(), base.inc_ref().ptr());
            } else {
                tmp = reinterpret_steal<object>(api.PyArray_NewCopy_(tmp.ptr(), -1 /* any order */));
            }
        }
        m_ptr = tmp.release().ptr();
    }

    array(const pybind11::dtype &dt, ShapeContainer shape, const void *ptr = nullptr, handle base = handle())
        : array(dt, std::move(shape), {}, ptr, base) { }

    template <typename T, typename = detail::enable_if_t<std::is_integral<T>::value && !std::is_same<bool, T>::value>>
    array(const pybind11::dtype &dt, T count, const void *ptr = nullptr, handle base = handle())
        : array(dt, {{count}}, ptr, base) { }

    template <typename T>
    array(ShapeContainer shape, StridesContainer strides, const T *ptr, handle base = handle())
        : array(pybind11::dtype::of<T>(), std::move(shape), std::move(strides), ptr, base) { }

    template <typename T>
    array(ShapeContainer shape, const T *ptr, handle base = handle())
        : array(std::move(shape), {}, ptr, base) { }

    template <typename T>
    explicit array(ssize_t count, const T *ptr, handle base = handle()) : array({count}, {}, ptr, base) { }

    explicit array(const buffer_info &info)
    : array(pybind11::dtype(info), info.shape, info.strides, info.ptr) { }

    /// Array descriptor (dtype)
    pybind11::dtype dtype() const {
        return reinterpret_borrow<pybind11::dtype>(detail::array_proxy(m_ptr)->descr);
    }

    /// Total number of elements
    ssize_t size() const {
        return std::accumulate(shape(), shape() + ndim(), (ssize_t) 1, std::multiplies<ssize_t>());
    }

    /// Byte size of a single element
    ssize_t itemsize() const {
        return detail::array_descriptor_proxy(detail::array_proxy(m_ptr)->descr)->elsize;
    }

    /// Total number of bytes
    ssize_t nbytes() const {
        return size() * itemsize();
    }

    /// Number of dimensions
    ssize_t ndim() const {
        return detail::array_proxy(m_ptr)->nd;
    }

    /// Base object
    object base() const {
        return reinterpret_borrow<object>(detail::array_proxy(m_ptr)->base);
    }

    /// Dimensions of the array
    const ssize_t* shape() const {
        return detail::array_proxy(m_ptr)->dimensions;
    }

    /// Dimension along a given axis
    ssize_t shape(ssize_t dim) const {
        if (dim >= ndim())
            fail_dim_check(dim, "invalid axis");
        return shape()[dim];
    }

    /// Strides of the array
    const ssize_t* strides() const {
        return detail::array_proxy(m_ptr)->strides;
    }

    /// Stride along a given axis
    ssize_t strides(ssize_t dim) const {
        if (dim >= ndim())
            fail_dim_check(dim, "invalid axis");
        return strides()[dim];
    }

    /// Return the NumPy array flags
    int flags() const {
        return detail::array_proxy(m_ptr)->flags;
    }

    /// If set, the array is writeable (otherwise the buffer is read-only)
    bool writeable() const {
        return detail::check_flags(m_ptr, detail::npy_api::NPY_ARRAY_WRITEABLE_);
    }

    /// If set, the array owns the data (will be freed when the array is deleted)
    bool owndata() const {
        return detail::check_flags(m_ptr, detail::npy_api::NPY_ARRAY_OWNDATA_);
    }

    /// Pointer to the contained data. If index is not provided, points to the
    /// beginning of the buffer. May throw if the index would lead to out of bounds access.
    template<typename... Ix> const void* data(Ix... index) const {
        return static_cast<const void *>(detail::array_proxy(m_ptr)->data + offset_at(index...));
    }

    /// Mutable pointer to the contained data. If index is not provided, points to the
    /// beginning of the buffer. May throw if the index would lead to out of bounds access.
    /// May throw if the array is not writeable.
    template<typename... Ix> void* mutable_data(Ix... index) {
        check_writeable();
        return static_cast<void *>(detail::array_proxy(m_ptr)->data + offset_at(index...));
    }

    /// Byte offset from beginning of the array to a given index (full or partial).
    /// May throw if the index would lead to out of bounds access.
    template<typename... Ix> ssize_t offset_at(Ix... index) const {
        if ((ssize_t) sizeof...(index) > ndim())
            fail_dim_check(sizeof...(index), "too many indices for an array");
        return byte_offset(ssize_t(index)...);
    }

    ssize_t offset_at() const { return 0; }

    /// Item count from beginning of the array to a given index (full or partial).
    /// May throw if the index would lead to out of bounds access.
    template<typename... Ix> ssize_t index_at(Ix... index) const {
        return offset_at(index...) / itemsize();
    }

    /**
     * Returns a proxy object that provides access to the array's data without bounds or
     * dimensionality checking.  Will throw if the array is missing the `writeable` flag.  Use with
     * care: the array must not be destroyed or reshaped for the duration of the returned object,
     * and the caller must take care not to access invalid dimensions or dimension indices.
     */
    template <typename T, ssize_t Dims = -1> detail::unchecked_mutable_reference<T, Dims> mutable_unchecked() {
        if (Dims >= 0 && ndim() != Dims)
            throw std::domain_error("array has incorrect number of dimensions: " + std::to_string(ndim()) +
                    "; expected " + std::to_string(Dims));
        return detail::unchecked_mutable_reference<T, Dims>(mutable_data(), shape(), strides(), ndim());
    }

    /**
     * Returns a proxy object that provides const access to the array's data without bounds or
     * dimensionality checking.  Unlike `mutable_unchecked()`, this does not require that the
     * underlying array have the `writable` flag.  Use with care: the array must not be destroyed or
     * reshaped for the duration of the returned object, and the caller must take care not to access
     * invalid dimensions or dimension indices.
     */
    template <typename T, ssize_t Dims = -1> detail::unchecked_reference<T, Dims> unchecked() const {
        if (Dims >= 0 && ndim() != Dims)
            throw std::domain_error("array has incorrect number of dimensions: " + std::to_string(ndim()) +
                    "; expected " + std::to_string(Dims));
        return detail::unchecked_reference<T, Dims>(data(), shape(), strides(), ndim());
    }

    /// Return a new view with all of the dimensions of length 1 removed
    array squeeze() {
        auto& api = detail::npy_api::get();
        return reinterpret_steal<array>(api.PyArray_Squeeze_(m_ptr));
    }

    /// Resize array to given shape
    /// If refcheck is true and more that one reference exist to this array
    /// then resize will succeed only if it makes a reshape, i.e. original size doesn't change
    void resize(ShapeContainer new_shape, bool refcheck = true) {
        detail::npy_api::PyArray_Dims d = {
            new_shape->data(), int(new_shape->size())
        };
        // try to resize, set ordering param to -1 cause it's not used anyway
        object new_array = reinterpret_steal<object>(
            detail::npy_api::get().PyArray_Resize_(m_ptr, &d, int(refcheck), -1)
        );
        if (!new_array) throw error_already_set();
        if (isinstance<array>(new_array)) { *this = std::move(new_array); }
    }

    /// Ensure that the argument is a NumPy array
    /// In case of an error, nullptr is returned and the Python error is cleared.
    static array ensure(handle h, int ExtraFlags = 0) {
        auto result = reinterpret_steal<array>(raw_array(h.ptr(), ExtraFlags));
        if (!result)
            PyErr_Clear();
        return result;
    }

protected:
    template<typename, typename> friend struct detail::npy_format_descriptor;

    void fail_dim_check(ssize_t dim, const std::string& msg) const {
        throw index_error(msg + ": " + std::to_string(dim) +
                          " (ndim = " + std::to_string(ndim()) + ")");
    }

    template<typename... Ix> ssize_t byte_offset(Ix... index) const {
        check_dimensions(index...);
        return detail::byte_offset_unsafe(strides(), ssize_t(index)...);
    }

    void check_writeable() const {
        if (!writeable())
            throw std::domain_error("array is not writeable");
    }

    // Default, C-style strides
    static std::vector<ssize_t> c_strides(const std::vector<ssize_t> &shape, ssize_t itemsize) {
        auto ndim = shape.size();
        std::vector<ssize_t> strides(ndim, itemsize);
        for (size_t i = ndim - 1; i > 0; --i)
            strides[i - 1] = strides[i] * shape[i];
        return strides;
    }

    // F-style strides; default when constructing an array_t with `ExtraFlags & f_style`
    static std::vector<ssize_t> f_strides(const std::vector<ssize_t> &shape, ssize_t itemsize) {
        auto ndim = shape.size();
        std::vector<ssize_t> strides(ndim, itemsize);
        for (size_t i = 1; i < ndim; ++i)
            strides[i] = strides[i - 1] * shape[i - 1];
        return strides;
    }

    template<typename... Ix> void check_dimensions(Ix... index) const {
        check_dimensions_impl(ssize_t(0), shape(), ssize_t(index)...);
    }

    void check_dimensions_impl(ssize_t, const ssize_t*) const { }

    template<typename... Ix> void check_dimensions_impl(ssize_t axis, const ssize_t* shape, ssize_t i, Ix... index) const {
        if (i >= *shape) {
            throw index_error(std::string("index ") + std::to_string(i) +
                              " is out of bounds for axis " + std::to_string(axis) +
                              " with size " + std::to_string(*shape));
        }
        check_dimensions_impl(axis + 1, shape + 1, index...);
    }

    /// Create array from any object -- always returns a new reference
    static PyObject *raw_array(PyObject *ptr, int ExtraFlags = 0) {
        if (ptr == nullptr) {
            PyErr_SetString(PyExc_ValueError, "cannot create a pybind11::array from a nullptr");
            return nullptr;
        }
        return detail::npy_api::get().PyArray_FromAny_(
            ptr, nullptr, 0, 0, detail::npy_api::NPY_ARRAY_ENSUREARRAY_ | ExtraFlags, nullptr);
    }
};

template <typename T, int ExtraFlags = array::forcecast> class array_t : public array {
private:
    struct private_ctor {};
    // Delegating constructor needed when both moving and accessing in the same constructor
    array_t(private_ctor, ShapeContainer &&shape, StridesContainer &&strides, const T *ptr, handle base)
        : array(std::move(shape), std::move(strides), ptr, base) {}
public:
    static_assert(!detail::array_info<T>::is_array, "Array types cannot be used with array_t");

    using value_type = T;

    array_t() : array(0, static_cast<const