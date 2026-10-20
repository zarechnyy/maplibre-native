#include <mln/renderer/layers/fill_extrusion_shadow_layer_tweaker.hpp>

#include <mln/gfx/context.hpp>
#include <mln/gfx/drawable.hpp>
#include <mln/gfx/renderable.hpp>
#include <mln/gfx/renderer_backend.hpp>
#include <mln/math/angles.hpp>
#include <mln/renderer/buckets/fill_extrusion_bucket.hpp>
#include <mln/renderer/layer_group.hpp>
#include <mln/renderer/paint_parameters.hpp>
#include <mln/renderer/paint_property_binder.hpp>
#include <mln/renderer/render_tile.hpp>
#include <mln/renderer/render_tree.hpp>
#include <mln/shaders/fill_extrusion_shadow_layer_ubo.hpp>
#include <mln/shaders/shader_program_base.hpp>
#include <mln/style/layers/fill_extrusion_layer_properties.hpp>
#include <mln/util/constants.hpp>
#include <mln/util/convert.hpp>
#include <mln/util/projection.hpp>

#include <algorithm>
#include <cmath>

namespace mln {

using namespace shaders;
using namespace style;

std::optional<FillExtrusionShadowDirection> fillExtrusionShadowDirection(const std::array<float, 3>& lightPosition) {
    const double x = lightPosition[0];
    const double y = lightPosition[1];
    const double z = lightPosition[2];
    if (z <= 0.0) {
        return std::nullopt;
    }
    const double horizontal = std::hypot(x, y);
    if (horizontal == 0.0) {
        // Straight overhead
        return FillExtrusionShadowDirection{};
    }
    // The shading lights walls whose normal faces away from (x, y), so the shadow falls along it
    const double maxLength = 1.0 / std::tan(util::deg2rad(minimumShadowElevation));
    return FillExtrusionShadowDirection{
        .x = x / horizontal, .y = y / horizontal, .length = std::min(horizontal / z, maxLength)};
}

void FillExtrusionShadowLayerTweaker::execute(LayerGroupBase& layerGroup, const PaintParameters& parameters) {
    if (layerGroup.empty()) {
        return;
    }

    auto& context = parameters.context;
    const auto& props = static_cast<const FillExtrusionLayerProperties&>(*evaluatedProperties);
    const auto& evaluated = props.evaluated;
    const auto& state = parameters.state;

#if !defined(NDEBUG)
    const auto label = layerGroup.getName() + "-update-uniforms";
    const auto debugGroup = parameters.encoder->createDebugGroup(label.c_str());
#endif

    // The direction and length come from the root light, through the same vector the building
    // shading uses: with `light.anchor: map` the shadow keeps its geographic direction, with
    // `viewport` it keeps its direction on screen.
    const auto shadow = fillExtrusionShadowDirection(
        FillExtrusionBucket::lightPosition(parameters.evaluatedLight, state));

    // Spec `minimum`/`maximum` are documentation only, so clamp here rather than trusting the style.
    // A light at or below the horizon casts no shadow.
    const auto opacity = shadow ? util::clamp(evaluated.get<FillExtrusionShadowOpacity>(), 0.0f, 1.0f) : 0.0f;

    // The alpha of the shadow color is ignored, `fill-extrusion-shadow-opacity` sets it. Colors are
    // premultiplied, so recover the straight color first.
    const auto& shadowColor = evaluated.get<FillExtrusionShadowColor>();
    const Color color = shadowColor.a > 0.0f ? Color{shadowColor.r / shadowColor.a,
                                                     shadowColor.g / shadowColor.a,
                                                     shadowColor.b / shadowColor.a,
                                                     1.0f}
                                             : Color::black();

    const FillExtrusionShadowPropsUBO propsUBO = {.color = color,
                                                  .opacity = opacity,
                                                  .base = constOrDefault<FillExtrusionBase>(evaluated),
                                                  .height = constOrDefault<FillExtrusionHeight>(evaluated),
                                                  .pad1 = 0};
    auto& layerUniforms = layerGroup.mutableUniformBuffers();
    layerUniforms.createOrUpdate(idFillExtrusionShadowPropsUBO, &propsUBO, context);

    propertiesUpdated = false;

    const double dirX = shadow ? shadow->x : 0.0;
    const double dirY = shadow ? shadow->y : 0.0;
    const double length = shadow ? shadow->length : 0.0;

    // Metres of extrusion height convert to tile units as
    //
    //     2^tile.z * EXTENT / (cos(lat) * 2*pi * EARTH_RADIUS_M)
    //
    // because the tile matrix scales x/y by (worldSize / 2^tile.z) / EXTENT while leaving z alone,
    // and the camera matrix separately scales z by pixelsPerMeter (see Camera::getWorldToCamera).
    // Everything except the per-tile 2^tile.z is hoisted out of the loop here.
    //
    // Note this uses the *camera centre* latitude, matching what getWorldToCamera uses. Using each
    // tile's own latitude instead would make shadow length jump between tiles.
    const double metersPerPixel = Projection::getMetersPerPixelAtLatitude(state.getLatLng().latitude(),
                                                                          state.getZoom());
    const double tileUnitsPerMeterBase = util::EXTENT / (metersPerPixel * Projection::worldSize(state.getScale()));

    const auto zoom = static_cast<float>(state.getZoom());

#if MLN_UBO_CONSOLIDATION
    int i = 0;
    std::vector<FillExtrusionShadowDrawableUBO> drawableUBOVector(layerGroup.getDrawableCount());
#endif

    visitLayerGroupDrawables(layerGroup, [&](gfx::Drawable& drawable) {
        if (!drawable.getTileID() || !checkTweakDrawable(drawable)) {
            return;
        }

        auto* binders = static_cast<FillExtrusionBinders*>(drawable.getBinders());
        const auto* tile = drawable.getRenderTile();
        if (!binders || !tile) {
            assert(false);
            return;
        }

        const UnwrappedTileID tileID = drawable.getTileID()->toUnwrapped();
        const auto& translation = evaluated.get<FillExtrusionTranslate>();
        const auto anchor = evaluated.get<FillExtrusionTranslateAnchor>();
        constexpr bool inViewportPixelUnits = false; // from RenderTile::translatedMatrix
        constexpr bool nearClipped = true;
        const auto matrix = getTileMatrix(
            tileID, parameters, translation, anchor, nearClipped, inViewportPixelUnits, drawable);

        const auto numTiles = static_cast<double>(1ull << tileID.canonical.z);
        const double shear = length * numTiles * tileUnitsPerMeterBase;

#if MLN_UBO_CONSOLIDATION
        drawableUBOVector[i] = {
#else
        const FillExtrusionShadowDrawableUBO drawableUBO = {
#endif
            .matrix = util::cast<float>(matrix),
            .offset_per_meter = {static_cast<float>(dirX * shear), static_cast<float>(dirY * shear)},

            .base_t = std::get<0>(binders->get<FillExtrusionBase>()->interpolationFactor(zoom)),
            .height_t = std::get<0>(binders->get<FillExtrusionHeight>()->interpolationFactor(zoom))
        };

#if MLN_UBO_CONSOLIDATION
        drawable.setUBOIndex(i++);
#else
        auto& drawableUniforms = drawable.mutableUniformBuffers();
        drawableUniforms.createOrUpdate(idFillExtrusionShadowDrawableUBO, &drawableUBO, context);
#endif
    });

#if MLN_UBO_CONSOLIDATION
    const size_t drawableUBOVectorSize = sizeof(FillExtrusionShadowDrawableUBO) * drawableUBOVector.size();
    if (!drawableUniformBuffer || drawableUniformBuffer->getSize() < drawableUBOVectorSize) {
        drawableUniformBuffer = context.createUniformBuffer(
            drawableUBOVector.data(), drawableUBOVectorSize, false, true);
    } else {
        drawableUniformBuffer->update(drawableUBOVector.data(), drawableUBOVectorSize);
    }

    layerUniforms.set(idFillExtrusionShadowDrawableUBO, drawableUniformBuffer);
#endif
}

} // namespace mln
