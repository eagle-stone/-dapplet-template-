
/*
    pybind11/std_bind.h: Binding generators for STL data types

    Copyright (c) 2016 Sergey Lyskov and Wenzel Jakob

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include "common.h"
#include "operators.h"

#include <algorithm>
#include <sstream>

NAMESPACE_BEGIN(pybind11)
NAMESPACE_BEGIN(detail)

/* SFINAE helper class used by 'is_comparable */
template <typename T>  struct container_traits {
    template <typename T2> static std::true_type test_comparable(decltype(std::declval<const T2 &>() == std::declval<const T2 &>())*);
    template <typename T2> static std::false_type test_comparable(...);
    template <typename T2> static std::true_type test_value(typename T2::value_type *);
    template <typename T2> static std::false_type test_value(...);
    template <typename T2> static std::true_type test_pair(typename T2::first_type *, typename T2::second_type *);
    template <typename T2> static std::false_type test_pair(...);

    static constexpr const bool is_comparable = std::is_same<std::true_type, decltype(test_comparable<T>(nullptr))>::value;
    static constexpr const bool is_pair = std::is_same<std::true_type, decltype(test_pair<T>(nullptr, nullptr))>::value;
    static constexpr const bool is_vector = std::is_same<std::true_type, decltype(test_value<T>(nullptr))>::value;
    static constexpr const bool is_element = !is_pair && !is_vector;
};

/* Default: is_comparable -> std::false_type */
template <typename T, typename SFINAE = void>
struct is_comparable : std::false_type { };

/* For non-map data structures, check whether operator== can be instantiated */
template <typename T>
struct is_comparable<
    T, enable_if_t<container_traits<T>::is_element &&
                   container_traits<T>::is_comparable>>
    : std::true_type { };

/* For a vector/map data structure, recursively check the value type (which is std::pair for maps) */
template <typename T>
struct is_comparable<T, enable_if_t<container_traits<T>::is_vector>> {
    static constexpr const bool value =
        is_comparable<typename T::value_type>::value;
};

/* For pairs, recursively check the two data types */
template <typename T>
struct is_comparable<T, enable_if_t<container_traits<T>::is_pair>> {
    static constexpr const bool value =
        is_comparable<typename T::first_type>::value &&
        is_comparable<typename T::second_type>::value;
};

/* Fallback functions */
template <typename, typename, typename... Args> void vector_if_copy_constructible(const Args &...) { }
template <typename, typename, typename... Args> void vector_if_equal_operator(const Args &...) { }
template <typename, typename, typename... Args> void vector_if_insertion_operator(const Args &...) { }
template <typename, typename, typename... Args> void vector_modifiers(const Args &...) { }

template<typename Vector, typename Class_>
void vector_if_copy_constructible(enable_if_t<is_copy_constructible<Vector>::value, Class_> &cl) {
    cl.def(init<const Vector &>(), "Copy constructor");
}

template<typename Vector, typename Class_>
void vector_if_equal_operator(enable_if_t<is_comparable<Vector>::value, Class_> &cl) {
    using T = typename Vector::value_type;

    cl.def(self == self);
    cl.def(self != self);

    cl.def("count",
        [](const Vector &v, const T &x) {
            return std::count(v.begin(), v.end(), x);
        },
        arg("x"),
        "Return the number of times ``x`` appears in the list"
    );

    cl.def("remove", [](Vector &v, const T &x) {
            auto p = std::find(v.begin(), v.end(), x);
            if (p != v.end())
                v.erase(p);
            else
                throw value_error();
        },
        arg("x"),
        "Remove the first item from the list whose value is x. "
        "It is an error if there is no such item."
    );

    cl.def("__contains__",
        [](const Vector &v, const T &x) {
            return std::find(v.begin(), v.end(), x) != v.end();
        },
        arg("x"),
        "Return true the container contains ``x``"
    );
}

// Vector modifiers -- requires a copyable vector_type:
// (Technically, some of these (pop and __delitem__) don't actually require copyability, but it seems
// silly to allow deletion but not insertion, so include them here too.)
template <typename Vector, typename Class_>
void vector_modifiers(enable_if_t<is_copy_constructible<typename Vector::value_type>::value, Class_> &cl) {
    using T = typename Vector::value_type;
    using SizeType = typename Vector::size_type;
    using DiffType = typename Vector::difference_type;

    cl.def("append",
           [](Vector &v, const T &value) { v.push_back(value); },
           arg("x"),
           "Add an item to the end of the list");

    cl.def("__init__", [](Vector &v, iterable it) {
        new (&v) Vector();
        try {
            v.reserve(len(it));
            for (handle h : it)
               v.push_back(h.cast<T>());
        } catch (...) {
            v.~Vector();
            throw;
        }
    });

    cl.def("extend",
       [](Vector &v, const Vector &src) {
           v.insert(v.end(), src.begin(), src.end());
       },
       arg("L"),
       "Extend the list by appending all the items in the given list"
    );

    cl.def("insert",
        [](Vector &v, SizeType i, const T &x) {
            if (i > v.size())
                throw index_error();
            v.insert(v.begin() + (DiffType) i, x);
        },
        arg("i") , arg("x"),
        "Insert an item at a given position."
    );

    cl.def("pop",
        [](Vector &v) {
            if (v.empty())
                throw index_error();
            T t = v.back();
            v.pop_back();
            return t;
        },
        "Remove and return the last item"
    );

    cl.def("pop",
        [](Vector &v, SizeType i) {
            if (i >= v.size())
                throw index_error();
            T t = v[i];
            v.erase(v.begin() + (DiffType) i);
            return t;
        },
        arg("i"),
        "Remove and return the item at index ``i``"
    );

    cl.def("__setitem__",
        [](Vector &v, SizeType i, const T &t) {
            if (i >= v.size())
                throw index_error();
            v[i] = t;
        }
    );

    /// Slicing protocol
    cl.def("__getitem__",
        [](const Vector &v, slice slice) -> Vector * {
            size_t start, stop, step, slicelength;

            if (!slice.compute(v.size(), &start, &stop, &step, &slicelength))
                throw error_already_set();

            Vector *seq = new Vector();
            seq->reserve((size_t) slicelength);

            for (size_t i=0; i<slicelength; ++i) {
                seq->push_back(v[start]);
                start += step;
            }
            return seq;
        },
        arg("s"),
        "Retrieve list elements using a slice object"
    );

    cl.def("__setitem__",
        [](Vector &v, slice slice,  const Vector &value) {
            size_t start, stop, step, slicelength;
            if (!slice.compute(v.size(), &start, &stop, &step, &slicelength))
                throw error_already_set();

            if (slicelength != value.size())
                throw std::runtime_error("Left and right hand size of slice assignment have different sizes!");

            for (size_t i=0; i<slicelength; ++i) {
                v[start] = value[i];
                start += step;
            }
        },
        "Assign list elements using a slice object"
    );

    cl.def("__delitem__",
        [](Vector &v, SizeType i) {
            if (i >= v.size())
                throw index_error();
            v.erase(v.begin() + DiffType(i));
        },
        "Delete the list elements at index ``i``"
    );

    cl.def("__delitem__",
        [](Vector &v, slice slice) {
            size_t start, stop, step, slicelength;

            if (!slice.compute(v.size(), &start, &stop, &step, &slicelength))
                throw error_already_set();

            if (step == 1 && false) {
                v.erase(v.begin() + (DiffType) start, v.begin() + DiffType(start + slicelength));
            } else {
                for (size_t i = 0; i < slicelength; ++i) {
                    v.erase(v.begin() + DiffType(start));
                    start += step - 1;
                }
            }
        },
        "Delete list elements using a slice object"
    );