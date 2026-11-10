#include <iomanip>
#include <iostream>
#include <sstream>

#include <emscripten/bind.h>
#include <emscripten/emscripten.h>
#include <emscripten/val.h>

#include "library/choropleth.h"
#include "library/choropleth_disks.h"
#include "library/disk_area.h"
#include "library/maximum_weight_disk.h"
#include "library/geojson_parsing.h"
#include "library/sampler.h"

#include <cartocrow/renderer/render_path.h>

using namespace emscripten;

using namespace cartocrow;
using namespace cartocrow::chorematic_map;

/// SVG path data of a render path, with enough digits for coordinates of any magnitude.
std::string svgPathData(const RenderPath& path) {
    std::stringstream ss;
    ss << std::setprecision(12);
    for (const auto& c : path.commands()) {
        if (std::holds_alternative<RenderPath::MoveTo>(c)) {
            Point<Inexact> to = std::get<RenderPath::MoveTo>(c).m_to;
            ss << "M " << to.x() << " " << to.y() << " ";
        } else if (std::holds_alternative<RenderPath::LineTo>(c)) {
            Point<Inexact> to = std::get<RenderPath::LineTo>(c).m_to;
            ss << "L " << to.x() << " " << to.y() << " ";
        } else if (std::holds_alternative<RenderPath::Close>(c)) {
            ss << "Z ";
        }
    }
    return ss.str();
}

/// Call f, and turn C++ exceptions into JavaScript errors with their message.
/// With -fwasm-exceptions, a C++ exception reaches JavaScript as a WebAssembly.Exception, which has no readable
/// message and cannot be sent from a web worker to the main thread.
template <typename F> auto withJSErrors(F f) -> decltype(f()) {
    try {
        return f();
    } catch (const std::exception& e) {
        val::global("Error").new_(std::string(e.what())).throw_();
    } catch (...) {
        val::global("Error").new_(std::string("Unknown C++ exception")).throw_();
    }
}

/// The chorematic map pipeline for use from JavaScript: regions are loaded once, then sampled, then a disk is fit
/// to a class of regions. The region arrangement and the sample are kept between calls, as they are expensive to
/// compute. All inputs and outputs are plain JavaScript values.
class ChorematicMap {
private:
    std::shared_ptr<RegionArrangement> m_arr;
    std::unique_ptr<Sampler> m_sampler;
    std::optional<WeightedRegionSample<Exact>> m_sample;
    /// Whether Voronoi sampling is being computed in steps; see beginSample.
    bool m_voronoiRunning = false;

public:
    /// Load the regions from a GeoJSON feature collection; the property regionAttribute identifies each region.
    void loadGeoJSON(const std::string& geojson, const std::string& regionAttribute) {
        auto [regionMap, _] = parseGeoJSON(geojson, regionAttribute);
        m_arr = std::make_shared<RegionArrangement>(regionMapToArrangement(*regionMap));
        m_sampler = std::make_unique<Sampler>(m_arr, 0);
        m_sample = std::nullopt;
    }

    /// The outline of the land (the union of all regions) as SVG path data.
    std::string getOutline() {
        RenderPath rp;
        for (const auto& poly : m_sampler->getLandmassPolys()) {
            rp << approximate(poly);
        }
        return svgPathData(rp);
    }

    // Sampling is done in steps, so that the worker can handle messages in between, for example to stop sampling
    // when the settings change: beginSample, then continueSample until it returns 1, then sampleCoordinates.
    // Only Voronoi sampling takes long enough to need more than one step.

    /// Start sampling points in the regions. settings is {method, n, perRegion, seed, voronoiIterations}, where method
    /// is "uniform", "voronoi", "squareGrid" or "hexGrid".
    void beginSample(val settings) {
        auto method = settings["method"].as<std::string>();
        int n = settings["n"].as<int>();
        m_sampler->setSamplePerRegion(settings["perRegion"].as<bool>());
        m_sampler->setSeed(settings["seed"].as<int>());
        m_voronoiRunning = false;

        if (method == "uniform") {
            m_sample = m_sampler->uniformRandomSamples(n);
        } else if (method == "voronoi") {
            m_sampler->startVoronoiUniform(n, settings["voronoiIterations"].as<int>());
            m_voronoiRunning = true;
        } else if (method == "squareGrid") {
            m_sample = m_sampler->squareGrid(n);
        } else if (method == "hexGrid") {
            m_sample = m_sampler->hexGrid(n);
        } else {
            throw std::runtime_error("Unknown sampling method " + method);
        }
    }

    /// Continue sampling for about maxMilliseconds; returns the fraction that is done, which is 1 when it is done.
    double continueSample(double maxMilliseconds) {
        if (!m_voronoiRunning) return 1;
        double start = emscripten_get_now();
        while (emscripten_get_now() - start < maxMilliseconds) {
            if (m_sampler->voronoiStep()) {
                m_sample = m_sampler->voronoiResult();
                m_voronoiRunning = false;
                return 1;
            }
        }
        return m_sampler->voronoiProgress();
    }

    /// The x and y coordinates of the sample points, in a flat array.
    val sampleCoordinates() {
        val coordinates = val::array();
        for (const auto& p : m_sample->m_points) {
            coordinates.call<void>("push", CGAL::to_double(p.x()));
            coordinates.call<void>("push", CGAL::to_double(p.y()));
        }
        return coordinates;
    }

    // Fitting a disk is split into three steps, as in fitDisks for two classes, so that the slow middle step can run
    // in another worker that does not have the regions and can be terminated to stop it:
    // 1. weightedPoints: give each sample point the weight of its region;
    // 2. maximumWeightDisk (a free function): find the smallest disk of maximum total weight;
    // 3. finishDisk: compute the score of that disk, and optionally refine its radius.
    // In each step, classes maps each region to class 0 or 1 (regions without a class are left out), and settings is
    // {invert, heuristic, symmetricDifference}; the disk is fit to class 1, or to class 0 with invert.

    /// The x and y coordinate and the weight of each sample point, in a flat array.
    val weightedPoints(val classes, val settings) {
        if (!m_sample.has_value()) {
            throw std::runtime_error("There is no sample to fit a disk to.");
        }
        auto weights = fitWeights(classes, settings);
        std::vector<WeightedPoint> points;
        m_sample->weightedPoints(std::back_inserter(points), weights.regionWeight);
        val result = val::array();
        for (const auto& wp : points) {
            result.call<void>("push", wp.point.x());
            result.call<void>("push", wp.point.y());
            result.call<void>("push", wp.weight);
        }
        return result;
    }

    /// Turn the points that define the disk (see maximumWeightDisk) into a disk, and compute its score.
    /// Returns {disk, score}, where disk is null, {kind: "circle", cx, cy, r}, or {kind: "halfplane", pathData}
    /// with the half-plane clipped to (a margin around) the bounding box of the regions.
    val finishDisk(val definingPoints, val classes, val settings) {
        auto weights = fitWeights(classes, settings);
        std::vector<Point<Exact>> pts;
        if (!definingPoints.isNull()) {
            auto n = definingPoints["length"].as<unsigned int>();
            for (unsigned int i = 0; i + 1 < n; i += 2) {
                pts.emplace_back(definingPoints[i].as<double>(), definingPoints[i + 1].as<double>());
            }
        }

        std::optional<GeneralCircle<Exact>> circle;
        if (pts.size() == 3) {
            if (abs(Triangle<Inexact>(approximate(pts[0]), approximate(pts[1]), approximate(pts[2])).area()) < M_EPSILON) {
                circle = Halfplane<Exact>(Line<Exact>(pts[0], pts[1]));
            } else {
                circle = Circle<Exact>(pts[0], pts[1], pts[2]);
            }
        } else if (pts.size() == 2) {
            circle = Circle<Exact>(pts[0], pts[1]);
        }

        val result = val::object();
        result.set("disk", val::null());
        if (!circle.has_value()) {
            result.set("score", 0);
            return result;
        }

        double score = totalWeight(*circle, *m_arr, weights.regionWeight) / weights.normalizer;
        if (settings["heuristic"].as<bool>()) {
            double areaPerPoint = (weights.positiveArea + weights.negativeArea) / m_sample->m_points.size();
            double deltaRadius = sqrt(areaPerPoint) * 2;
            auto [refined, refinedScore] =
                perturbDiskRadius(*circle, score, *m_arr, weights.regionWeight, deltaRadius, 20, weights.normalizer);
            circle = refined;
            score = refinedScore;
        }
        result.set("score", score);

        val disk = val::object();
        if (circle->is_circle()) {
            auto c = approximate(circle->get_circle());
            disk.set("kind", std::string("circle"));
            disk.set("cx", c.center().x());
            disk.set("cy", c.center().y());
            disk.set("r", sqrt(c.squared_radius()));
        } else {
            auto bb = approximate(m_sampler->getArrBoundingBox());
            double margin = std::max(bb.xmax() - bb.xmin(), bb.ymax() - bb.ymin());
            Rectangle<Inexact> rect(bb.xmin() - margin, bb.ymin() - margin, bb.xmax() + margin, bb.ymax() + margin);
            Halfplane<Inexact> halfplane(approximate(circle->get_halfplane().line()));
            RenderPath rp;
            rp << halfplane.polygon(rect);
            disk.set("kind", std::string("halfplane"));
            disk.set("pathData", svgPathData(rp));
        }
        result.set("disk", disk);
        return result;
    }

private:
    struct FitWeights {
        std::unordered_map<std::string, double> regionWeight;
        double positiveArea;
        double negativeArea;
        /// The weight of a disk that covers exactly the class, so that scores are between 0 and 1.
        double normalizer;
    };

    /// The weight of each region, as in fitDisks.
    FitWeights fitWeights(val classes, val settings) {
        auto data = std::make_shared<std::unordered_map<std::string, double>>();
        val keys = val::global("Object").call<val>("keys", classes);
        auto nKeys = keys["length"].as<unsigned int>();
        for (unsigned int i = 0; i < nKeys; ++i) {
            auto key = keys[i].as<std::string>();
            (*data)[key] = classes[key].as<double>();
        }
        std::vector<double> thresholds({0.5});
        Choropleth choropleth(m_arr, data, thresholds.begin(), thresholds.end());

        int binToFit = settings["invert"].as<bool>() ? 0 : 1;
        bool symmetricDifference = settings["symmetricDifference"].as<bool>();
        auto binAreas = choropleth.binAreas();
        Number<Exact> positiveArea = binAreas[binToFit];
        Number<Exact> negativeArea = binAreas[1 - binToFit];
        auto totalArea = positiveArea + negativeArea;

        FitWeights weights;
        for (const auto& [region, _] : *data) {
            auto bin = choropleth.regionToBin(region);
            if (!bin.has_value()) continue;
            if (symmetricDifference) {
                weights.regionWeight[region] = *bin == binToFit ? 1 : -1;
            } else {
                weights.regionWeight[region] =
                    CGAL::to_double(*bin == binToFit ? negativeArea / totalArea : -positiveArea / totalArea);
            }
        }
        weights.positiveArea = CGAL::to_double(positiveArea);
        weights.negativeArea = CGAL::to_double(negativeArea);
        weights.normalizer = CGAL::to_double(positiveArea * negativeArea / totalArea);
        return weights;
    }
};

/// The smallest disk of maximum total weight of weighted points, given as the x, y and weight of each point in a flat
/// array (see ChorematicMap::weightedPoints). progress is called with a number between 0 and 1.
/// Returns the x and y coordinates of the points that define the disk (two or three; see finishDisk) in a flat array,
/// or null if there is no such disk.
val maximumWeightDisk(val points, val progress) {
    std::vector<WeightedPoint> weightedPoints;
    auto n = points["length"].as<unsigned int>();
    for (unsigned int i = 0; i + 2 < n; i += 3) {
        weightedPoints.emplace_back(Point<Inexact>(points[i].as<double>(), points[i + 1].as<double>()),
                                    points[i + 2].as<double>());
    }
    auto [p1, p2, p3] = smallest_maximum_weight_disk(weightedPoints.begin(), weightedPoints.end(),
                                                     [&progress](int percentage) { progress(percentage / 100.0); });
    // A single point does not define a disk, as in fitDisks.
    if (!p1.has_value() || !p2.has_value()) return val::null();
    val result = val::array();
    for (const auto& p : {p1, p2, p3}) {
        if (!p.has_value()) continue;
        result.call<void>("push", p->point.x());
        result.call<void>("push", p->point.y());
    }
    return result;
}

EMSCRIPTEN_BINDINGS(chorematic_map) {
    class_<ChorematicMap>("ChorematicMap")
            .constructor<>()
            .function("loadGeoJSON", optional_override([](ChorematicMap& m, const std::string& geojson, const std::string& regionAttribute) {
                withJSErrors([&] { m.loadGeoJSON(geojson, regionAttribute); });
            }))
            .function("getOutline", optional_override([](ChorematicMap& m) {
                return withJSErrors([&] { return m.getOutline(); });
            }))
            .function("beginSample", optional_override([](ChorematicMap& m, val settings) {
                withJSErrors([&] { m.beginSample(settings); });
            }))
            .function("continueSample", optional_override([](ChorematicMap& m, double maxMilliseconds) {
                return withJSErrors([&] { return m.continueSample(maxMilliseconds); });
            }))
            .function("sampleCoordinates", optional_override([](ChorematicMap& m) {
                return withJSErrors([&] { return m.sampleCoordinates(); });
            }))
            .function("weightedPoints", optional_override([](ChorematicMap& m, val classes, val settings) {
                return withJSErrors([&] { return m.weightedPoints(classes, settings); });
            }))
            .function("finishDisk", optional_override([](ChorematicMap& m, val definingPoints, val classes, val settings) {
                return withJSErrors([&] { return m.finishDisk(definingPoints, classes, settings); });
            }));
    function("maximumWeightDisk", optional_override([](val points, val progress) {
        return withJSErrors([&] { return maximumWeightDisk(points, progress); });
    }));
}
