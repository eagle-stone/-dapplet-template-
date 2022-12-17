
#pragma once

#include "clipper/clipper.hpp"

// locality-aware NMS
namespace lanms {

	namespace cl = ClipperLib;

	struct Polygon {
		cl::Path poly;
		float score;
	};

	float paths_area(const ClipperLib::Paths &ps) {
		float area = 0;
		for (auto &&p: ps)
			area += cl::Area(p);
		return area;
	}

	float poly_iou(const Polygon &a, const Polygon &b) {
		cl::Clipper clpr;
		clpr.AddPath(a.poly, cl::ptSubject, true);
		clpr.AddPath(b.poly, cl::ptClip, true);

		cl::Paths inter, uni;
		clpr.Execute(cl::ctIntersection, inter, cl::pftEvenOdd);
		clpr.Execute(cl::ctUnion, uni, cl::pftEvenOdd);

		auto inter_area = paths_area(inter),
			 uni_area = paths_area(uni);
		return std::abs(inter_area) / std::max(std::abs(uni_area), 1.0f);
	}

	bool should_merge(const Polygon &a, const Polygon &b, float iou_threshold) {
		return poly_iou(a, b) > iou_threshold;
	}

	/**
	 * Incrementally merge polygons
	 */
	class PolyMerger {
		public:
			PolyMerger(): score(0), nr_polys(0) {
				memset(data, 0, sizeof(data));
			}

			/**
			 * Add a new polygon to be merged.
			 */
			void add(const Polygon &p_given) {
				Polygon p;
				if (nr_polys > 0) {
					// vertices of two polygons to merge may not in the same order;
					// we match their vertices by choosing the ordering that
					// minimizes the total squared distance.
					// see function normalize_poly for details.
					p = normalize_poly(get(), p_given);
				} else {
					p = p_given;
				}
				assert(p.poly.size() == 4);
				auto &poly = p.poly;
				auto s = p.score;
				data[0] += poly[0].X * s;
				data[1] += poly[0].Y * s;

				data[2] += poly[1].X * s;
				data[3] += poly[1].Y * s;

				data[4] += poly[2].X * s;
				data[5] += poly[2].Y * s;

				data[6] += poly[3].X * s;
				data[7] += poly[3].Y * s;

				score += p.score;

				nr_polys += 1;
			}

			inline std::int64_t sqr(std::int64_t x) { return x * x; }

			Polygon normalize_poly(
					const Polygon &ref,
					const Polygon &p) {

				std::int64_t min_d = std::numeric_limits<std::int64_t>::max();
				size_t best_start = 0, best_order = 0;

				for (size_t start = 0; start < 4; start ++) {
					size_t j = start;
					std::int64_t d = (
							sqr(ref.poly[(j + 0) % 4].X - p.poly[(j + 0) % 4].X)
							+ sqr(ref.poly[(j + 0) % 4].Y - p.poly[(j + 0) % 4].Y)
							+ sqr(ref.poly[(j + 1) % 4].X - p.poly[(j + 1) % 4].X)
							+ sqr(ref.poly[(j + 1) % 4].Y - p.poly[(j + 1) % 4].Y)
							+ sqr(ref.poly[(j + 2) % 4].X - p.poly[(j + 2) % 4].X)
							+ sqr(ref.poly[(j + 2) % 4].Y - p.poly[(j + 2) % 4].Y)
							+ sqr(ref.poly[(j + 3) % 4].X - p.poly[(j + 3) % 4].X)
							+ sqr(ref.poly[(j + 3) % 4].Y - p.poly[(j + 3) % 4].Y)
							);
					if (d < min_d) {
						min_d = d;
						best_start = start;
						best_order = 0;
					}

					d = (
							sqr(ref.poly[(j + 0) % 4].X - p.poly[(j + 3) % 4].X)
							+ sqr(ref.poly[(j + 0) % 4].Y - p.poly[(j + 3) % 4].Y)
							+ sqr(ref.poly[(j + 1) % 4].X - p.poly[(j + 2) % 4].X)
							+ sqr(ref.poly[(j + 1) % 4].Y - p.poly[(j + 2) % 4].Y)
							+ sqr(ref.poly[(j + 2) % 4].X - p.poly[(j + 1) % 4].X)
							+ sqr(ref.poly[(j + 2) % 4].Y - p.poly[(j + 1) % 4].Y)
							+ sqr(ref.poly[(j + 3) % 4].X - p.poly[(j + 0) % 4].X)
							+ sqr(ref.poly[(j + 3) % 4].Y - p.poly[(j + 0) % 4].Y)
						);
					if (d < min_d) {
						min_d = d;
						best_start = start;
						best_order = 1;
					}
				}

				Polygon r;
				r.poly.resize(4);