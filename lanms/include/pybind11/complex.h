/*
    pybind11/complex.h: Complex number support

    Copyright (c) 2016 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include "pybind11.h"
#include <complex>

/// glibc defines I as a macro which breaks things, e.g., boost template names
#ifdef I
#  undef I
#endif

NAMESPACE_BEGIN(pybind11)

template <typename T> struct format_descriptor<std::complex<T>, detail::enable_if_t<std::is_floating_point<T>::value>> {
    static constexpr const char c = format_descriptor<T>::c;
    static constexpr const char value[3] = { 'Z', c, '\0' };
    static std::string format() { return std::string(value); }
};

template <typename T> constexpr const char format_descriptor<
    std::complex<T>, detail::enable_if_t<std::is_floating_point<T>::value>>::value[3];

NAMESPACE_BEGIN(detail)

template <typename T> struct is_fmt_numeric<std::complex<T>, detail::enable_if_t<std::is_floating_point<T>::value>> {
    static constexpr bool value = true;
    static constexpr int index = is_fmt_numeric<T>::index + 3;
};

template <typename T> class type_caster<std::complex<T>> {
public:
    bool load(handle src, bool convert) {
        if (!src)
            return false;
        if (!convert && !PyComplex_Check(src.ptr()))
            return false;
        Py