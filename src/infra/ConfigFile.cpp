#include "ors/base/ConfigFile.h"

#include <stdexcept>
#include <utility>

#include "ini.h"

namespace ors::base {
namespace {

struct ParseContext {
  std::unordered_map<std::string, std::string> values;
  std::string error;
};

int handleLine(void* user, const char* section, const char* name, const char* value) {
  auto* context = static_cast<ParseContext*>(user);
  if (section != nullptr && section[0] != '\0') {
    context->error = "section not supported";
    return 0;
  }
  if (name == nullptr || name[0] == '\0') {
    context->error = "empty key";
    return 0;
  }
  context->values[name] = value != nullptr ? value : "";
  return 1;
}

}  // namespace

ConfigFile::ConfigFile(std::string label, std::unordered_map<std::string, std::string> values)
    : label_(std::move(label)), values_(std::move(values)) {}

ConfigFile ConfigFile::load(const std::string& path, const std::string& label) {
  ParseContext context;
  const int result = ini_parse(path.c_str(), handleLine, &context);
  if (result < 0) {
    throw std::runtime_error(label + ": cannot open " + path);
  }
  if (result > 0) {
    throw std::runtime_error(label + ": parse error at line " + std::to_string(result) +
                             (context.error.empty() ? "" : " (" + context.error + ")"));
  }
  return ConfigFile(label, std::move(context.values));
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

}  // namespace ors::base
