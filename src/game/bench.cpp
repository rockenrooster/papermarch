#include "game/bench.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "build_stamp.h"
#include "core/clock.h"
#include "core/log.h"
#include "platform/platform.h"

namespace gt {

void Bench::begin(const char* scenario, float seconds, double now) {
    if (!frames_) {
        frames_ = static_cast<BenchFrame*>(std::calloc(kMax, sizeof(BenchFrame)));
        presents_ = static_cast<double*>(std::calloc(kMax, sizeof(double)));
    }
    std::snprintf(scenario_, sizeof scenario_, "%s", scenario);
    seconds_ = seconds;
    start_ = now;
    n_ = 0;
    presentCount_ = 0;
    active_ = true;
    done_ = false;
    valid_ = true;
    allocStart_ = allocationCount();
}

void Bench::addFrame(const BenchFrame& f) {
    if (!active_) return;
    if (n_ < kMax) frames_[n_++] = f;
}

void Bench::addPresents(const double* t, int n) {
    if (!active_) return;
    for (int i = 0; i < n && presentCount_ < kMax; ++i) presents_[presentCount_++] = t[i];
}

void Bench::finish(const char* device, const char* glRenderer, const char* glVersion, float refreshHz, const char* extra) {
    if (!active_) return;
    active_ = false;
    done_ = true;
    const u64 allocs = allocationCount() - allocStart_;
    const int n = n_;
    float* tmp = static_cast<float*>(std::malloc(sizeof(float) * size_t(kMax)));

    auto stats = [&](auto get, float* out4) {
        double mx = 0;
        for (int i = 0; i < n; ++i) {
            tmp[i] = get(frames_[i]);
            mx = std::fmax(mx, tmp[i]);
        }
        out4[0] = percentile(tmp, n, 0.50f) * 1000.0f;
        out4[1] = percentile(tmp, n, 0.95f) * 1000.0f;
        out4[2] = percentile(tmp, n, 0.99f) * 1000.0f;
        out4[3] = float(mx) * 1000.0f;
    };
    float interval[4], cpu[4], tick[4], phases[kPhaseCount][4];
    stats([](const BenchFrame& f) { return f.interval; }, interval);
    stats([](const BenchFrame& f) { return f.cpu; }, cpu);
    stats([](const BenchFrame& f) { return f.tickMax; }, tick);
    for (int p = 0; p < kPhaseCount; ++p) stats([p](const BenchFrame& f) { return f.phase[p]; }, phases[p]);
    int maxW = 0, maxG = 0, maxP = 0, maxDraws = 0;
    for (int i = 0; i < n; ++i) {
        maxW = maxv(maxW, frames_[i].wisps);
        maxG = maxv(maxG, frames_[i].glooms);
        maxP = maxv(maxP, frames_[i].particles);
        maxDraws = maxv(maxDraws, frames_[i].drawCalls);
    }

    // The contract (PLAN 5): present intervals in whole refresh periods; late = shown two or more periods after the
    // previous frame. Without present times, swap intervals stand in (flagged).
    const double period = 1.0 / double(refreshHz > 1 ? refreshHz : 60.0f);
    const bool havePresents = presentCount_ > 10;
    int late = 0, counted = 0;
    double worst = 0;
    float display[4] = {0, 0, 0, 0};
    if (havePresents) {
        for (int i = 1; i < presentCount_; ++i) {
            const double d = presents_[i] - presents_[i - 1];
            if (d <= 0) continue;
            tmp[counted++] = float(d);
            worst = std::fmax(worst, d);
            if (std::lround(d / period) >= 2) ++late;
        }
    } else {
        for (int i = 0; i < n; ++i) {
            const double d = frames_[i].interval;
            tmp[counted++] = float(d);
            worst = std::fmax(worst, d);
            if (std::lround(d / period) >= 2) ++late;
        }
    }
    if (counted > 0) {
        display[0] = percentile(tmp, counted, 0.50f) * 1000.0f;
        display[1] = percentile(tmp, counted, 0.95f) * 1000.0f;
        display[2] = percentile(tmp, counted, 0.99f) * 1000.0f;
        display[3] = float(worst) * 1000.0f;
    }
    const float lateShare = counted ? float(late) / float(counted) : 1.0f;
    const bool pass = lateShare <= 0.05f && worst <= 0.050;
    std::free(tmp);

    static char json[8192];
    int o = std::snprintf(
        json, sizeof json,
        "{\"scenario\":\"%s\",\"device\":\"%s\",\"build\":\"%s %s %s\",\"gl_renderer\":\"%s\",\"gl_version\":\"%s\","
        "\"refresh_hz\":%.2f,\"seconds\":%.1f,\"frames\":%d,\"presents\":%d,\"present_timestamps\":%s,"
        "\"late_share\":%.4f,\"worst_ms\":%.2f,\"contract_pass\":%s,\"valid\":%s,"
        "\"display_ms\":[%.2f,%.2f,%.2f,%.2f],\"interval_ms\":[%.2f,%.2f,%.2f,%.2f],\"cpu_ms\":[%.2f,%.2f,%.2f,%.2f],"
        "\"tick_ms\":[%.3f,%.3f,%.3f,%.3f],\"heap_allocs\":%llu,\"max_wisps\":%d,\"max_glooms\":%d,\"max_particles\":%d,"
        "\"phases_p95_ms\":{\"events\":%.3f,\"sim\":%.3f,\"view\":%.3f,\"build\":%.3f,\"submit\":%.3f,\"swap\":%.3f}",
        scenario_, device, GT_VERSION, GT_COMMIT, GT_BUILD_DATE, glRenderer, glVersion, refreshHz, double(seconds_), n,
        presentCount_, havePresents ? "true" : "false", lateShare, worst * 1000.0, pass ? "true" : "false",
        valid_ ? "true" : "false", display[0], display[1], display[2], display[3], interval[0], interval[1], interval[2],
        interval[3], cpu[0], cpu[1], cpu[2], cpu[3], tick[0], tick[1], tick[2], tick[3], (unsigned long long)allocs, maxW,
        maxG, maxP, phases[kPhaseEvents][1], phases[kPhaseSim][1], phases[kPhaseView][1], phases[kPhaseBuild][1],
        phases[kPhaseSubmit][1], phases[kPhaseSwap][1]);
    if (o < int(sizeof json) - 40) o += std::snprintf(json + o, sizeof json - size_t(o), ",\"draw_calls\":%d", maxDraws);
    if (extra && o < int(sizeof json) - 2) o += std::snprintf(json + o, sizeof json - size_t(o), "%s", extra);
    if (o < int(sizeof json) - 2) std::snprintf(json + o, sizeof json - size_t(o), "}");
    GT_TAG("GTBENCH", "%s", json);

    char name[64], path[512];
    std::snprintf(name, sizeof name, "bench_%s.json", scenario_);
    platform::outputPath(name, path, sizeof path);
    if (FILE* f = std::fopen(path, "wb")) {
        std::fputs(json, f);
        std::fputs("\n", f);
        std::fclose(f);
        GT_LOG("bench: wrote %s", path);
    }
}

}  // namespace gt
