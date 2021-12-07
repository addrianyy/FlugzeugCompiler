#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

inline void hash_combine(std::size_t& seed) {}

template <typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& v, Rest... rest) {
  std::hash<T> hasher;
  if constexpr (sizeof(void*) == 8) {
    seed ^= hasher(v) + 0x9e3779b97f4a7c17 + (seed << 6) + (seed >> 2);
  } else {
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }

  hash_combine(seed, rest...);
}

template <typename... Args> inline std::size_t hash_combine(Args... args) {
  std::size_t seed = 0;
  hash_combine(seed, args...);

  return seed;
}