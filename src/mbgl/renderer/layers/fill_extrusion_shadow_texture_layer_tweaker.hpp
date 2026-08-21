#pragma once

#include <mbgl/renderer/layer_tweaker.hpp>
#include <mbgl/util/size.hpp>

#include <string>

namespace mln {

/// Tweaker for the two full-screen fill-extrusion shadow stages: the horizontal blur that renders
/// into the second render target, and the composite that folds in the vertical blur and colourises.
///
/// One class, two instances -- they bind the same struct and differ only in which texture they
/// sample, which the drawable itself carries.
class FillExtrusionShadowTextureLayerTweaker : public LayerTweaker {
public:
    FillExtrusionShadowTextureLayerTweaker(std::string id_, Immutable<style::LayerProperties> properties)
        : LayerTweaker(std::move(id_), properties) {}

    ~FillExtrusionShadowTextureLayerTweaker() override = default;

    void execute(LayerGroupBase&, const PaintParameters&) override;

    /// Size of the mask texture being sampled, needed for the blur's texel step. The owning layer
    /// updates this whenever the render target is resized.
    void setMaskSize(Size size) { maskSize = size; }

protected:
    Size maskSize{0, 0};
};

} // namespace mln
