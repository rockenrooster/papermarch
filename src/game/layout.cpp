#include "game/layout.h"

#include <cstdio>

namespace pm {

PageFit fitPage(const Layout& L, float pageH) {
    PageFit f;
    const float left = L.safe.left + L.px(6), right = float(L.w) - L.safe.right - L.px(6);
    f.screenTop = L.top() + L.px(kTopBandDp);
    f.screenBottom = L.bottom() - L.px(kBottomBandDp);
    const float availW = right - left, availH = f.screenBottom - f.screenTop;
    // A window of 160 map units fits; a taller page scrolls inside it.
    f.k = availW / 100.0f;
    if (f.k * 160.0f > availH) f.k = availH / 160.0f;
    f.viewH = pageH < availH / f.k ? pageH : availH / f.k;
    const float w = 100.0f * f.k, h = f.viewH * f.k;
    f.x = (left + right) * 0.5f - w * 0.5f;
    f.y = (f.screenTop + f.screenBottom) * 0.5f - h * 0.5f;
    return f;
}

Rect centered(const Layout& L, float cy, float wDp, float hDp) {
    const float w = L.px(wDp), h = L.px(hDp);
    return {float(L.w) * 0.5f - w * 0.5f, cy - h * 0.5f, w, h};
}

void formatThousands(long long v, char* buf, int n) {
    char tmp[32];
    const bool neg = v < 0;
    unsigned long long u = neg ? 0ull - (unsigned long long)v : (unsigned long long)v;
    int o = 0, digits = 0;
    do {
        if (digits && digits % 3 == 0) tmp[o++] = ',';
        tmp[o++] = char('0' + u % 10);
        u /= 10;
        ++digits;
    } while (u);
    if (neg) tmp[o++] = '-';
    int k = 0;
    while (o > 0 && k < n - 1) buf[k++] = tmp[--o];
    buf[k] = 0;
}

void formatHost(long long v, char* buf, int n) {
    if (v < 1000000) {
        formatThousands(v, buf, n);
        return;
    }
    const double m = double(v) / 1e6;
    if (m < 10) std::snprintf(buf, size_t(n), "%.2fM", m);
    else if (m < 100) std::snprintf(buf, size_t(n), "%.1fM", m);
    else std::snprintf(buf, size_t(n), "%.0fM", m);
}

}  // namespace pm
