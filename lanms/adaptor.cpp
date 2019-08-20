#include "pybind11/pybind11.h"
#include "pybind11/numpy.h"
#include "pybind11/stl.h"
#include "pybind11/stl_bind.h"

#include "lanms.h"

namespace py = pybind11;


namespace lanms_adaptor {

	std::vector<std::vector<float>> polys2floats(const std::vector<lanms::Polygon> &polys) {
		std::vector<std::vector<f