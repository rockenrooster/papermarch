// The road (2026-11-26): each pixel's ray from the camera meets the ground; there it is paving stones, a curb, the verge,
// a finish line; haze far away. One full-screen pass, no textures.
in highp vec2 vUV;
uniform highp vec4 uCam0;     // principal point x, y (px, top-left origin), focal length (px), camera height
uniform highp vec4 uCam1;     // sin and cos of the pitch, the camera's z, the finish line's z (< 0: none)
uniform mediump vec4 uStoneA; // rgb; w the road's half width
uniform mediump vec4 uStoneB; // rgb
uniform mediump vec4 uMortar; // rgb
uniform mediump vec4 uCurb;   // rgb
uniform mediump vec4 uGrassA; // rgb
uniform mediump vec4 uGrassB; // rgb; w the verge's speckle (petals, pebbles) strength
uniform mediump vec4 uHaze;   // rgb; w the depth where the haze begins
layout(location = 0) out vec4 oColor;

highp float hash21(highp vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}
float vnoise(highp vec2 p) {
    highp vec2 i = floor(p);
    highp vec2 f = fract(p);
    float a = hash21(i), b = hash21(i + vec2(1.0, 0.0)), c = hash21(i + vec2(0.0, 1.0)), d = hash21(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

void main() {
    highp vec2 px = vec2(gl_FragCoord.x, uScreen.y - gl_FragCoord.y);
    highp float k = (px.y - uCam0.y) / uCam0.z;
    highp float sn = uCam1.x, cs = uCam1.y, h = uCam0.w;
    highp float den = k * cs + sn;
    if (den < 0.003) {
        oColor = vec4(uHaze.rgb, 1.0);
        return;
    }
    highp float t = h * (cs - k * sn) / den;
    highp float depth = h * sn + t * cs;
    highp float x = (px.x - uCam0.x) * depth / uCam0.z;
    highp float z = uCam1.z + t;
    highp float aa = max(fwidth(x), fwidth(z));  // road units per pixel here
    float far = smoothstep(0.08, 0.3, aa);       // the pattern's lines fade before they shimmer
    float half_ = uStoneA.w;
    float ax = abs(x);
    vec3 c;
    if (ax < half_) {
        // Paving stones: rows across the road, every other row shifted by half a stone.
        const float rowL = 0.9, stoneW = 1.25;
        highp float row = floor(z / rowL);
        highp float sx = x + mod(row, 2.0) * stoneW * 0.5;
        highp float col = floor(sx / stoneW);
        float fz = fract(z / rowL), fx = fract(sx / stoneW);
        float h1 = hash21(vec2(row, col)), h2 = hash21(vec2(col * 1.7, row * 3.1));
        vec3 stone = mix(uStoneA.rgb, uStoneB.rgb, h1 * 0.85) * (0.95 + 0.08 * h2);
        float lit = 0.09 * smoothstep(0.72, 1.0, fz) - 0.08 * smoothstep(0.28, 0.0, fz) + 0.04 * smoothstep(0.25, 0.0, fx) -
                    0.04 * smoothstep(0.75, 1.0, fx);
        stone *= 1.0 + lit * (1.0 - far);
        float e = min(min(fx, 1.0 - fx) * stoneW, min(fz, 1.0 - fz) * rowL);
        float mortar = (1.0 - smoothstep(0.045 - aa * 0.5, 0.045 + aa * 0.5, e)) * (1.0 - far);
        vec3 avg = mix(uStoneA.rgb, uStoneB.rgb, 0.45) * 0.93;
        c = mix(mix(stone, uMortar.rgb, mortar), avg, far * 0.85);
        // A worn, lighter path down the middle.
        float wear = (1.0 - smoothstep(1.6, 3.4, ax)) * (0.6 + 0.4 * vnoise(vec2(x * 0.8, z * 0.25)));
        c = mix(c, c * 1.08 + 0.02, wear * 0.5);
        // The finish line: a band of checks.
        if (uCam1.w > 0.0) {
            highp float d = z - uCam1.w;
            if (abs(d) < 0.75) {
                float check = mod(floor(x / 0.75) + floor((d + 0.75) / 0.75), 2.0);
                c = mix(vec3(0.97, 0.96, 0.93), vec3(0.16, 0.15, 0.2), check);
            }
        }
        // The curb's shadow on the stones.
        c *= 1.0 - 0.22 * smoothstep(half_ - 0.35, half_, ax);
    } else if (ax < half_ + 0.55) {
        // The curb: long blocks, lit on top.
        const float blockL = 1.7;
        float fz = fract(z / blockL);
        float h1 = hash21(vec2(floor(z / blockL), sign(x)));
        vec3 curb = uCurb.rgb * (0.94 + 0.08 * h1);
        float e = min(fz, 1.0 - fz) * blockL;
        float joint = (1.0 - smoothstep(0.03 - aa * 0.5, 0.03 + aa * 0.5, e)) * (1.0 - far);
        float edge = smoothstep(half_ + 0.4, half_ + 0.55, ax);
        c = mix(curb, uMortar.rgb, joint * 0.8) * (1.0 - 0.18 * edge);
    } else {
        // The verge: two greens (or sands, or snows) in soft patches, tufts, a speckle of petals or pebbles.
        float n1 = vnoise(vec2(x * 0.3, z * 0.3)), n2 = vnoise(vec2(x * 1.6 + 7.0, z * 1.6));
        c = mix(uGrassA.rgb, uGrassB.rgb, clamp(n1 * 0.9 + n2 * 0.25 - 0.15, 0.0, 1.0));
        highp vec2 cell = floor(vec2(x, z) / 0.55);
        highp vec2 f = fract(vec2(x, z) / 0.55) - 0.5;
        float r = hash21(cell);
        float tuft = (1.0 - smoothstep(0.08, 0.16, length(f * vec2(1.0, 1.8) - vec2(r - 0.5, 0.0) * 0.5))) * step(0.55, r);
        c = mix(c, uGrassB.rgb * 0.82, tuft * 0.55 * (1.0 - far));
        float speck = (1.0 - smoothstep(0.05, 0.1, length(f - vec2(0.2 * r, -0.15)))) * step(0.86, fract(r * 13.7));
        c = mix(c, vec3(1.0, 0.97, 0.92), speck * uGrassB.w * (1.0 - far));
        // The curb's shadow on the verge.
        c *= 1.0 - 0.25 * (1.0 - smoothstep(half_ + 0.55, half_ + 1.1, ax));
    }
    // Haze in the distance, a little light from above the screen, the paper's grain.
    float haze = smoothstep(uHaze.w, uHaze.w + 50.0, depth) * 0.6;
    c = mix(c, uHaze.rgb, haze);
    float sy = px.y * uScreen.w;
    c *= 0.94 + 0.08 * (1.0 - sy);
    float g = hash21(floor(px * 0.5));
    c *= 0.975 + 0.04 * g;
    oColor = vec4(c, 1.0);
}
