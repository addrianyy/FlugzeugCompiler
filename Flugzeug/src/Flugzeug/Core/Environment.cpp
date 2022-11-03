#include "Environment.hpp"
#include "Error.hpp"
#include "Platform.hpp"

#ifdef PLATFORM_WINDOWS

#include <Windows.h>

namespace flugzeug::environment {

uint32_t get_current_process_id() {
  return GetProcessId(GetCurrentProcess());
}
uint32_t get_current_thread_id() {
  return GetThreadId(GetCurrentThread());
}
uint64_t get_tick_count() {
  return __rdtsc();
}

}  // namespace flugzeug::environment

#elif defined(PLATFORM_LINUX)

#include <unistd.h>
#include <x86intrin.h>

namespace flugzeug::environment {

uint32_t get_current_process_id() {
  return uint32_t(getpid());
}
uint32_t get_current_thread_id() {
  return uint32_t(gettid());
}
uint64_t get_tick_count() {
  return __rdtsc();
}

}  // namespace flugzeug::environment

#else

namespace flugzeug::environment {

uint32_t get_current_process_id() {
  return fatal_error("Not supported yet");
}
uint32_t get_current_thread_id() {
  return fatal_error("Not supported yet");
}
uint64_t get_tick_count() {
  return fatal_error("Not supported yet");
}

}  // namespace flugzeug::environment

#endif
