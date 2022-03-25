#pragma once
#include <cstdint>

namespace flugzeug::environment {
uint32_t get_current_process_id();
uint32_t get_current_thread_id();
uint64_t get_tick_count();
} // namespace flugzeug::environment