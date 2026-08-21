#include <mbgl/renderer/layers/fill_extrusion_shadow_layer_tweaker.hpp>

#include <mbgl/gfx/context.hpp>
#include <mbgl/gfx/drawable.hpp>
#include <mbgl/gfx/renderable.hpp>
#include <mbgl/gfx/renderer_backend.hpp>
#include <mbgl/math/angles.hpp>
#include <mbgl/renderer/buckets/fill_extrusion_bucket.hpp>
#include <mbgl/renderer/layer_group.hpp>
#include <mbgl/renderer/paint_parameters.hpp>
#include <mbgl/renderer/paint_property_binder.hpp>
#include <mbgl/renderer/render_tile.hpp>
#include <mbgl/renderer/render_tree.hpp>
#include <mbgl/shaders/fill_extrusion_shadow_layer_ubo.hpp>
#include <mbgl/shaders/shader_program_base.hpp>
#include <mbgl/style/layers/fill_extrusion_layer_properties.hpp>
#include <mbgl/util/constants.hpp>
#include <mbgl/util/convert.hpp>
#include <mbgl/util/projection.hpp>

#include <algorithm>
#include <cmath>

namespace mln {

using namespace shaders;
using namespace style;

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

    // The mask stage only reads `base` and `height`; the blur and composite stages own the colour,
    // opacity and blur fields in their own copy of this struct.
    const FillExtrusionShadowPropsUBO propsUBO = {.color = {},
                                                  .texel_step = {0.0f, 0.0f},
                                                  .blur_scale = 0.0f,
                                                  .opacity = 0.0f,
                                                  .base = constOrDefault<FillExtrusionBase>(evaluated),
                                                  .height = constOrDefault<FillExtrusionHeight>(evaluated),
                                                  .pad1 = 0,
                                                  .pad2 = 0};
    auto& layerUniforms = layerGroup.mutableUniformBuffers();
    layerUniforms.createOrUpdate(idFillExtrusionShadowPropsUBO, &propsUBO, context);

    propertiesUpdated = false;

    // Direction the shadow is cast towards, in tile space, where +x is east and +y is south. The
    // azimuth is map-anchored, so it keeps a fixed compass bearing as the map rotates.
    const auto azimuthRad = util::deg2rad(static_cast<double>(evaluated.get<FillExtrusionShadowAzimuth>()));
    const double dirX = std::sin(azimuthRad);
    const double dirY = -std::cos(azimuthRad);

    // Spec `minimum` is documentation only -- nothing upstream of here clamps.
    const auto length = std::max(evaluated.get<FillExtrusionShadowLength>(), 0.0f);

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
