// Static unit quad: x in {0,1} selects p1 or p2, y in {0,1} is the lower/upper ring.
layout (location = 0) in vec2 a_pos;

// GLES has no SSBO-style manual instance indexing, so unlike Vulkan (which read
// outline[gl_InstanceIndex] and outline[gl_InstanceIndex + 1] from one buffer in-shader), this and
// the next attribute below are supplied directly as ordinary per-instance vertex attributes.
layout (location = 1) in vec2 a_outline_pos;
layout (location = 2) in vec2 a_decimals_ed;
layout (location = 3) in vec2 a_next_outline_pos;
layout (location = 4) in vec2 a_next_decimals_ed;

layout (std140) uniform FillExtrusionShadowDrawableUBO {
    highp mat4 u_matrix;
    highp vec2 u_offset_per_meter;
    highp float u_base_t;
    highp float u_height_t;
};

layout (std140) uniform FillExtrusionShadowPropsUBO {
    highp vec4 u_color;
    highp vec2 u_texel_step;
    highp float u_blur_scale;
    highp float u_opacity;
    highp float u_base;
    highp float u_height;
    lowp float props_pad1;
    lowp float props_pad2;
};

#pragma mapbox: define highp float base
#pragma mapbox: define highp float height

void main() {
    #pragma mapbox: initialize highp float base
    #pragma mapbox: initialize highp float height

    // The low bit of decimals_ed.x marks the last vertex of each ring: the next-vertex attributes
    // aren't meaningful for it (they'd belong to an unrelated ring), so skip this instance.
    bool isDiscarded = mod(a_decimals_ed.x, 2.0) > 0.0;
    if (isDiscarded) {
        gl_Position = vec4(0.0);
        return;
    }

    base = max(0.0, base);
    height = max(0.0, height);

    vec2 p1 = a_outline_pos + unpack_float(floor(a_decimals_ed.x / 2.0)) / 128.0;
    vec2 p2 = a_next_outline_pos + unpack_float(floor(a_next_decimals_ed.x / 2.0)) / 128.0;

    float t = a_pos.y;

    // Shear by the vertex's own z, so a non-zero fill-extrusion-base correctly lifts the shadow's
    // near edge instead of anchoring it under the building.
    float z = t != 0.0 ? height : base;
    vec2 p = a_pos.x == 0.0 ? p1 : p2;

    gl_Position = u_matrix * vec4(p + u_offset_per_meter * z, 0.0, 1.0);
}
