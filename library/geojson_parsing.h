#pragma once

#include <cartocrow/core/region_map.h>

namespace cartocrow::chorematic_map {
using RegionWeight = std::unordered_map<std::string, double>;

std::pair<std::shared_ptr<RegionMap>, std::shared_ptr<std::unordered_map<std::string, RegionWeight>>>
parseGeoJSON(std::string geojson, std::string regionNameAttribute);
}