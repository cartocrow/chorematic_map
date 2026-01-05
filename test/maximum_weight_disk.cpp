#include "catch.hpp"

#include "library/maximum_weight_disk.h"
#include "library/parse_points.h"

namespace cartocrow::chorematic_map {
TEST_CASE("Smallest maximum weight disk") {
	auto pointSets = readPointsFromIpe("data/maximum_weight_disk_tests.ipe");
	auto disks = readDisksFromIpe("data/maximum_weight_disk_tests.ipe");
	if (pointSets.size() != disks.size()) {
		throw std::runtime_error("Impossible: there should be a one point set and one disk per page in the ipe file.");
	}
	for (int i = 0; i < pointSets.size(); ++i) {
		auto& points = pointSets[i];
		auto [p1, p2, p3] = disks[i];
		auto [wp1, wp2, wp3] = smallest_maximum_weight_disk(points.begin(), points.end());
		// Testing page i. (If I make SECTIONs then each run all points are reloaded...)
//		SECTION("Page " + std::to_string(i)) {
			if (!wp1.has_value()) {
				CHECK(!p1.has_value());
				continue;
			}
			CHECK(p1.has_value());
			if (!wp2.has_value()) {
				CHECK(!p2.has_value());
				CHECK(wp1->point == p1);
				continue;
			}
			CHECK(p2.has_value());
			if (!wp3.has_value()) {
				CHECK(!p3.has_value());
				CHECK((wp1->point == p1 && wp2->point == p2 || wp1->point == p2 && wp2->point == p1));
				continue;
			}
			CHECK(p3.has_value());
			std::vector<Point<Inexact>> expected({*p1, *p2, *p3});
			std::vector<Point<Inexact>> actual({wp1->point, wp2->point, wp3->point});
			std::sort(expected.begin(), expected.end());
			std::sort(actual.begin(), actual.end());
			CHECK(expected == actual);
//		}
	}
}
}