/*
    pybind11/descr.h: Helper type for concatenating type signatures
    either at runtime (C++11) or compile time (C++14)

    Copyright (c) 2016 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include "common.h"

NAMESPACE_BEGIN(pybind11)
NAMESPACE_BEGIN(detail)

/* Concatenate type signatures at compile time using C++14 */
#if defined(PYBIND11_CPP14) && !defined(_MSC_VER)
#define PYBIND11_CONSTEXPR_DESCR

template <size_t Size1, size_t Size2> class descr {
    template <size_t Size1_, size_t Size2_> friend class descr;
public:
    constexpr descr(char const (&text) [Size1+1], const std::type_info * const (&types)[Size2+1])
        : descr(text, types,
                make_index_sequence<Size1>(),
                make_index_sequence<Size2>()) { }

    constexpr const char *text() const { return m_text; }
    constexpr const std::type_info * const * types() const { return m_types; }

    template <size_t OtherSize1, size_t OtherSize2>
    constexpr descr<Size1 + OtherSize1, Size2 + OtherSize2> operator+(const descr<OtherSize1, OtherSize2> &other) const {
        return concat(other,
                      make_index_sequence<Size1>(),
                      make_index_sequence<Size2>(),
                      make_index_sequence<OtherSize1>(),
                      make_index_sequence<OtherSize2>());
    }

protected:
    template <size_t... Indices1, size_t... Indices2>
    constexpr descr(
        char const (&text) [Size1+1],
        const std::type_info * const (&types) [Size2+1],
        index_sequence<Indices1...>, index_sequence<Indices2...>)
        : m_text{text[Indices1]..., '\0'},
          m_types{types[Indices2]...,  nullptr } {}

    template <size_t OtherSize1, size_t OtherSize2, size_t... Indices1,
              size_t... Indices2, size_t... OtherIndices1, size_t... OtherIndices2>
    constexpr descr<Size1 + OtherSiz