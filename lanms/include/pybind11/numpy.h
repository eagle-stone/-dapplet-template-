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
       