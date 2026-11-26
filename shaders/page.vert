// A textured screen rectangle (the baked page, or the old page during a turn): uRect in pixels, uUV the texture's
// window (u0, v0 at the rectangle's top-left; v up in the texture).
uniform highp vec4 uRect;
uniform highp vec4 uUV;
out highp vec2 vUV;
out highp vec2 vPx;
void main() {
    vec2 c = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1));
    vec2 p = uRect.xy + c * uRect.zw;
    gl_Position = vec4(p.x * uScreen.z * 2.0 - 1.0, 1.0 - p.y * uScreen.w * 2.0, 0.0, 1.0);
    vUV = vec2(mix(uUV.x, uUV.z, c.x), mix(uUV.y, uUV.w, c.y));
    vPx = p;
}
