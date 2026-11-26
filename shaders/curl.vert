// The page turn (TECH_SPEC 5.5): the old page, a strip of rows across the fold direction, wrapped around a cylinder
// past the moving fold line; past half a turn it lies flat on its back over what is left. Rows are drawn in order of
// their distance along the fold direction, so later (higher) paper covers earlier.
layout(location = 0) in vec2 aUV;  // x: -1 or 1 across, y: 0..1 along the fold direction
uniform highp vec4 uCurl;          // x the fold line (px along n), y the radius (px), zw the direction n
uniform highp vec4 uSpan;          // x, y: the screen's span along n; z, w: across
out highp vec2 vTex;
out highp float vTheta;
void main() {
    vec2 n = uCurl.zw;
    vec2 t = vec2(-n.y, n.x);
    float across = mix(uSpan.z, uSpan.w, aUV.x * 0.5 + 0.5);
    float along = mix(uSpan.x, uSpan.y, aUV.y);
    vec2 p = t * across + n * along;
    float d = along - uCurl.x;
    float r = uCurl.y;
    float theta = 0.0;
    float pos = along;
    if (d > 0.0) {
        theta = d / r;
        pos = theta < 3.14159265 ? uCurl.x + r * sin(theta) : uCurl.x - (d - 3.14159265 * r);
    }
    vec2 q = t * across + n * pos;
    gl_Position = vec4(q.x * uScreen.z * 2.0 - 1.0, 1.0 - q.y * uScreen.w * 2.0, 0.0, 1.0);
    vTex = vec2(p.x * uScreen.z, 1.0 - p.y * uScreen.w);
    vTheta = theta;
}
