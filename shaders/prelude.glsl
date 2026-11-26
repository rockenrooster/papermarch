// Per-frame values shared by every program (TECH_SPEC 5). Members are highp so both stages agree on precision.
layout(std140) uniform Frame {
    highp mat4 uViewProj;
    highp vec4 uCamRight;     // xyz camera right
    highp vec4 uCamUp;        // xyz camera up
    highp vec4 uCamPos;       // xyz eye; w = lane length
    highp vec4 uTime;         // x seconds, y tick interpolation alpha, z cheer 0..1, w seconds wrapped to 60
    highp vec4 uGrade;        // x brighten, y desaturate, z fade (1 = visible), w vignette strength
    highp vec4 uScreen;       // xy frame size in pixels, zw reciprocals
    highp vec4 uLane;         // x half-width, y length, z light-map u scale, w light-map v scale
    highp vec4 uFog;          // rgb far colour, a strength
    highp vec4 uBackdrop[4];  // far to near
    highp vec4 uLaneFar;
    highp vec4 uLaneNear;
    highp vec4 uRail;
    highp vec4 uBrighten;     // rgb target of the win's dawn
    highp vec4 uWisp[3];      // body, light, rim
    highp vec4 uGloom[4];     // body, light, rim, eyes
    highp vec4 uGood;
    highp vec4 uBad;
    highp vec4 uSpire;        // rgb crack glow, a heart pulse 0..1
    highp vec4 uParams;       // x glow strength, y unit glow alpha, z light-map alpha, w hop height
    highp vec4 uParams2;      // x hop rate, y light radius, z lane light add, w unit glow size (radii)
    highp vec4 uSpireScreen;  // xy spire top on screen (0..1, y up), z glow radius, w the muzzle's z (sim::kMuzzleZ)
};

#ifndef GT_FRAG
// A wisp leaves the lantern's lens (1.4 m up at the muzzle) and drops to the lane in an arc that lands 4.5 m on: its
// height above its place on the lane, from its z. Flat at the lens, so the shot is seen to leave the barrel's tip.
float muzzleLift(float z) {
    float d = max(z - uSpireScreen.w, 0.0) / 4.5;
    return d < 1.0 ? 1.13 * (1.0 - d * d) : 0.0;
}
#endif

#ifdef GT_FRAG
// Colour grade, applied by every scene shader: dawn, loss of colour, fade, vignette (TECH_SPEC 5.3).
vec3 grade(vec3 c) {
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    c = mix(c, vec3(l), uGrade.y);
    c = c + (vec3(1.0) - c) * uBrighten.rgb * (0.55 * uGrade.x);
    highp vec2 q = gl_FragCoord.xy * uScreen.zw - 0.5;
    float v = 1.0 - uGrade.w * dot(q, q) * 1.7;
    return c * clamp(v, 0.0, 1.0) * uGrade.z;
}
// +-half a level of noise so 8-bit gradients never band.
float dither() {
    highp vec2 p = gl_FragCoord.xy;
    return (fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453) - 0.5) / 255.0;
}
#endif
