#pragma once

#include <mbgl/renderer/bucket.hpp>
#include <mbgl/renderer/paint_property_binder.hpp>
#include <mbgl/renderer/render_light.hpp>
#include <mbgl/tile/geometry_tile_data.hpp>
#include <mbgl/gfx/vertex_buffer.hpp>
#include <mbgl/gfx/index_buffer.hpp>
#include <mbgl/shaders/segment.hpp>
#include <mbgl/style/layers/fill_extrusion_layer_properties.hpp>

namespace mln {

class BucketParameters;
class RenderFillExtrusionLayer;

using FillExtrusionBinders = PaintPropertyBinders<style::FillExtrusionPaintProperties::DataDrivenProperties>;
using FillExtrusionStaticVertex = gfx::Vertex<TypeList<attributes::pos>>;

#if MLN_USE_FILL_EXTRUSION_INSTANCING
using FillExtrusionLayoutVertex = gfx::Vertex<TypeList<attributes::pos, attributes::decimals_ed>>;
#else
using FillExtrusionLayoutVertex = gfx::Vertex<TypeList<attributes::pos, attributes::decimals_ed, attributes::normal2d>>;
#endif

#if MLN_RENDER_BACKEND_OPENGL
// On this backend `FillExtrusionLayoutVertex`/`sharedTriangles` mix fully-expanded wall quads with
// roof triangles, which the ground shadow can't use directly (it needs one vertex per ring point
// and a roof-only triangle list). Built additively in addFeature() alongside the existing buffers.
using FillExtrusionShadowVertex = gfx::Vertex<TypeList<attributes::pos, attributes::decimals_ed>>;
#endif

class FillExtrusionBucket final : public Bucket {
public:
    ~FillExtrusionBucket() override;
    using PossiblyEvaluatedPaintProperties = style::FillExtrusionPaintProperties::PossiblyEvaluated;
    using PossiblyEvaluatedLayoutProperties = style::FillExtrusionLayoutProperties::PossiblyEvaluated;

    FillExtrusionBucket(const PossiblyEvaluatedLayoutProperties& layout,
                        const std::map<std::string, Immutable<style::LayerProperties>>&,
                        float,
                        uint32_t);

    void addFeature(const GeometryTileFeature&,
                    const GeometryCollection&,
                    const mln::ImagePositions&,
                    const PatternLayerMap&,
                    std::size_t,
                    const CanonicalTileID&) override;

    bool hasData() const override;

    void upload(gfx::UploadPass&) override;

    float getQueryRadius(const RenderLayer&) const override;

    void update(const FeatureStates&, const GeometryTileLayer&, const std::string&, const ImagePositions&) override;

#if MLN_USE_FILL_EXTRUSION_INSTANCING
    static FillExtrusionLayoutVertex layoutVertex(const Point<double>& p, uint16_t edgeDistance, bool isDiscarded) {
        auto intPart = Point<double>(std::floor(p.x), std::floor(p.y));
        // Multiply factional part by 2^7 to pack them into integers [0..127]
        auto fracPart = convertPoint<uint8_t>((p - intPart) * 128.0);

        return FillExtrusionLayoutVertex{
            {static_cast<int16_t>(intPart.x), static_cast<int16_t>(intPart.y)},
            { // We pack a bool (`isDiscarded`) indicating whether this instance is discarded
                static_cast<uint16_t>((fracPart.x * 256 + fracPart.y) * 2 + (isDiscarded ? 1 : 0)),
                // The edgeDistance attribute is used for wrapping fill_extrusion patterns
                edgeDistance
            }};
    }
#else
    static FillExtrusionLayoutVertex layoutVertex(
        const Point<double>& p, double nx, double ny, unsigned short t, uint16_t edgeDistance) {
        const auto factor = pow(2, 14);
        auto intPart = Point<double>(std::floor(p.x), std::floor(p.y));
        // Multiply factional part by 2^7 to pack them into integers [0..127]
        auto fracPart = convertPoint<uint8_t>((p - intPart) * 128.0);

        return FillExtrusionLayoutVertex{
            {static_cast<int16_t>(intPart.x), static_cast<int16_t>(intPart.y)},
            {// We pack a bool (`t`) indicating whether it is an upper or lower vertex
             static_cast<uint16_t>((fracPart.x * 256 + fracPart.y) * 2 + t),
             // The edgedistance attribute is used for wrapping fill_extrusion patterns
             edgeDistance},
            // Multiply normal vector components in the 2D plane by 2^14 to pack them into integers
            {static_cast<int16_t>(nx * factor), static_cast<int16_t>(ny * factor)}};
    }
#endif

#if MLN_RENDER_BACKEND_OPENGL
    // Same shape/packing as the MLN_USE_FILL_EXTRUSION_INSTANCING layoutVertex() overload above,
    // under its own name and type so it doesn't collide with this backend's own layoutVertex().
    static FillExtrusionShadowVertex shadowOutlineVertex(const Point<double>& p,
                                                          uint16_t edgeDistance,
                                                          bool isDiscarded) {
        auto intPart = Point<double>(std::floor(p.x), std::floor(p.y));
        auto fracPart = convertPoint<uint8_t>((p - intPart) * 128.0);

        return FillExtrusionShadowVertex{
            {static_cast<int16_t>(intPart.x), static_cast<int16_t>(intPart.y)},
            {static_cast<uint16_t>((fracPart.x * 256 + fracPart.y) * 2 + (isDiscarded ? 1 : 0)), edgeDistance}};
    }
#endif

    PossiblyEvaluatedLayoutProperties layout;

    static std::array<float, 3> lightColor(const EvaluatedLight&);
    static std::array<float, 3> lightPosition(const EvaluatedLight&, const TransformState&);
    static float lightIntensity(const EvaluatedLight&);

    using VertexVector = gfx::VertexVector<FillExtrusionLayoutVertex>;
    const std::shared_ptr<VertexVector> sharedVertices = std::make_shared<VertexVector>();
    VertexVector& vertices = *sharedVertices;

    using TriangleIndexVector = gfx::IndexVector<gfx::Triangles>;
    const std::shared_ptr<TriangleIndexVector> sharedTriangles = std::make_shared<TriangleIndexVector>();
    TriangleIndexVector& triangles = *sharedTriangles;

    SegmentVector triangleSegments;

#if MLN_RENDER_BACKEND_OPENGL
    // See FillExtrusionShadowVertex above for why these exist alongside sharedVertices/sharedTriangles.
    using ShadowVertexVector = gfx::VertexVector<FillExtrusionShadowVertex>;
    const std::shared_ptr<ShadowVertexVector> sharedShadowVertices = std::make_shared<ShadowVertexVector>();

    const std::shared_ptr<TriangleIndexVector> sharedShadowTriangles = std::make_shared<TriangleIndexVector>();

    SegmentVector shadowTriangleSegments;

    // `paintPropertyBinders` below is built to match `vertices`' indexing (one entry per
    // wall-expanded vertex), so its data-driven `base`/`height` values can't be read directly
    // against `sharedShadowVertices`, which has a different vertex count per feature. This
    // parallel set is populated the same way, but counted against `sharedShadowVertices` instead.
    std::unordered_map<std::string, FillExtrusionBinders> shadowPaintPropertyBinders;
#endif

    std::unordered_map<std::string, FillExtrusionBinders> paintPropertyBinders;
};

} // namespace mln
