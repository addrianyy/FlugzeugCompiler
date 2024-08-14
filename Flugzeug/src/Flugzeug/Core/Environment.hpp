#pragma once
#include <cstdint>

namespace flugzeug {

class Environment {
 public:
  static uint32_t current_process_id();
  static uint32_t current_thread_id();
  static uint64_t monotonic_timestamp();
};

}  // namespace flugzeug