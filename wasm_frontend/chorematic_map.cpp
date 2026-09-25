#include <iostream>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "library/choropleth.h"
#include "library/sampler.h"
#include "library/geojson_parsing.h"

#include <nlohmann/json.hpp>

#include <cartocrow/renderer/svg_renderer.h>

using namespace emscripten;

using namespace cartocrow;
using namespace cartocrow::renderer;
using namespace cartocrow::chorematic_map;

using json = nlohmann::json;

class ChorematicMap {
private:
    std::shared_ptr<RegionArrangement> m_arr;
    std::shared_ptr<std::unordered_map<std::string, RegionWeight>> m_weightMap;
    std::unique_ptr<Sampler> m_sampler;
    std::unique_ptr<Choropleth> m_choropleth = nullptr;

public:
    void loadGeoJSON(std::string geojson, std::string regionNameAttribute) {
        auto [regionMap, regionWeights] = parseGeoJSON(geojson, regionNameAttribute);
        m_arr = std::make_shared<RegionArrangement>(regionMapToArrangement(*regionMap));
        m_weightMap = regionWeights;
//        m_choropleth = std::make_unique<Choropleth>(regionMap, regionWeights, 2);
        m_sampler = std::make_unique<Sampler>(m_arr, 0);
    }

    std::string getOutline() {
        auto polys = m_sampler->getLandmassPolys();

        RenderPath rp;
        for (const auto& poly : polys) {
            rp << approximate(poly);
        }
        return renderPathToSVGCommands(rp);
    }

    void setAttribute(std::string attr) {
        auto weights = std::make_shared<RegionWeight>(m_weightMap->at(attr));
        if (m_choropleth == nullptr) {
            m_choropleth = std::make_unique<Choropleth>(m_arr, weights, 2);
        } else {
            m_choropleth->m_data = weights;
            m_choropleth->naturalBreaks(2);
            m_choropleth->rebin();
        }
    }

    int getBin(std::string regionName) {
        return *m_choropleth->regionToBin(regionName);
    }

    static val parse_json(const std::string& json) {
        return val::global("JSON").call<val>("parse", val(json));
    }

    static val parse_json(const json& json) {
        return parse_json(to_string(json));
    }

    val getWeightMap() {
        json j = *m_weightMap;
        return parse_json(j);
    }
};

EMSCRIPTEN_BINDINGS(doesntmatter) {
    class_<ChorematicMap>("ChorematicMap")
            .constructor<>()
            .function("loadGeoJSON", &ChorematicMap::loadGeoJSON)
            .function("setAttribute", &ChorematicMap::setAttribute)
            .function("getOutline", &ChorematicMap::getOutline)
            .function("getBin", &ChorematicMap::getBin)
            .function("getWeightMap", &ChorematicMap::getWeightMap);
}
