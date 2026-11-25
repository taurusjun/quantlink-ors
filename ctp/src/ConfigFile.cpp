#include "ors/ctp/ConfigFile.h"

#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace ors::ctp {
namespace {

std::string trim(std::string_view value) {
  auto isSpace = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
  while (!value.empty() && isSpace(value.front())) {
    value.remove_prefix(1);
  }
  while (!value.empty() && isSpace(value.back())) {
    value.remove_suffix(1);
  }
  return std::string(value);
}

}  // namespace

ConfigFile::ConfigFile(std::string label, std::unordered_map<std::string, std::string> values)
    : label_(std::move(label)), values_(std::move(values)) {}

ConfigFile ConfigFile::load(const std::string& path, const std::string& label) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error(label + ": cannot open " + path);
  }

  std::unordered_map<std::string, std::string> values;
  std::string line;
  while (std::getline(in, line)) {
    const auto hash = line.find('#');
    if (hash != std::string::npos) {
      line = line.substr(0, hash);
    }
    const auto equals = line.find('=');
    if (equals == std::string::npos) {
      continue;
    }
    const std::string key = trim(std::string_view(line).substr(0, equals));
    if (!key.empty()) {
      values[key] = trim(std::string_view(line).substr(equals + 1));
    }
  }
  return ConfigFile(label, std::move(values));
}

std::string ConfigFile::get(const std::string& key) const {
  const auto found = values_.find(key);
  return found == values_.end() ? std::string() : found->second;
}

bool ConfigFile::contains(const std::string& key) const {
  return values_.find(key) != values_.end();
}

std::string ConfigFile::require(const std::string& key, const char* envName) const {
  const std::string value = get(key);
  if (!value.empty()) {
    return value;
  }
  std::string message = label_ + ": " + key + " is required";
  if (envName != nullptr) {
    message += std::string(" (file or env ") + envName + ")";
  }
  throw std::runtime_error(message);
}

}  // namespace ors::ctp
