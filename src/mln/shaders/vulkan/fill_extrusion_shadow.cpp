#include <mln/shaders/vulkan/fill_extrusion_shadow.hpp>
#include <mln/shaders/shader_defines.hpp>
#include <mln/shaders/fill_extrusion_shadow_layer_ubo.hpp>

namespace mln {
namespace shaders {

//
// Fill extrusion shadow mask (roofs)

using FillExtrusionShadowMaskShaderSource =
    ShaderSource<BuiltIn::FillExtrusionShadowMaskShader, gfx::Backend::Type::Vulkan>;

const std::array<AttributeInfo, 4> FillExtrusionShadowMaskShaderSource::attributes = {
    AttributeInfo{0, gfx::AttributeDataType::Short2, idFillExtrusionShadowPosVertexAttribute},
    AttributeInfo{1, gfx::AttributeDataType::UShort2, idFillExtrusionShadowDecimalsEdAttribute},

    // Data driven
    AttributeInfo{2, gfx::AttributeDataType::Float, idFillExtrusionShadowBaseVertexAttribute},
    AttributeInfo{3, gfx::AttributeDataType::Float, idFillExtrusionShadowHeightVertexAttribute},
};
const std::array<TextureInfo, 0> FillExtrusionShadowMaskShaderSource::textures = {};

//
// Fill extrusion shadow mask, instanced (walls)

using FillExtrusionShadowMaskInstancedShaderSource =
    ShaderSource<BuiltIn::FillExtrusionShadowMaskInstancedShader, gfx::Backend::Type::Vulkan>;

const std::array<AttributeInfo, 1> FillExtrusionShadowMaskInstancedShaderSource::attributes = {
    AttributeInfo{0, gfx::AttributeDataType::Short2, idFillExtrusionShadowPosVertexAttribute},
};
const std::array<AttributeInfo, 4> FillExtrusionShadowMaskInstancedShaderSource::instanceAttributes = {
    // The shader also reads this same buffer directly as an OutlineInstance SSBO, so the two
    // entries below must stay adjacent and layout-identical to FillExtrusionLayoutVertex.
    AttributeInfo{1, gfx::AttributeDataType::Short2, idFillExtrusionShadowOutlinePosAttribute, idFillExtrusionInstanced},
    AttributeInfo{2, gfx::AttributeDataType::UShort2, idFillExtrusionShadowDecimalsEdAttribute, idFillExtrusionInstanced},

    // Data driven
    AttributeInfo{3, gfx::AttributeDataType::Float, idFillExtrusionShadowBaseVertexAttribute},
    AttributeInfo{4, gfx::AttributeDataType::Float, idFillExtrusionShadowHeightVertexAttribute},
};
const std::array<TextureInfo, 0> FillExtrusionShadowMaskInstancedShaderSource::textures = {};

//
// Fill extrusion shadow blur (horizontal)

using FillExtrusionShadowBlurShaderSource =
    ShaderSource<BuiltIn::FillExtrusionShadowBlurShader, gfx::Backend::Type::Vulkan>;

const std::array<AttributeInfo, 1> FillExtrusionShadowBlurShaderSource::attributes = {
    AttributeInfo{0, gfx::AttributeDataType::Short2, idFillExtrusionShadowPosVertexAttribute},
};
const std::array<TextureInfo, 1> FillExtrusionShadowBlurShaderSource::textures = {
    TextureInfo{0, idFillExtrusionShadowImageTexture},
};

//
// Fill extrusion shadow composite (vertical blur + colourise)

using FillExtrusionShadowShaderSource = ShaderSource<BuiltIn::FillExtrusionShadowShader, gfx::Backend::Type::Vulkan>;

const std::array<AttributeInfo, 1> FillExtrusionShadowShaderSource::attributes = {
    AttributeInfo{0, gfx::AttributeDataType::Short2, idFillExtrusionShadowPosVertexAttribute},
};
const std::array<TextureInfo, 1> FillExtrusionShadowShaderSource::textures = {
    TextureInfo{0, idFillExtrusionShadowImageTexture},
};

} // namespace shaders
} // namespace mln
