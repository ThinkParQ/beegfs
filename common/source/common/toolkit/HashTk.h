#pragma once

#include <cinttypes>

namespace HashTk {
  uint32_t hsieh32(const char* data, int len);
  uint64_t authHash(const unsigned char* data, std::size_t len);
}

