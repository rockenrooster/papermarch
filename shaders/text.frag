in highp vec2 vUV;
in vec4 vFill;
in vec4 vStroke;
in vec4 vP0;
in vec4 vP1;
uniform sampler2D uAtlas;
uniform mediump sampler2DArray uSprites;  // the art's atlases (premultiplied): mode 4
uniform float uSpread;    // atlas px
uniform float uOpacity;
layout(location = 0) out vec4 oColor;
void main() {
    if (vP0.x > 4.5) {  // a ring's arc: radius vP1.x, half band vP1.y, clockwise from twelve o'clock for vP1.z of a turn
        float d = vP1.y - abs(length(vUV) - vP1.x);
        float a = atan(vUV.x, -vUV.y);  // 0 at twelve o'clock, growing clockwise (y is down)
        if (a < 0.0) a += 6.2831853;
        if (a > vP1.z * 6.2831853) d = min(d, -0.6);
        float k = clamp(d + 0.5, 0.0, 1.0) * vFill.a;
        oColor = vec4(vFill.rgb * k, k) * uOpacity;
        return;
    }
    if (vP0.x > 3.5) {  // a sprite: tinted by fill (its alpha the opacity), its hue turned by vP1.y
        vec4 t = texture(uSprites, vec3(vUV, vP1.x));
        vec3 c = t.rgb;
        if (vP1.y > 0.002) {
            float ang = vP1.y * 6.2831853;
            vec3 axis = vec3(0.57735027);
            float ca = cos(ang), sa = sin(ang);
            c = max(c * ca + cross(axis, c) * sa + axis * dot(axis, c) * (1.0 - ca), vec3(0.0));
        }
        oColor = vec4(c * vFill.rgb, t.a) * vFill.a * uOpacity;
        return;
    }
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
