/*
    pybind11/operator.h: Metatemplates for operator overloading

    Copyright (c) 2016 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
*/

#pragma once

#include "pybind11.h"

#if defined(__clang__) && !defined(__INTEL_COMPILER)
#  pragma clang diagno