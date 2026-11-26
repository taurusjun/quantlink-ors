#include "ors/base/StringList.h"

namespace ors::base {

std::vector<std::string> splitList(std::string_view value) {
  std::vector<std::string> items;
  std::string current;
  for (const char character : value) {
    if (character == ',' || character == ' ') {
      if (!current.empty()) {
        items.push_back(current);
        current.clear();
      }
    } else {
      current.push_back(character);
    }
  }
  if (!current.empty()) {
    items.push_back(current);
  }
  return items;
}

}  // namespace ors::base
