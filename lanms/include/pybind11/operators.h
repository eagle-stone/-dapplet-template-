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
    op_add, op_sub, op_mul, op_div, op_mod, op_divmod, op_pow, op_lshift,
    op_rshift, op_and, op_xor, op_or, op_neg, op_pos, op_abs, op_invert,
    op_int, op_long, op_float, op_str, op_cmp, op_gt, op_ge, op_lt, op_le,
    op_eq, op_ne, op_iadd, op_isub, op_imul, op_idiv, op_imod, op_ilshift,
    op_irshift, op_iand, op_ixor, op_ior, op_complex, op_bool, op_nonzero,
    op_repr, op_truediv, op_itruediv
};

enum op_type : int {
    op_l, /* base type on left */
    op_r, /* base type on right */
    op_u  /* unary operator */
};

struct self_t { };
static const self_t self = self_t();

/// Type for an unused type slot
struct undefined_t { };

/// Don't warn about an unused variable
inline self_t __self() { return self; }

/// base template of operator implementations
template <op_id, op_type, typename B, typename L, typename R> struct op_impl { };

/// Operator implementation generator
template <op_id id, op_type ot, typename L, typename R> struct op_ {
    template <typename Class, t