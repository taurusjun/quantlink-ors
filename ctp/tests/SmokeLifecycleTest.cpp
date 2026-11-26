// SmokeLifecycle unit test: one login, ordered cases, fail stops the run.

#include <iostream>
#include <string>
#include <vector>

#include "ors/ctp/SmokeLifecycle.h"

namespace {

using ors::ctp::SmokeLifecycleVenue;
using ors::ctp::runSmokeLifecycle;

int failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

struct FakeVenue final : SmokeLifecycleVenue {
  std::vector<std::string> calls;
  int logins{0};
  bool cancelOk{true};
  bool fillOk{true};
  bool queryOk{true};

  bool login() override {
    ++logins;
    calls.push_back("login");
    return true;
  }
  bool cancelCase() override {
    calls.push_back("cancel");
    return cancelOk;
  }
  bool fillCase() override {
    calls.push_back("fill");
    return fillOk;
  }
  bool querySnapshots() override {
    calls.push_back("query");
    return queryOk;
  }
};

void checkHappyPathUsesOneLogin() {
  FakeVenue venue;
  check(runSmokeLifecycle(venue), "happy path succeeds");
  check(venue.logins == 1, "login happens exactly once");
  check(venue.calls == std::vector<std::string>{"login", "cancel", "fill", "query"},
        "login -> cancel -> fill -> query");
}

void checkFailureStopsRun() {
  FakeVenue venue;
  venue.cancelOk = false;
  check(!runSmokeLifecycle(venue), "cancel failure fails the run");
  check(venue.logins == 1, "cancel failure still used one login");
  check(venue.calls == std::vector<std::string>{"login", "cancel"},
        "fill/query do not run after cancel failure");
}

}  // namespace

int main() {
  checkHappyPathUsesOneLogin();
  checkFailureStopsRun();
  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "smoke lifecycle: one login, ordered cases\n";
  return 0;
}
