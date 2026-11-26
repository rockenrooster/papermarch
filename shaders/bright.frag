// Bloom's bright pass: a 4x4 average of the world (four bilinear taps) at a quarter of the size, keeping what is
// brighter than the threshold.
in highp vec2 vUV;
uniform sampler2D uTex;
uniform highp vec4 uBright;  // xy one source pixel, z the threshold
layout(location = 0) out vec4 oColor;
void main() {
    highp vec2 d = uBright.xy;
    vec3 c = (texture(uTex, vUV + vec2(-d.x, -d.y)).rgb + texture(uTex, vUV + vec2(d.x, -d.y)).rgb +
              texture(uTex, vUV + vec2(-d.x, d.y)).rgb + texture(uTex, vUV + vec2(d.x, d.y)).rgb) * 0.25;
    float l = max(c.r, max(c.g, c.b));
    float k = max(l - uBright.z, 0.0) / max(1.0 - uBright.z, 0.01);
    oColor = vec4(c * min(k, 1.0), 1.0);
}
