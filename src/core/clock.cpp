#include "core/clock.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <ctime>
#endif

namespace pm {

double threadCpuSeconds() {
#if defined(_WIN32)
    FILETIME c, e, k, u;
    if (!GetThreadTimes(GetCurrentThread(), &c, &e, &k, &u)) return 0;
    const unsigned long long kt = (unsigned long long)k.dwHighDateTime << 32 | k.dwLowDateTime;
    const unsigned long long ut = (unsigned long long)u.dwHighDateTime << 32 | u.dwLowDateTime;
    return double(kt + ut) * 1e-7;
#else
    timespec ts{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return double(ts.tv_sec) + double(ts.tv_nsec) * 1e-9;
#endif
}

}  // namespace pm
