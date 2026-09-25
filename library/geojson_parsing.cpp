#include "geojson_parsing.h"

#include <cartocrow/reader/geojson_reader.h>

namespace cartocrow::chorematic_map {
std::pair<std::shared_ptr<RegionMap>, std::shared_ptr<std::unordered_map<std::string, RegionWeight>>>
parseGeoJSON(std::string geojson, std::string regionNameAttribute) {
    auto regionMap = std::make_shared<RegionMap>();
    auto weightMap = std::make_shared<std::unordered_map<std::string, RegionWeight>>();

    GeoJSONReader reader(geojson);
    auto features = reader.read<Multiple, PolygonSetRaw<Inexact>, WithAttributes>();

    auto getName = [regionNameAttribute](const GeometricFeature<PolygonSetRaw<Inexact>>& feature) -> std::optional<std::string> {
        if (!feature.attributes.contains(regionNameAttribute)) {
            std::cerr << "Feature does not have attribute " << regionNameAttribute << std::endl;
            return std::nullopt;
        }

        auto nameAttr = feature.attributes.at(regionNameAttribute);
        std::string name;
        if (std::holds_alternative<std::string>(nameAttr)) {
            return std::get<std::string>(nameAttr);
        } else if(std::holds_alternative<int>(nameAttr)) {
            return std::to_string(std::get<int>(nameAttr));
        } else {
            std::cerr << "Attribute " << std::get<std::string>(nameAttr) << " is not a string or integer!" << std::endl;
            return std::nullopt;
        }
    };

    for (const auto& feature : features) {
        auto name = getName(feature);
        if (!name.has_value()) continue;
        (*regionMap)[*name] = Region(*name, Color{0, 0, 0}, pretendExact(feature.geometry).polygonSet());
    }

    // we assume features have the same attributes
    std::vector<std::string> keys;
    if (features.size() > 0) {
        for (const auto &[key, value]: features[0].attributes) {
            if (convertible_to_double(value)) {
                keys.push_back(key);
            }
        }
    }

    for (const auto& key : keys) {
        auto& weights = (*weightMap)[key];
        for (const auto& f : features) {
            auto name = getName(f);
            if (!name.has_value()) continue;
            if (!f.attributes.contains(key)) continue;
            auto value = f.attributes.at(key);
            if (!convertible_to_double(value)) {
                std::cerr << "Attribute " << key << " of feature " << *name << " is not convertible to double!" << std::endl;
            }
            weights[*name] = to_double(value);
        }
    }

    return {regionMap, weightMap};
}
}