#pragma once

#include <mln/renderer/layer_tweaker.hpp>

#include <array>
#include <optional>
#include <string>

namespace mln {

/// Where a fill-extrusion's ground shadow falls for a given light.
struct FillExtrusionShadowDirection {
    /// Unit direction the shadow is cast towards in tile space, where +x is east and +y is south
    double x = 0;
    double y = 0;
    /// Shadow length as a multiple of the extrusion height, i.e. cot of the light's elevation
    double length = 0;
};

/// The ground shadow for `lightPosition`, the light vector the building shading uses (see
/// `FillExtrusionBucket::lightPosition`, which already applies `light.anchor`). Using the same
/// vector keeps the shadow on the unlit side of the walls for either anchor. Empty when the light is
/// at or below the horizon. Very low lights are clamped to `minimumShadowElevation`.
std::optional<FillExtrusionShadowDirection> fillExtrusionShadowDirection(const std::array<float, 3>& lightPosition);

/// Lowest light elevation, in degrees, that still casts a full-length shadow
constexpr double minimumShadowElevation = 5.0;

/// Tweaker for the fill-extrusion ground-shadow drawables.
///
/// Runs against the shadow's own tile layer group (registered ahead of the building layer group at
/// the same layer index, so shadows draw underneath the buildings), and owns the drawable-to-drawable
/// matrix/shear as well as the layer-wide colour/opacity, base and height.
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
