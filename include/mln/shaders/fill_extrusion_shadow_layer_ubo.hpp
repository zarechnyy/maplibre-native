#pragma once

#include <mln/shaders/layer_ubo.hpp>

#include <array>

namespace mln {
namespace shaders {

/// Per-drawable data for the ground-shadow mask pass.
///
/// Consolidated into an SSBO array under MLN_UBO_CONSOLIDATION and indexed by idGlobalUBOIndex,
/// exactly like FillExtrusionDrawableUBO.
struct alignas(16) FillExtrusionShadowDrawableUBO {
    /*  0 */ std::array<float, 4 * 4> matrix;
    /// Tile units of horizontal shear per metre of vertex z. This already folds in the shadow
    /// length, the azimuth direction and the metres-to-tile-units conversion, which depends on
    /// the tile's zoom level and so cannot live in the layer-wide UBO below.
    /* 64 */ std::array<float, 2> offset_per_meter;

    // Interpolations
    /* 72 */ float base_t;
    /* 76 */ float height_t;
    /* 80 */
};
static_assert(sizeof(FillExtrusionShadowDrawableUBO) == 5 * 16);

/// Evaluated shadow properties that do not depend on the tile. Shared verbatim by the mask, blur
/// and composite stages, so there is one struct, one Metal mirror, and one id to bind.
///
/// APPEND-ONLY: to add a field, consume a `pad` slot below or append a whole new 16-byte block.
/// Never insert in the middle and never reorder -- the Metal mirror in
/// include/mln/shaders/mtl/fill_extrusion_shadow.hpp must stay byte-identical, and both it and
/// the static_assert have to be updated in the same commit.
struct alignas(16) FillExtrusionShadowPropsUBO {
    /*  0 */ Color color;
    /// Reciprocal of the mask texture size, i.e. one texel step for the separable blur.
    /* 16 */ std::array<float, 2> texel_step;
    /* 24 */ float blur_scale;
    /* 28 */ float opacity;

    // Uniform (non-data-driven) fill-extrusion geometry, used when the corresponding
    // HAS_UNIFORM_u_* permutation is compiled. These must match the values the fill-extrusion
    // shaders themselves use, or the shadow will not line up with the building.
    /* 32 */ float base;
    /* 36 */ float height;
    /* 40 */ float pad1;
    /* 44 */ float pad2;
    /* 48 */
};
static_assert(sizeof(FillExtrusionShadowPropsUBO) == 3 * 16);

} // namespace shaders
} // namespace mln
