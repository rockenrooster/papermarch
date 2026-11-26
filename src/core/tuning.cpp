#include "core/tuning.h"

#include <cstdlib>
#include <cstring>

namespace pm {

Tuning tune;

bool setTuning(const char* name, const char* value) {
    char* end = nullptr;
    const float v = std::strtof(value, &end);
    if (end == value || *end != '\0') return false;
#define PM_TUNING_SET(field, def)            \
    if (std::strcmp(name, #field) == 0) {    \
        tune.field = v;                      \
        return true;                         \
    }
    PM_TUNING(PM_TUNING_SET)
#undef PM_TUNING_SET
    return false;
}

bool setTuningPair(const char* pair) {
    const char* eq = std::strchr(pair, '=');
    if (!eq || eq == pair) return false;
    char name[64];
    const size_t n = size_t(eq - pair);
    if (n >= sizeof name) return false;
    std::memcpy(name, pair, n);
    name[n] = 0;
    return setTuning(name, eq + 1);
}

}  // namespace pm
