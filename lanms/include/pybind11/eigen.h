
/*
    pybind11/eigen.h: Transparent conversion for dense and sparse Eigen matrices

    Copyright (c) 2016 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include "numpy.h"

#if defined(__INTEL_COMPILER)
#  pragma warning(disable: 1682) // implicit conversion of a 64-bit integral type to a smaller integral type (potential portability problem)
#elif defined(__GNUG__) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#  if __GNUC__ >= 7
#    pragma GCC diagnostic ignored "-Wint-in-bool-context"
#  endif
#endif

#include <Eigen/Core>
#include <Eigen/SparseCore>

#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4127) // warning C4127: Conditional expression is constant
#endif

// Eigen prior to 3.2.7 doesn't have proper move constructors--but worse, some classes get implicit
// move constructors that break things.  We could detect this an explicitly copy, but an extra copy
// of matrices seems highly undesirable.
static_assert(EIGEN_VERSION_AT_LEAST(3,2,7), "Eigen support in pybind11 requires Eigen >= 3.2.7");

NAMESPACE_BEGIN(pybind11)

// Provide a convenience alias for easier pass-by-ref usage with fully dynamic strides:
using EigenDStride = Eigen::Stride<Eigen::Dynamic, Eigen::Dynamic>;
template <typename MatrixType> using EigenDRef = Eigen::Ref<MatrixType, 0, EigenDStride>;
template <typename MatrixType> using EigenDMap = Eigen::Map<MatrixType, 0, EigenDStride>;

NAMESPACE_BEGIN(detail)

#if EIGEN_VERSION_AT_LEAST(3,3,0)
using EigenIndex = Eigen::Index;
#else
using EigenIndex = EIGEN_DEFAULT_DENSE_INDEX_TYPE;
#endif

// Matches Eigen::Map, Eigen::Ref, blocks, etc:
template <typename T> using is_eigen_dense_map = all_of<is_template_base_of<Eigen::DenseBase, T>, std::is_base_of<Eigen::MapBase<T, Eigen::ReadOnlyAccessors>, T>>;
template <typename T> using is_eigen_mutable_map = std::is_base_of<Eigen::MapBase<T, Eigen::WriteAccessors>, T>;
template <typename T> using is_eigen_dense_plain = all_of<negation<is_eigen_dense_map<T>>, is_template_base_of<Eigen::PlainObjectBase, T>>;
template <typename T> using is_eigen_sparse = is_template_base_of<Eigen::SparseMatrixBase, T>;
// Test for objects inheriting from EigenBase<Derived> that aren't captured by the above.  This
// basically covers anything that can be assigned to a dense matrix but that don't have a typical
// matrix data layout that can be copied from their .data().  For example, DiagonalMatrix and
// SelfAdjointView fall into this category.
template <typename T> using is_eigen_other = all_of<
    is_template_base_of<Eigen::EigenBase, T>,
    negation<any_of<is_eigen_dense_map<T>, is_eigen_dense_plain<T>, is_eigen_sparse<T>>>
>;

// Captures numpy/eigen conformability status (returned by EigenProps::conformable()):
template <bool EigenRowMajor> struct EigenConformable {
    bool conformable = false;
    EigenIndex rows = 0, cols = 0;
    EigenDStride stride{0, 0};      // Only valid if negativestrides is false!
    bool negativestrides = false;   // If true, do not use stride!

    EigenConformable(bool fits = false) : conformable{fits} {}
    // Matrix type:
    EigenConformable(EigenIndex r, EigenIndex c,
            EigenIndex rstride, EigenIndex cstride) :
        conformable{true}, rows{r}, cols{c} {
        // TODO: when Eigen bug #747 is fixed, remove the tests for non-negativity. http://eigen.tuxfamily.org/bz/show_bug.cgi?id=747
        if (rstride < 0 || cstride < 0) {
            negativestrides = true;
        } else {
            stride = {EigenRowMajor ? rstride : cstride /* outer stride */,
                      EigenRowMajor ? cstride : rstride /* inner stride */ };
        }
    }
    // Vector type:
    EigenConformable(EigenIndex r, EigenIndex c, EigenIndex stride)
        : EigenConformable(r, c, r == 1 ? c*stride : stride, c == 1 ? r : r*stride) {}

    template <typename props> bool stride_compatible() const {
        // To have compatible strides, we need (on both dimensions) one of fully dynamic strides,
        // matching strides, or a dimension size of 1 (in which case the stride value is irrelevant)
        return
            !negativestrides &&
            (props::inner_stride == Eigen::Dynamic || props::inner_stride == stride.inner() ||
                (EigenRowMajor ? cols : rows) == 1) &&
            (props::outer_stride == Eigen::Dynamic || props::outer_stride == stride.outer() ||
                (EigenRowMajor ? rows : cols) == 1);
    }
    operator bool() const { return conformable; }
};

template <typename Type> struct eigen_extract_stride { using type = Type; };
template <typename PlainObjectType, int MapOptions, typename StrideType>
struct eigen_extract_stride<Eigen::Map<PlainObjectType, MapOptions, StrideType>> { using type = StrideType; };
template <typename PlainObjectType, int Options, typename StrideType>
struct eigen_extract_stride<Eigen::Ref<PlainObjectType, Options, StrideType>> { using type = StrideType; };

// Helper struct for extracting information from an Eigen type
template <typename Type_> struct EigenProps {
    using Type = Type_;
    using Scalar = typename Type::Scalar;
    using StrideType = typename eigen_extract_stride<Type>::type;
    static constexpr EigenIndex
        rows = Type::RowsAtCompileTime,
        cols = Type::ColsAtCompileTime,
        size = Type::SizeAtCompileTime;
    static constexpr bool
        row_major = Type::IsRowMajor,
        vector = Type::IsVectorAtCompileTime, // At least one dimension has fixed size 1
        fixed_rows = rows != Eigen::Dynamic,
        fixed_cols = cols != Eigen::Dynamic,
        fixed = size != Eigen::Dynamic, // Fully-fixed size
        dynamic = !fixed_rows && !fixed_cols; // Fully-dynamic size

    template <EigenIndex i, EigenIndex ifzero> using if_zero = std::integral_constant<EigenIndex, i == 0 ? ifzero : i>;
    static constexpr EigenIndex inner_stride = if_zero<StrideType::InnerStrideAtCompileTime, 1>::value,
                                outer_stride = if_zero<StrideType::OuterStrideAtCompileTime,
                                                       vector ? size : row_major ? cols : rows>::value;
    static constexpr bool dynamic_stride = inner_stride == Eigen::Dynamic && outer_stride == Eigen::Dynamic;
    static constexpr bool requires_row_major = !dynamic_stride && !vector && (row_major ? inner_stride : outer_stride) == 1;
    static constexpr bool requires_col_major = !dynamic_stride && !vector && (row_major ? outer_stride : inner_stride) == 1;

    // Takes an input array and determines whether we can make it fit into the Eigen type.  If