#include "Environment.hpp"
#include "Platform.hpp"

#ifdef PLATFORM_WINDOWS
#include <Windows.h>

namespace environment {
uint32_t get_current_process_id() { return GetProcessId(GetCurrentProcess()); }

uint32_t get_current_thread_id() { return GetThreadId(GetCurrentThread()); }

uint64_t get_tick_count() { return __rdtsc(); }
} // namespace environment

#else

namespace environment {
uint32_t get_current_process_id() { fatal_error("TODO"); }

uint32_t get_current_thread_id() { fatal_error("TODO"); }

uint64_t get_tick_count() { fatal_error("TODO"); }
} // namespace environment

#endif
