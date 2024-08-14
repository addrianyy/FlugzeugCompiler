#include "FileIRPrinter.hpp"

#include <Flugzeug/Core/Error.hpp>

using namespace flugzeug;

void FileIRPrinter::write_string(std::string_view string) {
  file_stream << string;
}

FileIRPrinter::FileIRPrinter(const std::string& path) : file_stream(path, std::ios::out) {
  verify(!!file_stream, "Failed to open `{}` for writing", path);
}
