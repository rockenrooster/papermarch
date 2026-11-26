#include "core/tuning.h"

#include <cstdlib>
#include <cstring>

namespace gt {

Tuning tune;

bool setTuning(const char* name, const char* value) {
    char* end = nullptr;
    const float v = std::strtof(value, &end);
    if (end == value || *end != '\0') return false;
#define GT_TUNING_SET(field, def)            \
    if (std::strcmp(name, #field) == 0) {    \
        tune.field = v;                      \
        return true;                         \
    }
    GT_TUNING(GT_TUNING_SET)
#undef GT_TUNING_SET
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

}  // namespace gt
