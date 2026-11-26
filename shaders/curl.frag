// The turning page: its printed side shaded as it bends away, its back plain paper.
in highp vec2 vTex;
in highp float vTheta;
uniform sampler2D uTex;
uniform highp vec4 uBack;  // rgb the paper's back
layout(location = 0) out vec4 oColor;
void main() {
    if (vTex.x < 0.0 || vTex.x > 1.0 || vTex.y < 0.0 || vTex.y > 1.0) discard;
    vec4 c = texture(uTex, vTex);
    if (c.a < 0.01) discard;
    if (vTheta > 1.5707963) {
        float shade = 0.80 + 0.2 * clamp((vTheta - 1.5707963) / 1.5707963, 0.0, 1.0);
        c = vec4(uBack.rgb * shade * c.a, c.a);
    } else {
        c.rgb *= 1.0 - 0.38 * sin(vTheta);
    }
    oColor = c;
}
