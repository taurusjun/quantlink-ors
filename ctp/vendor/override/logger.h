#pragma once

#include <iostream>

// Task 1 compatibility shim: the company core still uses ILOG. Task 3 replaces
// every call with the ORS JSONL facade.
#define ILOG(level) std::cerr
#define ILOGQ(level, message) do { (void)(message); } while (0)
#define ILOGP std::cerr
#define LOG(level) std::cerr
#define LOGP std::cerr
#define RLOG(level) std::cerr
#define RLOGQ(level, message) do { (void)(message); } while (0)
