#pragma once

#include <mbgl/renderer/render_layer.hpp>
#include <mbgl/renderer/render_target.hpp>
#include <mbgl/renderer/buckets/fill_extrusion_bucket.hpp>
#include <mbgl/style/layers/fill_extrusion_layer_impl.hpp>
#include <mbgl/style/layers/fill_extrusion_layer_properties.hpp>

namespace mln {

class FillExtrusionShadowTextureLayerTweaker;

class RenderFillExtrusionLayer final : public RenderLayer {
public:
    explicit RenderFillExtrusionLayer(Immutable<style::FillExtrusionLayer::Impl>);
    ~RenderFillExtrusionLayer() override;

private:
    void transition(const TransitionParameters&) override;
    void evaluate(const PropertyEvaluationParameters&) override;
    bool hasTransition() const override;
    bool hasCrossfade() const override;
    bool is3D() const override;

    void markLayerRenderable(bool willRender, UniqueChangeRequestVec&) override;
    void layerIndexChanged(int32_t newLayerIndex, UniqueChangeRequestVec&) override;
    std::size_t removeAllDrawables() override;

    /// Generate any changes needed by the layer
    void update(gfx::ShaderRegistry&,
                gfx::Context&,
                const TransformState&,
                const std::shared_ptr<UpdateParameters>&,
                const PaintParameters&,
                const RenderTree&,
                UniqueChangeRequestVec&) override;

    bool queryIntersectsFeature(const GeometryCoordinates&,
                                const GeometryTileFeature&,
                                float,
                                const TransformState&,
                                float,
                                const mat4&,
                                const FeatureState&) const override;

    // Paint properties
    style::FillExtrusionPaintProperties::Unevaluated unevaluated;

    gfx::ShaderGroupPtr fillExtrusionGroup;
    gfx::ShaderGroupPtr fillExtrusionPatternGroup;

#if MLN_USE_FILL_EXTRUSION_INSTANCING
    gfx::ShaderGroupPtr fillExtrusionInstancedGroup;
    gfx::ShaderGroupPtr fillExtrusionPatternInstancedGroup;
#endif

#if MLN_USE_FILL_EXTRUSION_INSTANCING || MLN_USE_FILL_EXTRUSION_SHADOW
    // Shared by the regular building walls' instanced layout (Metal/Vulkan) and the shadow mask's
    // static full-screen/unit quads (every backend that supports shadows).
    using FillExtrusionVertexVector = gfx::VertexVector<FillExtrusionStaticVertex>;
    using TriangleIndexVector = gfx::IndexVector<gfx::Triangles>;

    std::shared_ptr<FillExtrusionVertexVector> staticDataVertices;
    std::shared_ptr<TriangleIndexVector> staticDataIndices;
    std::shared_ptr<SegmentVector> staticDataSegments;
#endif

#if MLN_USE_FILL_EXTRUSION_SHADOW
    // Ground shadows. The mask silhouette is rendered offscreen, blurred, then composited under the
    // buildings. The wall-mask shader's per-instance data layout is backend-specific (see
    // MLN_USE_FILL_EXTRUSION_SHADOW's definition); on backends without a shadow shader
    // specialization, the shader lookup in prepareShadow() simply fails and the whole feature stays
    // switched off.

    /// Whether the evaluated properties ask for a shadow at all.
    bool shadowEnabled() const;

    /// Create or resize the render targets and the composite layer group. Returns false if any
    /// resource could not be obtained, in which case the caller should skip the shadow entirely.
    bool prepareShadow(gfx::ShaderRegistry&, gfx::Context&, const TransformState&, UniqueChangeRequestVec&);

    /// Release every shadow resource and deregister it from the orchestrator.
    void teardownShadow(UniqueChangeRequestVec&);

    /// Rebuild the two full-screen quads. Called once per update, after the per-tile mask drawables.
    void updateShadowQuads(gfx::Context&);

    RenderTargetPtr shadowMaskTarget;
    RenderTargetPtr shadowBlurTarget;
    /// Composite quad. Registered at the same layerIndex as `layerGroup` but *before* it, so it
    /// draws underneath the buildings.
    LayerGroupBasePtr shadowCompositeGroup;

    gfx::ShaderGroupPtr shadowMaskShaderGroup;
    gfx::ShaderGroupPtr shadowMaskInstancedShaderGroup;
    gfx::ShaderProgramBasePtr shadowBlurShader;
    gfx::ShaderProgramBasePtr shadowCompositeShader;

    LayerTweakerPtr shadowMaskTweaker;
    std::shared_ptr<FillExtrusionShadowTextureLayerTweaker> shadowBlurTweaker;
    std::shared_ptr<FillExtrusionShadowTextureLayerTweaker> shadowCompositeTweaker;

    /// Tracks whether the shadow was active last update, so that toggling it forces a full drawable
    /// rebuild. Without this, tiles that already have building drawables get skipped by updateTile
    /// and would never gain their mask drawables.
    bool shadowWasEnabled = false;

    // Throttled cost reporting, enabled with the MLN_SHADOW_STATS environment variable. Reports the
    // CPU side of the shadow only; the GPU cost shows up as extra draw calls and one extra offscreen
    // pass per render target in a frame capture.
    void reportShadowStats(double setupMs, double quadsMs);
    std::uint64_t shadowFrameCount = 0;
    double shadowSetupMsAccum = 0.0;
    double shadowQuadsMsAccum = 0.0;
#endif
};

} // namespace mln
