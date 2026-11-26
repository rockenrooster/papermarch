#pragma once
// Fixed-width aliases and tiny helpers shared by every layer.

#include <cstddef>
#include <cstdint>

namespace gt {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

template <typename T, size_t N>
constexpr int countOf(const T (&)[N]) { return int(N); }

template <typename T>
constexpr T clampv(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
template <typename T>
constexpr T minv(T a, T b) { return a < b ? a : b; }
template <typename T>
constexpr T maxv(T a, T b) { return a > b ? a : b; }

// Heap allocations made by global operator new on this thread (src/core/alloc_counter.cpp). Benchmarks and
// tests read it before and after a window that must not allocate.
u64 allocationCount();

}  // namespace gt
