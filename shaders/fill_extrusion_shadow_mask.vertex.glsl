layout (location = 0) in vec2 a_pos;
layout (location = 1) in vec2 a_decimals_ed;

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

#pragma mapbox: define highp float height

void main() {
    #pragma mapbox: initialize highp float height

    height = max(0.0, height);

    // Roof triangles all sit at the top of the extrusion, exactly as the main FillExtrusionShader
    // does with its hardcoded t = 1.0.
    float z = height;
    vec2 decimals = unpack_float(floor(a_decimals_ed.x / 2.0)) / 128.0;
    vec2 p = a_pos + decimals;

    // Shear the vertex along the light ray onto the ground plane. This is the exact ground
    // projection of the point (p, z), not an approximation.
    gl_Position = u_matrix * vec4(p + u_offset_per_meter * z, 0.0, 1.0);
}
