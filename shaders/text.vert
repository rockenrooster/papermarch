// Screen-space text and rounded rectangles (TECH_SPEC 5.7, 5.11). Positions in pixels, top-left origin.
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aFill;
layout(location = 3) in vec4 aStroke;
layout(location = 4) in vec4 aP0;  // mode, stroke px, radius px, shadow px
layout(location = 5) in vec4 aP1;  // half size px (rect), screen px per atlas px (text)
out highp vec2 vUV;
out vec4 vFill;
out vec4 vStroke;
out vec4 vP0;
out vec4 vP1;
void main() {
    gl_Position = vec4(aPos.x * uScreen.z * 2.0 - 1.0, 1.0 - aPos.y * uScreen.w * 2.0, 0.0, 1.0);
    vUV = aUV;
    vFill = aFill;
    vStroke = aStroke;
    vP0 = aP0;
    vP1 = aP1;
}
