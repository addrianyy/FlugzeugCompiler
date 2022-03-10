#include "Files.hpp"
#include "Error.hpp"

#include <fstream>
#include <string>

std::string flugzeug::read_file_to_string(const std::string& path) {
  std::string source;
  {
    std::ifstream file(path);
    verify(!!file, "Failed to open `{}` for reading", path);

    file.seekg(0, std::ios::end);
    source.reserve(file.tellg());
    file.seekg(0, std::ios::beg);

    source.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  }

  return source;
}
