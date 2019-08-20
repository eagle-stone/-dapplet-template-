#include "pybind11/pybind11.h"
#include "pybind11/numpy.h"
#include "pybind11/stl.h"
#include "pybind11/stl_bind.h"

#include "lanms.h"

namespace py = pybind11;


namespace lanms_adaptor {

	std::vector<std::vector<float>> polys2floats(const std::vector<lanms::Polygon> &polys) {
		std::vector<std::vector<float>> ret;
		for (size_t i = 0; i < polys.size(); i ++) {
			auto &p = polys[i];
			auto &poly = p.poly;
			ret.emplace_back(std::vector<float>{
					float(poly[0].X), flo