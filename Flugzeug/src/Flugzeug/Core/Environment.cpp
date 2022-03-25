#include "Environment.hpp"
#include "Error.hpp"
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

#include <unistd.h>
#include <x86intrin.h>

uint32_t get_current_process_id() { return uint32_t(getpid()); }

uint32_t get_current_thread_id() { return uint32_t(gettid()); }

uint64_t get_tick_count() { return __rdtsc(); }

} // namespace environment

#endif
