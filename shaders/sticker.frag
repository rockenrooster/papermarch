// A sticker's shape as a signed distance in pixels (negative inside), its two-tone fold and an analytic edge
// (TECH_SPEC 5.1). Shapes are chosen by an if chain, never by indexing a constant array (TECH_SPEC 10).
precision highp float;
in highp vec2 vLocal;
flat in highp vec4 vA;
flat in highp vec4 vB;
flat in mediump vec4 vC0;
flat in mediump vec4 vC1;
flat in mediump vec4 vC2;
uniform highp vec4 uMode;
layout(location = 0) out vec4 oColor;

float sdBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
float sdEllipse(vec2 p, vec2 r) {
    float k0 = length(p / r);
    float k1 = length(p / (r * r));
    return k1 > 1e-5 ? k0 * (k0 - 1.0) / k1 : -min(r.x, r.y);
}
// Isosceles triangle, apex at the origin, base at y = q.y (y down), half base q.x.
float sdTri(vec2 p, vec2 q) {
    p.x = abs(p.x);
    vec2 a = p - q * clamp(dot(p, q) / dot(q, q), 0.0, 1.0);
    vec2 b = p - q * vec2(clamp(p.x / q.x, 0.0, 1.0), 1.0);
    float s = -sign(q.y);
    vec2 d = min(vec2(dot(a, a), s * (p.x * q.y - p.y * q.x)), vec2(dot(b, b), s * (p.y - q.y)));
    return -sqrt(d.x) * sign(d.y);
}
float sdCapsule(vec2 p, vec2 a, vec2 b, float r) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - r;
}
float sdStar5(vec2 p, float r, float rf) {
    const vec2 k1 = vec2(0.809016994375, -0.587785252292);
    const vec2 k2 = vec2(-0.809016994375, -0.587785252292);
    p.x = abs(p.x);
    p -= 2.0 * max(dot(k1, p), 0.0) * k1;
    p -= 2.0 * max(dot(k2, p), 0.0) * k2;
    p.x = abs(p.x);
    p.y -= r;
    vec2 ba = rf * vec2(-k1.y, k1.x) - vec2(0.0, 1.0);
    float h = clamp(dot(p, ba) / dot(ba, ba), 0.0, r);
    return length(p - ba * h) * sign(p.y * ba.x - p.x * ba.y);
}

void main() {
    vec2 p = vLocal;
    float hw = vA.x, hh = vA.y;
    int shape = int(vA.z + 0.5);
    int part = 0;
    float d;
    if (shape == 0) {          // rounded box: p0 = corner radius, of the shorter half side
        d = sdBox(p, vec2(hw, hh), vB.x * min(hw, hh));
    } else if (shape == 1) {   // ellipse
        d = sdEllipse(p, vec2(hw, hh));
    } else if (shape == 2) {   // triangle, apex up
        d = sdTri(vec2(p.x, p.y + hh), vec2(hw, 2.0 * hh));
    } else if (shape == 3) {   // a pawn: a bell-shaped body and a head; the scarf in c2, the body's far side in c1
        float body = sdEllipse(p - vec2(0.0, 0.34 * hh), vec2(hw, 0.66 * hh));
        float head = length(p - vec2(0.0, -0.58 * hh)) - 0.40 * hh;
        d = min(body, head);
        if (body < 0.5 && p.y > -0.33 * hh && p.y < -0.17 * hh) part = 2;
        else if (body < 0.5 && p.x > 0.06 * hw) part = 1;
    } else if (shape == 4) {   // a swallow-tailed banner from the pole (left) to the tip; p0 = wave phase; stripe in c2
        float u = clamp((p.x + hw) / (2.0 * hw), 0.0, 1.0);
        float y = p.y - sin(u * 5.5 + vB.x) * 0.22 * hh * u;
        float half_ = hh * (1.0 - 0.25 * u);
        float tip = hw - 0.34 * hw * (1.0 - clamp(abs(y) / half_, 0.0, 1.0));
        d = max(max(abs(y) - half_, p.x - tip), -hw - p.x);
        if (abs(y) < 0.24 * half_) part = 2;
    } else if (shape == 5) {   // a five-pointed star
        d = sdStar5(vec2(p.x, -p.y), min(hw, hh), 0.45);
    } else if (shape == 6) {   // a ring: p0 = thickness (px)
        d = abs(length(p) - (min(hw, hh) - vB.x * 0.5)) - vB.x * 0.5;
    } else if (shape == 7) {   // a capsule along x
        d = sdCapsule(p, vec2(-hw + hh, 0.0), vec2(hw - hh, 0.0), hh);
    } else if (shape == 8) {   // a land blob: a superellipse (exponent p2) with three wobbles (amplitude p0, phase p1)
        float th = atan(p.y, p.x);
        float k = 1.0 + vB.x * (0.5 * sin(3.0 * th + vB.y) + 0.3 * sin(5.0 * th + vB.y * 1.7 + 1.3) +
                                0.2 * sin(8.0 * th + vB.y * 2.3 + 2.1));
        vec2 q = abs(p / (vec2(hw, hh) * k));
        float n = max(vB.z, 1.0);
        float f = pow(pow(q.x, n) + pow(q.y, n), 1.0 / n);
        d = (f - 1.0) * min(hw, hh) * k;
    } else if (shape == 9) {   // a paper horse, facing right: legs in c1
        float body = sdEllipse(p - vec2(-0.05 * hw, -0.05 * hh), vec2(0.62 * hw, 0.36 * hh));
        float neck = sdCapsule(p, vec2(0.42 * hw, -0.15 * hh), vec2(0.72 * hw, -0.62 * hh), 0.17 * hh);
        float head = sdEllipse(p - vec2(0.82 * hw, -0.62 * hh), vec2(0.2 * hw, 0.15 * hh));
        float tail = sdCapsule(p, vec2(-0.62 * hw, -0.15 * hh), vec2(-0.88 * hw, 0.35 * hh), 0.08 * hh);
        float legs = min(sdBox(p - vec2(-0.38 * hw, 0.62 * hh), vec2(0.07 * hw, 0.38 * hh), 0.0),
                         sdBox(p - vec2(0.36 * hw, 0.62 * hh), vec2(0.07 * hw, 0.38 * hh), 0.0));
        d = min(min(min(body, neck), min(head, tail)), legs);
        if (legs < 0.5 && body > 0.0) part = 1;
        else if (tail < 0.0 || (neck < 0.0 && p.x < 0.66 * hw && p.y < -0.25 * hh)) part = 2;
    } else if (shape == 10) {  // a diamond
        d = (abs(p.x) / hw + abs(p.y) / hh - 1.0) * min(hw, hh) * 0.7;
    } else {                   // an arch: a box with a round top
        float r = hw;
        d = min(sdBox(p - vec2(0.0, r * 0.5), vec2(hw, hh - r * 0.5), 0.0), length(p - vec2(0.0, -hh + r)) - r);
    }
    int fold = int(vB.w + 0.5);
    if (part == 0 && fold == 1 && p.x > 0.0) part = 1;
    if (part == 0 && fold == 2 && p.y > 0.0) part = 1;
    float soft = max(vA.w, 0.75);
    float a = 1.0 - smoothstep(-0.5 * soft, 0.5 * soft, d);
    if (uMode.x > 0.5) {
        oColor = vec4(a * uMode.z * vC0.a);
        return;
    }
    vec4 c = part == 0 ? vC0 : (part == 1 ? vC1 : vC2);
    float alpha = c.a * a * uMode.z;
    oColor = vec4(c.rgb * alpha, alpha);
}
