// The page: the baked texture (premultiplied) with paper grain (GAME_DESIGN 9.1).
in highp vec2 vUV;
in highp vec2 vPx;
uniform sampler2D uTex;
uniform highp vec4 uPage;  // x grain strength, y opacity
layout(location = 0) out vec4 oColor;
void main() {
    vec4 c = texture(uTex, vUV);
    highp vec2 cell = floor(vPx);
    float g = fract(sin(dot(cell, vec2(12.9898, 78.233))) * 43758.5453) - 0.5;
    c.rgb *= 1.0 + g * uPage.x;
    oColor = c * uPage.y;
}
