#pragma once

#include <string>
#include <unordered_map>

namespace ors::base {

/// Strict key=value configuration backed by vendored inih.
class ConfigFile {
 public:
  static ConfigFile load(const std::string& path, const std::string& label);

  std::string get(const std::string& key) const;
  bool contains(const std::string& key) const;
  std::string require(const std::string& key, const char* envName = nullptr) const;

 private:
  ConfigFile(std::string label, std::unordered_map<std::string, std::string> values);

  std::string label_;
  std::unordered_map<std::string, std::string> values_;
};

}  // namespace ors::base
