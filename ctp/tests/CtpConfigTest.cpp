// Configuration test: CMake must fail, with a message naming CTP_HOME, when the
// CTP SDK is missing. Runs the configure step only; it never builds anything.

#include <sys/wait.h>

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

namespace {

struct CommandResult {
  int exitCode;
  std::string output;
};

CommandResult run(const std::string& command) {
  CommandResult result{-1, {}};
  FILE* pipe = ::popen(command.c_str(), "r");
  if (pipe == nullptr) {
    return result;
  }
  char buffer[4096];
  size_t read = 0;
  while ((read = std::fread(buffer, 1, sizeof buffer, pipe)) > 0) {
    result.output.append(buffer, read);
  }
  const int status = ::pclose(pipe);
  if (WIFEXITED(status)) {
    result.exitCode = WEXITSTATUS(status);
  }
  return result;
}

std::string quote(const std::string& value) {
  return "\"" + value + "\"";
}

// Configures the project into a scratch build dir and reports whether it failed
// while naming CTP_HOME in the error output.
bool configFailsNamingCtpHome(const std::string& name, const std::string& envPrefix,
                              const std::string& extraArgs) {
  const std::filesystem::path buildDir = std::filesystem::path{ORS_TEST_BUILD_DIR} / name;
  std::error_code ignored;
  std::filesystem::remove_all(buildDir, ignored);

  const std::string command = envPrefix + quote(ORS_CTP_CMAKE_COMMAND) +
                              " -S " + quote(ORS_SOURCE_DIR) + " -B " + quote(buildDir.string()) +
                              " " + extraArgs + " 2>&1";
  const CommandResult result = run(command);

  if (result.exitCode == 0) {
    std::cerr << name << ": configure unexpectedly succeeded\n" << result.output << '\n';
    return false;
  }
  if (result.output.find("CTP_HOME") == std::string::npos) {
    std::cerr << name << ": configure failed without naming CTP_HOME\n" << result.output << '\n';
    return false;
  }
  return true;
}

}  // namespace

int main() {
  // No CTP_HOME at all, and CTP_HOME pointing at a directory without the SDK.
  const bool unsetPassed = configFailsNamingCtpHome("no_ctp_home", "env -u CTP_HOME ", "");
  const bool missingPassed =
      configFailsNamingCtpHome("bad_ctp_home", "env -u CTP_HOME ", "-DCTP_HOME=/nonexistent");

  if (!unsetPassed || !missingPassed) {
    return 1;
  }
  std::cout << "ctp_config: configure fails with a CTP_HOME message\n";
  return 0;
}
