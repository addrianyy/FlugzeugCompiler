#pragma once
#include <string>

namespace flugzeug {

class File {
 public:
  static std::string read_to_string(const std::string& path);
};

}  // namespace flugzeug