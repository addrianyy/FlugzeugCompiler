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

#elif defined(PLATFORM_MAC)

#include <pthread.h>
#include <unistd.h>

namespace flugzeug::environment {

uint32_t get_current_process_id() {
  return uint32_t(getpid());
}
uint32_t get_current_thread_id() {
  uint64_t tid;
  pthread_threadid_np(nullptr, &tid);
  return uint32_t(tid);
}
uint64_t get_tick_count() {
  return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}

}  // namespace flugzeug::environment

#else

namespace flugzeug::environment {

uint32_t get_current_process_id() {
  fatal_error("Not supported yet");
}
uint32_t get_current_thread_id() {
  fatal_error("Not supported yet");
}
uint64_t get_tick_count() {
  fatal_error("Not supported yet");
}

}  // namespace flugzeug::environment

#endif
