find_package(CTP REQUIRED)

# CtpConfig has no SDK dependency, so its unit test runs without the CTP .so.
add_library(ors_ctp_config STATIC src/CtpConfig.cpp)
target_include_directories(ors_ctp_config PUBLIC include)

add_library(ors_ctp_core STATIC src/CtpTraderSession.cpp src/CtpMdSession.cpp
                                src/CtpQueryService.cpp)
target_include_directories(ors_ctp_core PUBLIC include)
target_link_libraries(ors_ctp_core PUBLIC ors_ctp_config PRIVATE CTP::trader CTP::md)

add_executable(ors_ctp src/main.cpp)
target_link_libraries(ors_ctp PRIVATE ors_ctp_core CTP::trader CTP::md)

add_executable(ctp_config_test tests/CtpConfigTest.cpp)
target_link_libraries(ctp_config_test PRIVATE ors_ctp_config)
add_test(NAME ctp_config COMMAND ctp_config_test)

# Only mapDepthMarketData() runs here: no login, no network.
add_executable(ctp_md_test tests/CtpMdSessionTest.cpp)
target_link_libraries(ctp_md_test PRIVATE ors_ctp_core)
add_test(NAME ctp_md COMMAND ctp_md_test)

# Row mapping and batch aggregation; the failure case uses an unstarted session,
# so no login and no network either.
add_executable(ctp_query_test tests/CtpQueryServiceTest.cpp)
target_link_libraries(ctp_query_test PRIVATE ors_ctp_core)
add_test(NAME ctp_query COMMAND ctp_query_test)

# Executes the binary, so a missing/incompatible CTP .so fails here and not
# only at link time.
add_test(NAME ctp_link_smoke COMMAND ors_ctp)

# The md-smoke mode is reachable in the shipped binary: without --config it
# stops at its argument check instead of doing anything on the network.
add_test(NAME ctp_md_mode_wired COMMAND ors_ctp --mode md-smoke)
set_tests_properties(ctp_md_mode_wired PROPERTIES
  PASS_REGULAR_EXPRESSION "--config is required for --mode md-smoke")

# Query smoke mode is wired the same way: argument check only, no network.
add_test(NAME ctp_query_mode_wired COMMAND ors_ctp --mode query-smoke)
set_tests_properties(ctp_query_mode_wired PROPERTIES
  PASS_REGULAR_EXPRESSION "--config is required for --mode query-smoke")
