#pragma once

#include <mln/renderer/layer_tweaker.hpp>

#include <string>

namespace mln {

/// Tweaker for the fill-extrusion ground-shadow mask pass.
///
/// Runs against the mask render target's own tile layer group, so its uniform buffers are separate
/// from both the building layer's and the composite stage's.
class FillExtrusionShadowLayerTweaker : public LayerTweaker {
public:
    FillExtrusionShadowLayerTweaker(std::string id_, Immutable<style::LayerProperties> properties)
        : LayerTweaker(std::move(id_), properties) {}

    ~FillExtrusionShadowLayerTweaker() override = default;

    void execute(LayerGroupBase&, const PaintParameters&) override;

protected:
#if MLN_UBO_CONSOLIDATION
    gfx::UniformBufferPtr drawableUniformBuffer;
#endif
};

} // namespace mln
