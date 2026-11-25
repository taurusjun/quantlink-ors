#include <iostream>
#include <string>

#include "ors/ctp/TextEncoding.h"

int main() {
  const std::string gbk("\xB1\xA8\xB5\xA5\xD2\xD1\xCC\xE1\xBD\xBB");
  if (ors::ctp::gbkToUtf8(gbk) != "报单已提交") {
    std::cerr << "FAIL: GBK error message was not converted\n";
    return 1;
  }
  if (ors::ctp::gbkToUtf8("plain") != "plain") {
    std::cerr << "FAIL: ASCII message changed\n";
    return 1;
  }
  std::cout << "text_encoding: GBK -> UTF-8\n";
  return 0;
}
