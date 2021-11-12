#include "Log.hpp"
#include <iostream>

void detail::log::log(const char* file, int line, LogLevel level, const std::string& message) {
  std::ostream& stream = level >= LogLevel::Warn ? std::cerr : std::cout;
  const char* header = nullptr;

  switch (level) {
  case LogLevel::Debug:
    header = "\x1b[32;1m[debug] ";
    break;
  case LogLevel::Info:
    header = "\x1b[36;1m[info ] ";
    break;
  case LogLevel::Warn:
    header = "\x1b[33;1m[warn ] ";
    break;
  case LogLevel::Error:
    header = "\x1b[31;1m[error] ";
    break;
  default:
    header = "??? ";
  }

  stream << header << message << "\x1b[0m" << std::endl;
}