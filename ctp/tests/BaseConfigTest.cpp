#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "ors/base/ConfigFile.h"

namespace {
using ors::base::ConfigFile;
int failures = 0;
void check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}
std::string write(const std::filesystem::path& path, const std::string& text) {
  std::ofstream out(path, std::ios::trunc);
  out << text;
  return path.string();
}
template <typename Fn>
bool throwsContaining(Fn fn, const std::string& needle) {
  try {
    fn();
  } catch (const std::runtime_error& error) {
    return std::string(error.what()).find(needle) != std::string::npos;
  }
  return false;
}
}  // namespace

int main() {
  const auto dir = std::filesystem::temp_directory_path() / "ors_base_config_test";
  std::error_code ignored;
  std::filesystem::remove_all(dir, ignored);
  std::filesystem::create_directories(dir);

  const std::string full = write(dir / "full.ini",
                                 "A = one\n"
                                 "B=two\n"
                                 "PASSWORD = abc#def\n"
                                 "DUP = first\n"
                                 "DUP = last\n"
                                 "# comment\n"
                                 "   ; indented comment\n");
  const ConfigFile config = ConfigFile::load(full, "test");
  check(config.get("A") == "one", "spaced key/value");
  check(config.get("B") == "two", "compact key/value");
  check(config.get("PASSWORD") == "abc#def", "inline # is preserved");
  check(config.get("DUP") == "last", "later duplicate wins");
  check(config.require("A") == "one", "require returns value");
  check(throwsContaining([&] { config.require("MISSING"); }, "MISSING"),
        "missing key names the key");

  const std::string section = write(dir / "section.ini", "[rm]\nKEY = value\n");
  check(throwsContaining([&] { ConfigFile::load(section, "test"); }, "section"),
        "section is rejected");

  const std::string bad = write(dir / "bad.ini", "NO_EQUALS\n");
  check(throwsContaining([&] { ConfigFile::load(bad, "test"); }, "parse error"),
        "malformed line reports parse error");
  check(throwsContaining([&] { ConfigFile::load(dir / "absent.ini", "test"); },
                         "cannot open"),
        "missing file is reported");

  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "base_config: inih keys, comments, section rejection\n";
  return 0;
}
