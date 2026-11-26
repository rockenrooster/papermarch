// Bloom's blur: one direction of a 9-tap Gaussian in five bilinear taps.
in highp vec2 vUV;
uniform sampler2D uTex;
uniform highp vec4 uDir;  // xy one pixel along the blur (times its spread)
layout(location = 0) out vec4 oColor;
void main() {
    highp vec2 d1 = uDir.xy * 1.3846154, d2 = uDir.xy * 3.2307692;
    vec3 c = texture(uTex, vUV).rgb * 0.2270270;
    c += (texture(uTex, vUV + d1).rgb + texture(uTex, vUV - d1).rgb) * 0.3162162;
    c += (texture(uTex, vUV + d2).rgb + texture(uTex, vUV - d2).rgb) * 0.0702703;
    oColor = vec4(c, 1.0);
}
