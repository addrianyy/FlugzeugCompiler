#pragma once
#include <cstdint>

namespace environment {
uint32_t get_current_process_id();
uint32_t get_current_thread_id();
uint64_t get_tick_count();
} // namespace environment