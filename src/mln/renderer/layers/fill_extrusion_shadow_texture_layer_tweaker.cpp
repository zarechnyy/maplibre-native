#include <mln/renderer/layers/fill_extrusion_shadow_texture_layer_tweaker.hpp>

#include <mln/gfx/context.hpp>
#include <mln/gfx/drawable.hpp>
#include <mln/gfx/renderable.hpp>
#include <mln/gfx/renderer_backend.hpp>
#include <mln/renderer/layer_group.hpp>
#include <mln/renderer/paint_parameters.hpp>
#include <mln/renderer/render_tree.hpp>
#include <mln/shaders/fill_extrusion_shadow_layer_ubo.hpp>
#include <mln/style/layers/fill_extrusion_layer_properties.hpp>
#include <mln/util/convert.hpp>

#include <algorithm>

namespace mln {

using namespace style;
using namespace shaders;

namespace {

/// Offset of the outermost tap in the 5-sample Gaussian kernel in the shaders. `blur_scale` is
/// divided by it so that a requested radius lands on the kernel's edge rather than its centre.
constexpr float outermostTapOffset = 3.2307692f;

} // namespace

void FillExtrusionShadowTextureLayerTweaker::execute(LayerGroupBase& layerGroup, const PaintParameters& parameters) {
    if (layerGroup.empty()) {
        return;
    }

    const auto& evaluated = static_cast<const FillExtrusionLayerProperties&>(*evaluatedProperties).evaluated;

#if !defined(NDEBUG)
    const auto label = layerGroup.getName() + "-update-uniforms";
    const auto debugGroup = parameters.encoder->createDebugGroup(label.c_str());
#endif

    propertiesUpdated = false;

    // Spec `minimum`/`maximum` are documentation only, so clamp here rather than trusting the style.
    const auto opacity = util::clamp(evaluated.get<FillExtrusionShadowOpacity>(), 0.0f, 1.0f);
    const auto blurRadius = std::max(evaluated.get<FillExtrusionShadowBlur>(), 0.0f);

    // The mask is rendered at half viewport dimensions, so one logical point is half a mask texel.
    // Clamp to one texel so a zero radius still samples the centre tap cleanly instead of
    // collapsing every tap onto the same texel.
    const float blurScale = std::max(blurRadius * 0.5f, 1.0f) / outermostTapOffset;

    const float texelStepX = maskSize.width > 0 ? 1.0f / static_cast<float>(maskSize.width) : 0.0f;
    const float texelStepY = maskSize.height > 0 ? 1.0f / static_cast<float>(maskSize.height) : 0.0f;

    const FillExtrusionShadowPropsUBO propsUBO = {.color = evaluated.get<FillExtrusionShadowColor>(),
                                                  .texel_step = {texelStepX, texelStepY},
                                                  .blur_scale = blurScale,
                                                  .opacity = opacity,
                                                  // Only the mask stage reads these.
                                                  .base = 0.0f,
                                                  .height = 0.0f,
                                                  .pad1 = 0,
                                                  .pad2 = 0};

    auto& layerUniforms = layerGroup.mutableUniformBuffers();
    layerUniforms.createOrUpdate(idFillExtrusionShadowPropsUBO, &propsUBO, parameters.context);
}

} // namespace mln
