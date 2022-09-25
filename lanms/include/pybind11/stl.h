/*
    pybind11/stl.h: Transparent conversion for STL data types

    Copyright (c) 2016 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include "pybind11.h"
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <iostream>
#include <list>
#include <valarray>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4127) // warning C4127: Conditional expression is constant
#endif

#ifdef __has_include
// std::optional (but including it in c++14 mode isn't allowed)
#  if defined(PYBIND11_CPP17) && __has_include(<optional>)
#    include <optional>
#    define PYBIND11_HAS_OPTIONAL 1
#  endif
// std::experimental::optional (but not allowed in c++11 mode)
#  if defined(PYBIND11_CPP14) && __has_include(<experimental/optional>)
#    include <experimental/optional>
#    define PYBIND11_HAS_EXP_OPTIONAL 1
#  endif
// std::variant
#  if defined(PYBIND11_CPP17) && __has_include(<variant>)
#    include <variant>
#    define PYBIND11_HAS_VARIANT 1
#  endif
#elif defined(_MSC_VER) && defined(PYBIND11_CPP17)
#  include <optional>
#  include <variant>
#  define PYBIND11_HAS_OPTIONAL 1
#  define PYBIND11_HAS_VARIANT 1
#endif

NAMESPACE_BEGIN(pybind11)
NAMESPACE_BEGIN(detail)

/// Extracts an const lvalue reference or rvalue reference for U based on the type of T (e.g. for
/// forwarding a container element).  Typically used indirect via forwarded_type(), below.
template <typename T, typename U>
using forwarded_type = conditional_t<
    std::is_lvalue_reference<T>::value, remove_reference_t<U> &, remove_reference_t<U> &&>;

/// Forwards a value U as rvalue or lvalue according to whether T is rvalue or lvalue; typically
/// used for forwarding a container's elements.
template <typename T, typename U>
forwarded_type<T, U> forward_like(U &&u) {
    return std::forward<detail::forwarded_type<T, U>>(std::forward<U>(u));
}

template <typename Type, typename Key> struct set_caster {
    using type = Type;
    using key_conv = make_caster<Key>;

    bool load(handle src, bool convert) {
        if (!isinstance<pybind11::set>(src))
            return false;
        auto s = reinterpret_borrow<pybind11::set>(src);
        value.clear();
        for (auto entry : s) {
            key_conv conv;
            if (!conv.load(entry, convert))
                return false;
            value.insert(cast_op<Key &&>(std::move(conv)));
        }
        return true;
    }

    template <typename T>
    static handle cast(T &&src, return_value_policy policy, handle parent) {
        pybind11::set s;
        for (auto &value: src) {
            auto value_ = reinterpret_steal<object>(key_conv::cast(forward_like<T>(value), policy, parent));
            if (!value_ || !s.add(value_))
                return handle();
        }
        return s.release();
    }

    PYBIND11_TYPE_CASTER(type, _("Set[") + key_conv::name() + _("]"));
};

template <typename Type, typename Key, typename Value> struct map_caster {
    using key_conv   = make_caster<Key>;
    using value_conv = make_caster<Value>;

    bool load(handle src, bool convert) {
        if (!isinstance<dict>(src))
            return false;
        auto d = reinterpret_borrow<dict>(src);
        value.clear();
        for (auto it : d) {
            key_conv kconv;
            value_conv vconv;
            if (!kconv.load(it.first.ptr(), convert) ||
                !vconv.load(it.second.ptr(), convert))
                return false;
            value.emplace(cast_op<Key &&>(std::move(kconv)), cast_op<Value &&>(std::move(vconv)));
        }
        return true;
    }

    template <typename T>
    static handle cast(T &&src, return_value_policy poli