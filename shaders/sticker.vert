// A sticker (TECH_SPEC 5.1): one cut-paper shape on an instanced quad, drawn as a 4-vertex strip. Positions in pixels,
// top-left origin. uMode.x = 1: the shadow pass (coverage, moved by the sticker's lift).
layout(location = 0) in vec4 aPosSize;  // centre x, y, half width, half height
layout(location = 1) in vec4 aParams;   // rotation (radians), shape, edge softness (px), p0
layout(location = 2) in vec4 aParams2;  // p1, p2, fold, lift (px above the page: the shadow's offset)
layout(location = 3) in vec4 aC0;
layout(location = 4) in vec4 aC1;
layout(location = 5) in vec4 aC2;
uniform highp vec4 uMode;   // x shadow pass, y extra softness, z alpha scale, w shadow offset scale
uniform highp vec4 uShift;  // xy pixels added to every position (scrolling), zw scale (the unfold), unused here
out highp vec2 vLocal;
flat out highp vec4 vA;     // half width, half height, shape, softness
flat out highp vec4 vB;     // p0, p1, p2, fold
flat out mediump vec4 vC0;
flat out mediump vec4 vC1;
flat out mediump vec4 vC2;
void main() {
    vec2 corner = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1)) * 2.0 - 1.0;
    float lift = aParams2.w;
    float soft = aParams.z + (uMode.x > 0.5 ? uMode.y + lift * 0.35 : 0.0);
    float pad = soft + 2.0;
    vec2 local = corner * (aPosSize.zw + vec2(pad));
    float c = cos(aParams.x), s = sin(aParams.x);
    vec2 p = aPosSize.xy + vec2(c * local.x - s * local.y, s * local.x + c * local.y) + uShift.xy;
    if (uMode.x > 0.5) p += vec2(0.45, 0.85) * lift * uMode.w;
    gl_Position = vec4(p.x * uScreen.z * 2.0 - 1.0, 1.0 - p.y * uScreen.w * 2.0, 0.0, 1.0);
    if (uMode.x > 0.5 && lift <= 0.0) gl_Position = vec4(2.0, 2.0, 2.0, 1.0);  // flat on the page: no shadow
    vLocal = local;
    vA = vec4(aPosSize.zw, aParams.y, soft);
    vB = vec4(aParams.w, aParams2.x, aParams2.y, aParams2.z);
    vC0 = aC0;
    vC1 = aC1;
    vC2 = aC2;
}
