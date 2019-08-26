/*
    pybind11/buffer_info.h: Python buffer object interface

    Copyright (c) 2016 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include "common.h"

NAMESPACE_BEGIN(pybind11)

/// Information record describing a Python buffer object
struct buffer_info {
    void *ptr = nullptr;          // Pointer to the underlying storage
    ssize_t itemsize = 0;         // Size of individual items in bytes
    ssize_t size = 0;             // Total number of entries
    std::string format;           // For homogeneous buffers, this should be set to format_descriptor<T>::format()
    ssize_t ndim = 0;             // Number of dimensions
    std::vector<ssize_t> shape;   // Shape of the tensor (1 entry per dimension)
    std::vector<ssize_t> strides; // Number of entries between adjacent entries (for each per dimension)

    buffer_info() { }

    buffer_info(void *ptr, ssize_t itemsize, const std::string &format, ssize_t ndim,
                detail::any_container<ssize_t> shape_in, detail::any_container<ssize_t> strides_in)
    : ptr(ptr), itemsize(itemsize), size(1), format(format), ndim(ndim),
      shape(std::move(shape_in)), strides(std::move(strides_in)) {
        if (ndim != (ssize_t) shape.size() || ndim != (ssize_t) strides.size())
            pybind11_fail("buffer_info: ndim doesn't match shape and/or strides length");
        for (size_t i = 0; i < (size_t) ndim; ++i)
            size *= shape[i];
    }

    template <typename T>
    buffer_info(T *ptr, detail::any_container<ssize_t> shape_in, detail::any_container<ssize_t> strides_in)
    : buffer_info(private_ctr_tag(), ptr, sizeof(T), format_descriptor<T>::format(), static_cast<ssize_t>(shape_in->size()), std::move(shape_in), std::move(strides_in)) { }

    buffer_info(void *ptr, ssize_t itemsize, const std::string &format, ssize_t size)
    : buffer_info(ptr, itemsize, format, 1, {size}, {itemsize}) { }

    template <typename T>
    buffer_info(T *ptr, ssize_t size)
    : buffer_info(ptr, sizeof(T), format_descriptor<T>::format(), size) { }

    explicit buffer_info(Py_buffer *view, bool ownview = true)
    : buffer_info(view->buf, view->itemsize, view->format, view->ndim,
            {view->shape, view->shape + view->ndim}, {view->strides, view->strides + view->ndim}) {
        this->view = view;
        this->ownview = ownview;
    }

    buffer_info(const buffer_info &) = delete;
    buffer_info& operator=(const buffer_info &) = delete;

    buffer_info(buffer_info &&other) {
        (*this) = st