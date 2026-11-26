#include "core/log.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>

#if defined(__ANDROID__)
#include <android/log.h>
#endif

namespace gt {

namespace {
std::atomic<int> g_warnings{0};
}

int logWarningCount() { return g_warnings.load(); }

void logf(LogLevel level, const char* tag, const char* fmt, ...) {
    if (level != LogLevel::Info) g_warnings.fetch_add(1);
    char buf[4000];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
#if defined(__ANDROID__)
    const int prio = level == LogLevel::Error ? ANDROID_LOG_ERROR : level == LogLevel::Warn ? ANDROID_LOG_WARN : ANDROID_LOG_INFO;
    __android_log_write(prio, tag, buf);
#else
    const char* lv = level == LogLevel::Error ? "error: " : level == LogLevel::Warn ? "warning: " : "";
    std::fprintf(stdout, "%s %s%s\n", tag, lv, buf);
    std::fflush(stdout);
#endif
}

}  // namespace gt
