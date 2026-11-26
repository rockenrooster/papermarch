// The world with its glow, a colour grade (the world's light at the same brightness, contrast, saturation) and a
// vignette that leaves the HUD's top row alone.
in highp vec2 vUV;
uniform sampler2D uTex;    // the world
uniform sampler2D uBloom;  // the blurred bright pass
uniform mediump vec4 uPost;   // x bloom strength, y vignette, z grade strength, w width / height
uniform mediump vec4 uLight;  // rgb the grade's colour
uniform mediump vec4 uGrade;  // x contrast, y saturation, z the HUD's share of the height at the top (no vignette)
uniform mediump vec4 uEdge;   // rgb a flash at the screen's edges (a lost gate), a its strength
layout(location = 0) out vec4 oColor;
void main() {
    vec3 c = texture(uTex, vUV).rgb;
    c += texture(uBloom, vUV).rgb * uPost.x;
    float lum = dot(uLight.rgb, vec3(0.299, 0.587, 0.114));
    c *= mix(vec3(1.0), uLight.rgb / max(lum, 0.05), uPost.z);
    c = (c - 0.5) * uGrade.x + 0.5;
    c = mix(vec3(dot(c, vec3(0.299, 0.587, 0.114))), c, uGrade.y);
    vec2 q = (vUV - 0.5) * vec2(uPost.w, 1.0) * 2.0;
    float hud = 1.0 - smoothstep(1.0 - uGrade.z - 0.05, 1.0 - uGrade.z, vUV.y);
    c *= 1.0 - uPost.y * smoothstep(0.55, 1.05, length(q)) * hud;
    vec2 e = abs(vUV - 0.5) * 2.0;
    c = mix(c, uEdge.rgb, uEdge.a * smoothstep(0.55, 1.0, max(e.x, e.y)));
    oColor = vec4(clamp(c, 0.0, 1.0), 1.0);
}
