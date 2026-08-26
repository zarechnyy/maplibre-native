in vec2 v_pos;
uniform sampler2D u_image;

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

// A 9-tap Gaussian collapsed into 5 bilinear samples by exploiting linear filtering: each of the
// outer taps reads two texels at once from a fractional offset.
const float FES_W0 = 0.2270270270;
const float FES_W1 = 0.3162162162;
const float FES_W2 = 0.0702702703;
const float FES_O1 = 1.3846153846;
const float FES_O2 = 3.2307692308;

float fesBlur(vec2 uv, vec2 step) {
    float v = texture(u_image, uv).r * FES_W0;
    v += (texture(u_image, uv + step * FES_O1).r + texture(u_image, uv - step * FES_O1).r) * FES_W1;
    v += (texture(u_image, uv + step * FES_O2).r + texture(u_image, uv - step * FES_O2).r) * FES_W2;
    return v;
}

void main() {
#ifdef OVERDRAW_INSPECTOR
    fragColor = vec4(1.0);
    return;
#endif

    // Second half of the separable blur, folded in here so it costs no extra render target.
    vec2 step = vec2(0.0, u_texel_step.y) * u_blur_scale;
    float mask = clamp(fesBlur(v_pos, step), 0.0, 1.0);

    // The composite drawable is created with gfx::ColorMode::alphaBlended(), i.e. (One,
    // OneMinusSrcAlpha) -- premultiplied. mln::Color is already premultiplied, so scaling the
    // whole vector keeps rgb == straight_rgb * out_alpha.
    fragColor = u_color * (mask * u_opacity);
}
