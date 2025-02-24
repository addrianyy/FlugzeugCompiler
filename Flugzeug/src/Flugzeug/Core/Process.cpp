#include "Process.hpp"
#include "Error.hpp"
#include "Platform.hpp"

#include <fmt/core.h>

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#include <vector>

uint32_t flugzeug::run_process(const std::string& application,
                               const std::string& command_line,
                               const std::string& process_stdin) {
  const auto actual_command_line = fmt::format(R"("{}" {})", application, command_line);

  std::vector<char> command_line_buffer(actual_command_line.size() + 1);
  std::memcpy(command_line_buffer.data(), actual_command_line.c_str(),
              actual_command_line.size() + 1);

  HANDLE stdin_read, stdin_write;

  SECURITY_ATTRIBUTES attribs{};
  attribs.nLength = sizeof(SECURITY_ATTRIBUTES);
  attribs.bInheritHandle = true;
  attribs.lpSecurityDescriptor = nullptr;

  verify(CreatePipe(&stdin_read, &stdin_write, &attribs, 0), "Failed to create stdin pipe");
  verify(SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0),
         "Failed to set inherit information for stdin handles");

  STARTUPINFOA startup_info{};
  PROCESS_INFORMATION process_info{};

  startup_info.cb = sizeof(STARTUPINFOA);
  startup_info.dwFlags = STARTF_USESTDHANDLES;
  startup_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
  startup_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  startup_info.hStdInput = stdin_read;

  verify(CreateProcessA(nullptr, command_line_buffer.data(), nullptr, nullptr, true, 0, nullptr,
                        nullptr, &startup_info, &process_info),
         "Failed to start process `{}`", application);

  CloseHandle(stdin_read);
  verify(
    WriteFile(stdin_write, process_stdin.c_str(), DWORD(process_stdin.size()), nullptr, nullptr),
    "Failed to write to the stdin");
  CloseHandle(stdin_write);

  DWORD exit_code;
  verify(WaitForSingleObject(process_info.hProcess, INFINITE) == WAIT_OBJECT_0,
         "Failed to wait for the process");
  verify(GetExitCodeProcess(process_info.hProcess, &exit_code), "Failed to get process exit code");

  CloseHandle(process_info.hThread);
  CloseHandle(process_info.hProcess);

  return exit_code;
}

#elif defined(PLATFORM_LINUX) || defined(PLATFORM_MAC)
#include <cstdio>

uint32_t flugzeug::run_process(const std::string& application,
                               const std::string& command_line,
                               const std::string& process_stdin) {
  const auto actual_command_line = fmt::format("{} {}", application, command_line);

  const auto pipe = popen(actual_command_line.c_str(), "w");
  verify(pipe, "Failed to start process `{}`", application);

  verify(fwrite(process_stdin.c_str(), 1, process_stdin.size(), pipe) == process_stdin.size(),
         "Failed to write to the stdin");
  const auto exit_code = pclose(pipe);
  verify(exit_code != -1, "Failed to wait for the process");

  return uint32_t(exit_code);
}

#else

uint32_t flugzeug::run_process(const std::string& application,
                               const std::string& command_line,
                               const std::string& process_stdin) {
  fatal_error("`run_process` is not implemented yet for this platform");
}

#endif
