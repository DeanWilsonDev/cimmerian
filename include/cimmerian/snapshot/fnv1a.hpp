#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace Cimmerian::Snapshot {

inline std::string Fnv1a64Hex(const void* data, std::size_t size)
{
  constexpr std::uint64_t OFFSET_BASIS = 0xcbf29ce484222325ULL;
  constexpr std::uint64_t PRIME = 0x100000001b3ULL;

  std::uint64_t hash = OFFSET_BASIS;
  const unsigned char* bytes = static_cast<const unsigned char*>(data);
  for (std::size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= PRIME;
  }

  static constexpr char HEX_DIGITS[] = "0123456789abcdef";
  std::string out(16, '0');
  for (int i = 15; i >= 0; --i) {
    out[static_cast<std::size_t>(i)] = HEX_DIGITS[hash & 0xF];
    hash >>= 4;
  }
  return out;
}

} // namespace Cimmerian::Snapshot
