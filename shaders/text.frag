in highp vec2 vUV;
in vec4 vFill;
in vec4 vStroke;
in vec4 vP0;
in vec4 vP1;
uniform sampler2D uAtlas;
uniform float uSpread;    // atlas px
uniform float uOpacity;
layout(location = 0) out vec4 oColor;
void main() {
    float d;  // pixels, positive inside
    if (vP0.x < 0.5) {
        float s = texture(uAtlas, vUV).r;
        d = (s - 0.5) * 2.0 * uSpread * vP1.z;
    } else if (vP0.x < 1.5) {
        vec2 q = abs(vUV) - (vP1.xy - vec2(vP0.z));
        d = -(length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - vP0.z);
    } else if (vP0.x < 2.5) {  // a four-point star of radius vP1.x
        vec2 q = abs(vUV) / vP1.x;
        float s = sqrt(q.x) + sqrt(q.y);
        d = (1.0 - s) * vP1.x * 0.45;
    } else {  // a five-pointed star of radius vP1.x (point up)
        highp vec2 p = vec2(vUV.x, -vUV.y);
        const highp vec2 k1 = vec2(0.809016994375, -0.587785252292);
        const highp vec2 k2 = vec2(-0.809016994375, -0.587785252292);
        p.x = abs(p.x);
        p -= 2.0 * max(dot(k1, p), 0.0) * k1;
        p -= 2.0 * max(dot(k2, p), 0.0) * k2;
        p.x = abs(p.x);
        p.y -= vP1.x;
        highp vec2 ba = 0.45 * vec2(-k1.y, k1.x) - vec2(0.0, 1.0);
        highp float h = clamp(dot(p, ba) / dot(ba, ba), 0.0, vP1.x);
        d = -length(p - ba * h) * sign(p.y * ba.x - p.x * ba.y);
    }
    float fillA = clamp(d + 0.5, 0.0, 1.0) * vFill.a;
    float strokeA = vP0.y > 0.0 ? clamp(d + vP0.y + 0.5, 0.0, 1.0) * vStroke.a : 0.0;
    float shadowA = vP0.w > 0.0 ? smoothstep(-vP0.w, 0.0, d + vP0.y) * 0.35 : 0.0;
    // Composite: shadow, then stroke, then fill (premultiplied).
    vec3 c = vec3(0.0);
    float a = shadowA;
    c = vStroke.rgb * strokeA + c * (1.0 - strokeA);
    a = strokeA + a * (1.0 - strokeA);
    c = vFill.rgb * fillA + c * (1.0 - fillA);
    a = fillA + a * (1.0 - fillA);
    oColor = vec4(c, a) * uOpacity;
}
