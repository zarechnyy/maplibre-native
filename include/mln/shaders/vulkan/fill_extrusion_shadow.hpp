#pragma once

#include <mln/shaders/shader_source.hpp>
#include <mln/shaders/vulkan/shader_program.hpp>

namespace mln {
namespace shaders {

/// Shared by all four shadow stages. The struct fields here must stay byte-identical to the
/// canonical C++ ones in include/mln/shaders/fill_extrusion_shadow_layer_ubo.hpp (see that
/// header for the full cross-backend sync requirement).
constexpr auto fillExtrusionShadowShaderPrelude = R"(

#define idFillExtrusionShadowDrawableUBO            idDrawableReservedVertexOnlyUBO
// Must be `layerUBOStartId`. shader_defines.hpp's getEnumValue() resolves every per-layer "props"
// UBO id (idFillExtrusionPropsUBO, idHeatmapEvaluatedPropsUBO, etc.) to `layerUBOStartId` when
// MLN_RENDER_BACKEND_VULKAN is set, and that's the id the cross-backend tweaker writes its data
// to. This local binding number has to match it exactly. Getting it wrong doesn't fail to
// compile or trip a validation layer; it just reads whatever unrelated data happens to be at the
// wrong binding, so the shadow silently renders as nothing.
#define idFillExtrusionShadowPropsUBO               layerUBOStartId
// Purely a local Vulkan convention for the mask-instanced shader's OutlineInstance SSBO -- unlike
// the id above, this one has no generic shader_defines.hpp counterpart to stay in sync with, so
// reusing drawableSSBOStartId (as plain FillExtrusionInstancedShader also does) is fine as-is.
#define idFillExtrusionShadowInstancedDrawableUBO   drawableSSBOStartId

// A 9-tap Gaussian collapsed into 5 bilinear samples by exploiting linear filtering: each of the
// outer taps reads two texels at once from a fractional offset.
#define FES_W0 0.2270270270
#define FES_W1 0.3162162162
#define FES_W2 0.0702702703
#define FES_O1 1.3846153846
#define FES_O2 3.2307692308

float fesBlur(sampler2D img, vec2 uv, vec2 blurStep) {
    float v = texture(img, uv).r * FES_W0;
    v += (texture(img, uv + blurStep * FES_O1).r + texture(img, uv - blurStep * FES_O1).r) * FES_W1;
    v += (texture(img, uv + blurStep * FES_O2).r + texture(img, uv - blurStep * FES_O2).r) * FES_W2;
    return v;
}

// Maps the unit-square static quad straight to clip space for an offscreen render target, with a
// hardcoded y-flip (Vulkan NDC convention). There is no device-rotation term here because an
// offscreen render target has no surface orientation to compensate for -- passes that draw into
// the main frame use applySurfaceTransform() instead, same as every other on-screen shader.
vec4 fesFullscreenPosition(vec2 p) {
    return vec4(p.x * 2.0 - 1.0, 1.0 - p.y * 2.0, 0.0, 1.0);
}

)";

//
// Shadow mask, roof triangles (non-instanced)

template <>
struct ShaderSource<BuiltIn::FillExtrusionShadowMaskShader, gfx::Backend::Type::Vulkan> {
    static constexpr const char* name = "FillExtrusionShadowMaskShader";

    static const std::array<AttributeInfo, 4> attributes;
    static constexpr std::array<AttributeInfo, 0> instanceAttributes{};
    static const std::array<TextureInfo, 0> textures;

    static constexpr auto prelude = fillExtrusionShadowShaderPrelude;
    static constexpr auto vertex = R"(

layout(location = 0) in ivec2 in_position;
layout(location = 1) in uvec2 in_decimals_ed;

#if !defined(HAS_UNIFORM_u_base)
layout(location = 2) in vec2 in_base;
#endif
#if !defined(HAS_UNIFORM_u_height)
layout(location = 3) in vec2 in_height;
#endif

layout(push_constant) uniform Constants {
    int ubo_index;
} constant;

struct FillExtrusionShadowDrawableUBO {
    mat4 matrix;
    vec2 offset_per_meter;
    float base_t;
    float height_t;
};

layout(std140, set = LAYER_SET_INDEX, binding = idFillExtrusionShadowDrawableUBO) readonly buffer FillExtrusionShadowDrawableUBOVector {
    FillExtrusionShadowDrawableUBO drawable_ubo[];
} drawableVector;

layout(set = LAYER_SET_INDEX, binding = idFillExtrusionShadowPropsUBO) uniform FillExtrusionShadowPropsUBO {
    vec4 color;
    vec2 texel_step;
    float blur_scale;
    float opacity;
    float base;
    float height;
    float pad1;
    float pad2;
} props;

void main() {
    const FillExtrusionShadowDrawableUBO drawable = drawableVector.drawable_ubo[constant.ubo_index];

#if defined(HAS_UNIFORM_u_height)
    const float height = props.height;
#else
    const float height = max(unpack_mix_float(in_height, drawable.height_t), 0.0);
#endif

    // Roof triangles all sit at the top of the extrusion, exactly as FillExtrusionShader does with
    // its hardcoded t = 1.0.
    const float z = height;
    const vec2 decimals = unpack_float(floor(in_decimals_ed.x / 2)) / 128.0;
    const vec2 p = vec2(in_position) + decimals;

    // Shear the vertex along the light ray onto the ground plane. This is the exact ground
    // projection of the point (p, z), not an approximation.
    gl_Position = drawable.matrix * vec4(p + drawable.offset_per_meter * z, 0.0, 1.0);
    gl_Position.y *= -1.0;
}
)";

    static constexpr auto fragment = R"(

layout(location = 0) out vec4 out_color;

void main() {
    // A binary silhouette. Blending is off, so overlapping geometry cannot compound.
    out_color = vec4(1.0, 0.0, 0.0, 1.0);
}
)";
};

//
// Shadow mask, wall quads (instanced)

template <>
struct ShaderSource<BuiltIn::FillExtrusionShadowMaskInstancedShader, gfx::Backend::Type::Vulkan> {
    static constexpr const char* name = "FillExtrusionShadowMaskInstancedShader";

    static const std::array<AttributeInfo, 1> attributes;
    static const std::array<AttributeInfo, 4> instanceAttributes;
    static const std::array<TextureInfo, 0> textures;

    static constexpr auto prelude = fillExtrusionShadowShaderPrelude;
    static constexpr auto vertex = R"(

// Static unit quad: x in {0,1} selects p1 or p2, y in {0,1} is the lower/upper ring.
layout(location = 0) in ivec2 in_position;

#if !defined(HAS_UNIFORM_u_base)
layout(location = 3) in vec2 in_base;
#endif
#if !defined(HAS_UNIFORM_u_height)
layout(location = 4) in vec2 in_height;
#endif

layout(push_constant) uniform Constants {
    int ubo_index;
} constant;

struct FillExtrusionShadowDrawableUBO {
    mat4 matrix;
    vec2 offset_per_meter;
    float base_t;
    float height_t;
};

layout(std140, set = LAYER_SET_INDEX, binding = idFillExtrusionShadowDrawableUBO) readonly buffer FillExtrusionShadowDrawableUBOVector {
    FillExtrusionShadowDrawableUBO drawable_ubo[];
} drawableVector;

layout(set = LAYER_SET_INDEX, binding = idFillExtrusionShadowPropsUBO) uniform FillExtrusionShadowPropsUBO {
    vec4 color;
    vec2 texel_step;
    float blur_scale;
    float opacity;
    float base;
    float height;
    float pad1;
    float pad2;
} props;

// Must stay layout-identical to FillExtrusionLayoutVertex.
struct OutlineInstance {
    int pos;
    uint decimals_ed;
};

layout(std430, set = DRAWABLE_UBO_SET_INDEX, binding = idFillExtrusionShadowInstancedDrawableUBO) readonly buffer FillExtrusionShadowInstanceVector {
    OutlineInstance instance[];
} instanceVector;

void main() {
    const vec2 instanceDecimalsEd = unpack_uint(instanceVector.instance[gl_InstanceIndex].decimals_ed);

    // The low bit of decimals_ed.x marks the last vertex of each ring. Skipping it also guards the
    // instance[gl_InstanceIndex + 1] read below from running off the end of the ring.
    bool isDiscarded = mod(instanceDecimalsEd.x, 2.0) > 0.0;
    if (isDiscarded) {
        gl_Position = vec4(0.0);
        return;
    }

    const FillExtrusionShadowDrawableUBO drawable = drawableVector.drawable_ubo[constant.ubo_index];

#if defined(HAS_UNIFORM_u_base)
    const float base = props.base;
#else
    const float base = max(unpack_mix_float(in_base, drawable.base_t), 0.0);
#endif
#if defined(HAS_UNIFORM_u_height)
    const float height = props.height;
#else
    const float height = max(unpack_mix_float(in_height, drawable.height_t), 0.0);
#endif

    const vec2 instancePos = unpack_int(instanceVector.instance[gl_InstanceIndex].pos);
    const vec2 nextInstancePos = unpack_int(instanceVector.instance[gl_InstanceIndex + 1].pos);
    const vec2 nextInstanceDecimalsEd = unpack_uint(instanceVector.instance[gl_InstanceIndex + 1].decimals_ed);

    const vec2 p1 = instancePos + unpack_float(floor(instanceDecimalsEd.x / 2)) / 128.0;
    const vec2 p2 = nextInstancePos + unpack_float(floor(nextInstanceDecimalsEd.x / 2)) / 128.0;

    const float t = float(in_position.y);

    // Shear by the vertex's own z, so a non-zero fill-extrusion-base correctly lifts the shadow's
    // near edge instead of anchoring it under the building.
    const float z = (t != 0.0) ? height : base;
    const vec2 p = (in_position.x == 0) ? p1 : p2;

    gl_Position = drawable.matrix * vec4(p + drawable.offset_per_meter * z, 0.0, 1.0);
    gl_Position.y *= -1.0;
}
)";

    static constexpr auto fragment = R"(

layout(location = 0) out vec4 out_color;

void main() {
    out_color = vec4(1.0, 0.0, 0.0, 1.0);
}
)";
};

//
// Shadow blur, horizontal pass into a second render target

template <>
struct ShaderSource<BuiltIn::FillExtrusionShadowBlurShader, gfx::Backend::Type::Vulkan> {
    static constexpr const char* name = "FillExtrusionShadowBlurShader";

    static const std::array<AttributeInfo, 1> attributes;
    static constexpr std::array<AttributeInfo, 0> instanceAttributes{};
    static const std::array<TextureInfo, 1> textures;

    static constexpr auto prelude = fillExtrusionShadowShaderPrelude;
    // Draws into an offscreen RenderTarget, not the swapchain, so the vertex shader bypasses the
    // view/projection matrix and applySurfaceTransform() entirely -- see fesFullscreenPosition().
    static constexpr auto vertex = R"(

layout(location = 0) in ivec2 in_position;
layout(location = 0) out vec2 uv;

void main() {
    const vec2 p = vec2(in_position);
    gl_Position = fesFullscreenPosition(p);
    uv = p;
}
)";

    static constexpr auto fragment = R"(

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;

layout(set = LAYER_SET_INDEX, binding = idFillExtrusionShadowPropsUBO) uniform FillExtrusionShadowPropsUBO {
    vec4 color;
    vec2 texel_step;
    float blur_scale;
    float opacity;
    float base;
    float height;
    float pad1;
    float pad2;
} props;

layout(set = DRAWABLE_IMAGE_SET_INDEX, binding = 0) uniform sampler2D image_sampler;

void main() {
    const vec2 blurStep = vec2(props.texel_step.x, 0.0) * props.blur_scale;
    out_color = vec4(fesBlur(image_sampler, uv, blurStep), 0.0, 0.0, 1.0);
}
)";
};

//
// Shadow composite: the vertical blur pass fused with colourisation, drawn in the main pass

template <>
struct ShaderSource<BuiltIn::FillExtrusionShadowShader, gfx::Backend::Type::Vulkan> {
    static constexpr const char* name = "FillExtrusionShadowShader";

    static const std::array<AttributeInfo, 1> attributes;
    static constexpr std::array<AttributeInfo, 0> instanceAttributes{};
    static const std::array<TextureInfo, 1> textures;

    static constexpr auto prelude = fillExtrusionShadowShaderPrelude;
    // Unlike the mask/blur passes, this draws into the main on-screen frame at the building's
    // layer index, so (unlike fesFullscreenPosition) it must go through applySurfaceTransform()
    // to get the same device-rotation/NDC handling as every other main-pass drawable.
    static constexpr auto vertex = R"(

layout(location = 0) in ivec2 in_position;
layout(location = 0) out vec2 uv;

void main() {
    const vec2 p = vec2(in_position);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
    applySurfaceTransform();
    uv = p;
}
)";

    static constexpr auto fragment = R"(

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;

layout(set = LAYER_SET_INDEX, binding = idFillExtrusionShadowPropsUBO) uniform FillExtrusionShadowPropsUBO {
    vec4 color;
    vec2 texel_step;
    float blur_scale;
    float opacity;
    float base;
    float height;
    float pad1;
    float pad2;
} props;

layout(set = DRAWABLE_IMAGE_SET_INDEX, binding = 0) uniform sampler2D image_sampler;

void main() {

#if defined(OVERDRAW_INSPECTOR)
    out_color = vec4(1.0);
    return;
#endif

    // Second half of the separable blur, folded in here so it costs no extra render target.
    const vec2 blurStep = vec2(0.0, props.texel_step.y) * props.blur_scale;
    const float mask = clamp(fesBlur(image_sampler, uv, blurStep), 0.0, 1.0);

    // The composite drawable is created with gfx::ColorMode::alphaBlended(), i.e. (One,
    // OneMinusSrcAlpha) -- premultiplied. mln::Color is already premultiplied, so scaling the
    // whole vector keeps rgb == straight_rgb * out_alpha.
    out_color = props.color * (mask * props.opacity);
}
)";
};

} // namespace shaders
} // namespace mln
