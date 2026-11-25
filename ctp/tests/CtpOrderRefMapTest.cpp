// CtpOrderRefMap tests: the twelve-digit CTP OrderRef limit, duplicate
// rejection, and resolving a venue report back to the contract orderRef.

#include <iostream>
#include <string>

#include "ors/ctp/CtpOrderRefMap.h"

namespace {

using ors::ctp::CtpOrderRefMap;
using ors::ctp::kMaxCtpOrderRef;

int failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

void checkFormatting() {
  CtpOrderRefMap refs;
  check(refs.add(1), "orderRef 1 is accepted");
  check(refs.ctpRef(1) == "1", "orderRef 1 maps to the CTP string \"1\"");

  check(refs.add(kMaxCtpOrderRef), "999999999999 is accepted");
  check(refs.ctpRef(kMaxCtpOrderRef) == "999999999999",
        "the largest orderRef is exactly twelve digits");
  check(refs.ctpRef(kMaxCtpOrderRef).size() == 12, "the CTP OrderRef is twelve digits");

  check(!refs.add(kMaxCtpOrderRef + 1), "1000000000000 is rejected");
  check(refs.ctpRef(kMaxCtpOrderRef + 1).empty(), "a rejected orderRef has no CTP OrderRef");
  check(!refs.add(0), "orderRef 0 is rejected");
  check(refs.size() == 2, "rejected refs are not registered");
}

void checkDuplicates() {
  CtpOrderRefMap refs;
  check(refs.add(42), "first registration succeeds");
  check(!refs.add(42), "a duplicate orderRef is rejected");
  check(refs.ctpRef(42) == "42", "a duplicate never changes the existing mapping");
  check(refs.size() == 1, "a duplicate adds nothing");
}

void checkResolve() {
  CtpOrderRefMap refs;
  check(refs.add(7), "orderRef 7 is registered");
  check(refs.resolve(0, 0, "7") == 7, "an unbound ref resolves without front/session");

  check(!refs.bind(7, 0, 0), "binding without a front/session is refused");
  check(refs.bind(7, 3, 400), "the accepting front/session is bound");
  check(refs.resolve(3, 400, "7") == 7, "the bound triple resolves");
  check(refs.resolve(0, 0, "7") == 7, "a known front/session still matches a bare lookup");
  check(refs.resolve(4, 400, "7") == 0, "a different front does not resolve");
  check(refs.resolve(3, 401, "7") == 0, "a different session does not resolve");
  check(refs.resolve(3, 400, "8") == 0, "an unknown CTP OrderRef resolves to 0");

  check(!refs.bind(7, 4, 401), "a second session cannot take over an order");
  check(refs.resolve(3, 400, "7") == 7, "the first binding survives");
}

void checkClear() {
  CtpOrderRefMap refs;
  check(refs.add(9), "orderRef 9 is registered");
  check(refs.bind(9, 1, 2), "orderRef 9 is bound");
  refs.clear();
  check(refs.size() == 0, "clear drops every mapping");
  check(refs.resolve(1, 2, "9") == 0, "a cleared ref no longer resolves");
  check(refs.ctpRef(9).empty(), "a cleared ref has no CTP OrderRef");
}

}  // namespace

int main() {
  checkFormatting();
  checkDuplicates();
  checkResolve();
  checkClear();

  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "ctp_order_ref_map: twelve-digit limit, duplicates, resolve\n";
  return 0;
}
