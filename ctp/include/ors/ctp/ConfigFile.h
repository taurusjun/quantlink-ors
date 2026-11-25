#pragma once

#include <string>
#include <unordered_map>

namespace ors::ctp {

/// Strict key=value configuration shared by the CTP and ORS runtime loaders.
/// Comment (`#`) and blank lines are ignored; a later duplicate key wins.
class ConfigFile {
 public:
  static ConfigFile load(const std::string& path, const std::string& label);

  /// Empty when the key is absent.
  std::string get(const std::string& key) const;
  bool contains(const std::string& key) const;

  /// Throws when the key is absent or empty. The message names the key and
  /// never echoes values.
  std::string require(const std::string& key, const char* envName = nullptr) const;

 private:
  ConfigFile(std::string label, std::unordered_map<std::string, std::string> values);

  std::string label_;
  std::unordered_map<std::string, std::string> values_;
};

}  // namespace ors::ctp
