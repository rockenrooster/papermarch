// Counted global operator new: every heap allocation through new/new[] on a thread bumps that thread's counter.
// Left out of the sanitizer build (the sanitizer's runtime owns operator new there).
#include <cstdlib>
#include <new>

#include "core/types.h"

namespace {
thread_local gt::u64 t_allocations = 0;  // trivial type: no TLS constructor, safe inside operator new

void* countedAlloc(std::size_t size) {
    ++t_allocations;
    void* p = std::malloc(size ? size : 1);
    return p;
}
}  // namespace

gt::u64 gt::allocationCount() { return t_allocations; }

void* operator new(std::size_t size) {
    void* p = countedAlloc(size);
    if (!p) std::abort();
    return p;
}
void* operator new[](std::size_t size) {
    void* p = countedAlloc(size);
    if (!p) std::abort();
    return p;
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return countedAlloc(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return countedAlloc(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
