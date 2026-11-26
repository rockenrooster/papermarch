// Every standing thing's shadow at once (TECH_SPEC 5.4): the union of their silhouettes (a max-blended coverage
// target), laid over the page as one soft dark layer, so overlapping shadows never darken twice.
in highp vec2 vUV;
uniform sampler2D uTex;
uniform highp vec4 uShadow;  // rgb colour, a strength
layout(location = 0) out vec4 oColor;
void main() {
    float a = texture(uTex, vUV).r * uShadow.a;
    oColor = vec4(uShadow.rgb * a, a);
}
