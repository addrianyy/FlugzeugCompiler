#include "Environment.hpp"
#include "Error.hpp"
#include "Platform.hpp"

#ifdef PLATFORM_WINDOWS

#include <Windows.h>

namespace flugzeug {

uint32_t Environment::current_process_id() {
  return GetProcessId(GetCurrentProcess());
}
uint32_t Environment::current_thread_id() {
  return GetThreadId(GetCurrentThread());
}
uint64_t Environment::monotonic_timestamp() {
  return __rdtsc();
}

}  // namespace flugzeug

#elif defined(PLATFORM_LINUX)

#include <unistd.h>
#include <x86intrin.h>

namespace flugzeug {

uint32_t Environment::current_process_id() {
  return uint32_t(getpid());
}
uint32_t Environment::current_thread_id() {
  return uint32_t(gettid());
}
uint64_t Environment::monotonic_timestamp() {
  return __rdtsc();
}

}  // namespace flugzeug

#elif defined(PLATFORM_MAC)

#include <pthread.h>
#include <unistd.h>

namespace flugzeug {

uint32_t Environment::current_process_id() {
  return uint32_t(getpid());
}
uint32_t Environment::current_thread_id() {
  uint64_t tid;
  pthread_threadid_np(nullptr, &tid);
  return uint32_t(tid);
}
uint64_t Environment::monotonic_timestamp() {
  return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}

}  // namespace flugzeug

#else

namespace flugzeug {

uint32_t Environment::current_process_id() {
  fatal_error("Not supported yet");
}
uint32_t Environment::current_thread_id() {
  fatal_error("Not supported yet");
}
uint64_t Environment::monotonic_timestamp() {
  fatal_error("Not supported yet");
}

}  // namespace flugzeug

#endif
