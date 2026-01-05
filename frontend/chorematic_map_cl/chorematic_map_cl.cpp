#include <filesystem>
#include <fstream>

#include <QApplication>

#include <nlohmann/json.hpp>

#include <cartocrow/core/centroid.h>
#include <cartocrow/core/region_map.h>
#include <cartocrow/core/region_arrangement.h>
#include <cartocrow/core/transform_helpers.h>
#include <cartocrow/reader/ipe_reader.h>
#include <cartocrow/reader/region_map_reader.h>
#include <cartocrow/renderer/geometry_painting.h>
#include <cartocrow/renderer/painting_renderer.h>
#include <cartocrow/renderer/svg_renderer.h>

#include "library/choropleth.h"
#include "library/choropleth_disks.h"
#include "library/sampler.h"
#include "library/input_parsing.h"

using namespace cartocrow;
using json = nlohmann::json;

int main(int argc, char* argv[]) {
    if (argc != 3 && argc != 4) {
        std::cout << "Usage: chorematic_map_cl <project_file> <output_file> [<map_file>]\n";
        std::cout << "where <project_file> is a JSON file describing the map to generate,\n";
        std::cout << "<output_file> is the SVG file to write the output to, and <map_file>\n";
        std::cout << "is an Ipe file containing the underlying map (if necessary for the\n";
        std::cout << "map type generated.)\n";
        return 1;
    }

    const std::filesystem::path projectFilename = argv[1];
    const std::filesystem::path outputFilename = argv[2];
    std::string mapFilename = "";
    if (argc == 4) {
        mapFilename = argv[3];
    }
    std::ifstream f(projectFilename);
    json projectData = json::parse(f);

    std::shared_ptr<renderer::GeometryPainting> painting;
    std::shared_ptr<renderer::GeometryPainting> debugPainting;

    RegionMap map = ipeToRegionMap(mapFilename, true);
    auto arr = std::make_shared<RegionArrangement>(regionMapToArrangementParallel(map));

    auto regionDataFilePath = projectFilename.parent_path() / projectData["regionData"];
    std::ifstream inputStream(regionDataFilePath, std::ios_base::in);
    if (!inputStream.good()) {
        throw std::runtime_error("Failed to read input");
    }
    std::stringstream buffer;
    buffer << inputStream.rdbuf();

    auto regionData = std::make_shared<chorematic_map::RegionWeight>(chorematic_map::parseRegionData(buffer.str()));
    chorematic_map::Choropleth choropleth(arr, regionData, 2);

    auto boundsCoords = projectData["outputBounds"];
    double xMin = boundsCoords[0];
    double yMin = boundsCoords[1];
    double xMax = boundsCoords[2];
    double yMax = boundsCoords[3];
    // As svg renderer inverts y, we do -y...
    Rectangle<Inexact> bboxOutput(xMin, -yMax, xMax, -yMin);
    Rectangle<Inexact> arrBbox = bboxInexact(*arr);
    auto trans = fitInto(arrBbox, bboxOutput);

    bool schematize = projectData.contains("schematization");
    renderer::RenderPath schematization;
    if (schematize) {
        auto schematizationPath = projectFilename.parent_path() / projectData["schematization"];
        schematization = orthogonal_transform(trans, IpeReader::loadIpePath(schematizationPath));
    }

    auto entryToColor = [](const nlohmann::basic_json<>& entry) {
        std::string hexString = entry.get<std::string>();
        return Color(std::strtol(hexString.c_str(), nullptr, 0));
    };

    std::vector<Color> colors;
    for (const auto& entry : projectData["binColors"]) {
        colors.push_back(entryToColor(entry));
    }

    chorematic_map::ChoroplethPainting::Options choroplethPOptions;
    Color outlineColor = entryToColor(projectData["outlineColor"]);
    Color boundaryColor = entryToColor(projectData["boundaryColor"]);
    choroplethPOptions.drawLabels = false;
    choroplethPOptions.noDataColor = Color(255, 0, 0);
    choroplethPOptions.strokeColor = boundaryColor;
    choroplethPOptions.strokeWidth = 0.75;

    choroplethPOptions.transformation = trans;

    chorematic_map::ChoroplethPainting choroplethP(choropleth, colors.begin(), colors.end(), choroplethPOptions);

    auto pr = std::make_shared<renderer::PaintingRenderer>();
    renderer::GeometryRenderer& gr = *pr;
    if (!schematize) {
        choroplethP.paint(gr);
    }

    int seed = projectData["seed"];
    bool local = projectData["local"];

    chorematic_map::Sampler sampler(arr, seed, local);

    chorematic_map::WeightedRegionSample<Exact> sample;
    std::string technique = projectData["technique"];
    int n = projectData["nPoints"];
    if (technique == "Voronoi") {
        sample = sampler.voronoiUniform(n, 25);
    } else if (technique == "Random") {
        sample = sampler.uniformRandomSamples(n);
    } else if (technique == "Square") {
        sample = sampler.squareGrid(n);
    } else if (technique == "Hex") {
        sample = sampler.hexGrid(n);
    } else {
        std::stringstream errMsg;
        errMsg << "Unknown sampling technique: " << technique;
        throw std::runtime_error(errMsg.str());
    }

    bool invert = projectData["invert"];
    auto disks = chorematic_map::fitDisks(choropleth, sample, invert, false, false, false);

    if (schematize) {
        gr.setMode(renderer::GeometryRenderer::fill);
        auto bgBin = disks[0].bin == 0 ? 1 : 0;
        gr.setFill(choroplethP.m_colors[bgBin]);
        gr.draw(schematization);
    }

    for (const auto& disk : disks) {
        for (const auto& binDisk : disks) {
            gr.setMode(renderer::GeometryRenderer::stroke | renderer::GeometryRenderer::fill);
            gr.setFill(choroplethP.m_colors[binDisk.bin]);
            if (!schematize) {
                gr.setFillOpacity(127);
                gr.setStroke(boundaryColor, 2.0);
            } else {
                gr.setClipping(true);
                gr.setClipPath(schematization);
                gr.setStroke(boundaryColor, 4.0);
            }
            auto c = binDisk.disk;
            if (c.has_value() && c->is_circle()) {
                gr.draw(approximate(c->get_circle()).orthogonal_transform(trans));
            } else {
                auto hp = c->get_halfplane();
                gr.draw(Halfplane<Inexact>(approximate(hp.line()).transform(trans)));
            }
            gr.setClipping(false);
        }
    }

    if (!schematize) {
        const auto& outlinePolys = sampler.getLandmassPolys();
        gr.setStroke(outlineColor, 2);
        for (const auto &outlinePoly: outlinePolys) {
            gr.draw(transform(trans, approximate(outlinePoly)));
        }
    } else {
        gr.setMode(renderer::GeometryRenderer::stroke);
        gr.setStroke(outlineColor, 4);
        gr.draw(schematization);
    }

    painting = pr;

    cartocrow::renderer::SvgRenderer renderer(painting);
    renderer.save(outputFilename);
    return 0;
}