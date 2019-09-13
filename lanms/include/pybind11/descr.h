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
#if defined(PYBI