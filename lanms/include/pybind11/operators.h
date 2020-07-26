/*
    pybind11/operator.h: Metatemplates for operator overloading

    Copyright (c) 2016 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include "pybind11.h"

#if defined(__clang__) && !defined(__INTEL_COMPILER)
#  pragma clang diagnostic ignored "-Wunsequenced" // multiple unsequenced modifications to 'self' (when using def(py::self OP Type()))
#elif defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4127) // warning C4127: Conditional expression is constant
#endif

NAMESPACE_BEGIN(pybind11)
NAMESPACE_BEGIN(detail)

/// Enumeration with all supported operator types
enum op_id : int {
    o