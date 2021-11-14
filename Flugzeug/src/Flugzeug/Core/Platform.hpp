#pragma once

#if defined(_WIN32) || defined(_WIN64) || defined(_WINDOWS)
#define PLATFORM_WINDOWS
#elif defined(__linux__)
#define PLATFORM_LINUX
#else
#error "Unknown platform"
#endif
