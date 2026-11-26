// The wooden table under the page (GAME_DESIGN 9.1): lighter in the middle, plank lines, streaks of grain.
in highp vec2 vUV;
uniform highp vec4 uTableA;     // rgb, the middle
uniform highp vec4 uTableB;     // rgb, the far ends
uniform highp vec4 uTableLine;  // rgb, w planks across the screen
layout(location = 0) out vec4 oColor;
void main() {
    highp vec2 px = gl_FragCoord.xy;
    float y = px.y * uScreen.w;
    float t = clamp(abs(y - 0.55) * 1.6, 0.0, 1.0);
    vec3 c = mix(uTableA.rgb, uTableB.rgb, t);
    highp float x = px.x * uScreen.z * uTableLine.w;
    float f = fract(x);
    float w = 2.5 * uScreen.z * uTableLine.w;
    float line = 1.0 - smoothstep(0.0, w, min(f, 1.0 - f));
    c = mix(c, uTableLine.rgb, line * 0.8);
    highp float streak = fract(sin(floor(px.x * 0.45) * 91.7 + floor(x) * 7.3 + floor(px.y * 0.012) * 13.1) * 43758.5);
    c *= 0.95 + 0.07 * streak;
    oColor = vec4(c, 1.0);
}
