#include "ors/ctp/TextEncoding.h"

#include <cerrno>
#include <iconv.h>

namespace ors::ctp {

std::string trimAsciiSpace(std::string_view input) {
  const std::size_t first = input.find_first_not_of(' ');
  if (first == std::string_view::npos) {
    return {};
  }
  const std::size_t last = input.find_last_not_of(' ');
  return std::string(input.substr(first, last - first + 1));
}

std::string gbkToUtf8(std::string_view input) {
  if (input.empty()) return {};
  iconv_t converter = iconv_open("UTF-8", "GB18030");
  if (converter == reinterpret_cast<iconv_t>(-1)) return std::string(input);

  std::string output;
  char buffer[256];
  char* source = const_cast<char*>(input.data());
  std::size_t sourceLeft = input.size();
  while (sourceLeft > 0) {
    char* target = buffer;
    std::size_t targetLeft = sizeof(buffer);
    const std::size_t result =
        iconv(converter, &source, &sourceLeft, &target, &targetLeft);
    output.append(buffer, static_cast<std::size_t>(target - buffer));
    if (result == static_cast<std::size_t>(-1)) {
      if (errno == E2BIG) continue;
      iconv_close(converter);
      return std::string(input);
    }
  }
  iconv_close(converter);
  return output;
}

}  // namespace ors::ctp
