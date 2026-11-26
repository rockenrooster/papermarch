// P0 test quads and development read-outs: screen-space rectangles placed between ticks (aPos.zw = last step).
layout(location = 0) in vec4 aPos;    // x, y, last step dx, dy (pixels, top-left origin)
layout(location = 1) in vec2 aSize;   // pixels
layout(location = 2) in vec4 aColor;
out vec4 vColor;
void main() {
    vec2 c = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1));
    vec2 p = aPos.xy - aPos.zw * (1.0 - uTime.y) + (c - 0.5) * aSize;
    gl_Position = vec4(p.x * uScreen.z * 2.0 - 1.0, 1.0 - p.y * uScreen.w * 2.0, 0.0, 1.0);
    vColor = aColor;
}
