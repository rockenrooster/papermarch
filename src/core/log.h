#pragma once
// Logging: stdout on the desktop, logcat on Android. Tags: PAPERMARCH (everything), PMBENCH, PMSIM, PMLIFE, FLOW.

namespace pm {

enum class LogLevel { Info, Warn, Error };
void logf(LogLevel level, const char* tag, const char* fmt, ...) __attribute__((format(printf, 3, 4)));

// Warnings and errors logged so far (exit code 3 in debug builds counts GL messages separately).
int logWarningCount();

}  // namespace pm

#define PM_LOG(...) ::pm::logf(::pm::LogLevel::Info, "PAPERMARCH", __VA_ARGS__)
#define PM_WARN(...) ::pm::logf(::pm::LogLevel::Warn, "PAPERMARCH", __VA_ARGS__)
#define PM_ERR(...) ::pm::logf(::pm::LogLevel::Error, "PAPERMARCH", __VA_ARGS__)
#define PM_TAG(tag, ...) ::pm::logf(::pm::LogLevel::Info, tag, __VA_ARGS__)
