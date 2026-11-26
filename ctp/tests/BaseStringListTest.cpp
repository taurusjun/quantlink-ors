#include <iostream>
#include <string>
#include <vector>

#include "ors/base/StringList.h"

int main() {
  const auto values = ors::base::splitList("au2612, ag2612  rb2601");
  const std::vector<std::string> expected{"au2612", "ag2612", "rb2601"};
  if (values != expected) {
    std::cerr << "FAIL: splitList\n";
    return 1;
  }
  if (!ors::base::splitList(" ,  ").empty()) {
    std::cerr << "FAIL: empty list\n";
    return 1;
  }
  std::cout << "base_string_list: comma/space split\n";
  return 0;
}
