#pragma once
// Logging: stdout on the desktop, logcat on Android. Tags: GLOWTIDE (everything), GTBENCH, GTSIM, GTLIFE, FLOW.

namespace gt {

enum class LogLevel { Info, Warn, Error };
void logf(LogLevel level, const char* tag, const char* fmt, ...) __attribute__((format(printf, 3, 4)));

// Warnings and errors logged so far (exit code 3 in debug builds counts GL messages separately).
int logWarningCount();

}  // namespace gt

#define GT_LOG(...) ::gt::logf(::gt::LogLevel::Info, "GLOWTIDE", __VA_ARGS__)
#define GT_WARN(...) ::gt::logf(::gt::LogLevel::Warn, "GLOWTIDE", __VA_ARGS__)
#define GT_ERR(...) ::gt::logf(::gt::LogLevel::Error, "GLOWTIDE", __VA_ARGS__)
#define GT_TAG(tag, ...) ::gt::logf(::gt::LogLevel::Info, tag, __VA_ARGS__)
