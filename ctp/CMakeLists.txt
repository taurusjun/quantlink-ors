find_package(CTP REQUIRED)

set(CONTRACT_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/contract" CACHE PATH
    "Directory produced by tools/gen_contract_cpp.sh")

# SBE codec + Aeron publisher adapter; generated headers are build artifacts.
add_library(ors_contract STATIC src/ContractSink.cpp)
target_include_directories(ors_contract PUBLIC include "${CONTRACT_GENERATED_DIR}")
target_compile_features(ors_contract PUBLIC cxx_std_20)

find_package(Threads REQUIRED)
find_path(SPDLOG_INCLUDE_DIR NAMES spdlog/spdlog.h
    HINTS /root/quantlink-trading/tbadmin/inc)
if(NOT SPDLOG_INCLUDE_DIR)
  message(FATAL_ERROR "SPDLOG_INCLUDE_DIR is required (spdlog/spdlog.h)")
endif()

add_library(ors_base STATIC
    src/base/ConfigFile.cpp
    src/base/StringList.cpp
    src/base/Log.cpp
    ../third_party/inih/ini.c)
target_include_directories(ors_base PUBLIC include ../third_party/inih "${SPDLOG_INCLUDE_DIR}")
target_link_libraries(ors_base PUBLIC Threads::Threads)
target_compile_features(ors_base PUBLIC cxx_std_20)
target_compile_definitions(ors_base PRIVATE
    INI_ALLOW_INLINE_COMMENTS=0
    INI_ALLOW_MULTILINE=0
    INI_ALLOW_NO_VALUE=0
    INI_MAX_LINE=4096)

set(AERON_HOME "" CACHE PATH "Aeron source headers; must be supplied")
set(AERON_RUNTIME_HOME "" CACHE PATH "Aeron runtime lib directory; must be supplied")
if(NOT AERON_HOME AND DEFINED ENV{AERON_HOME})
  set(AERON_HOME "$ENV{AERON_HOME}" CACHE PATH "Aeron source headers" FORCE)
endif()
if(NOT AERON_RUNTIME_HOME AND DEFINED ENV{AERON_RUNTIME_HOME})
  set(AERON_RUNTIME_HOME "$ENV{AERON_RUNTIME_HOME}" CACHE PATH "Aeron runtime lib" FORCE)
endif()
if(NOT AERON_HOME OR NOT AERON_RUNTIME_HOME)
  message(FATAL_ERROR "AERON_HOME and AERON_RUNTIME_HOME are required")
endif()
find_path(AERON_CPP_INCLUDE NAMES Aeron.h
    PATHS "${AERON_HOME}/aeron-client/src/main/cpp_wrapper" REQUIRED)
find_path(AERON_C_INCLUDE NAMES aeronc.h
    PATHS "${AERON_HOME}/aeron-client/src/main/c" REQUIRED)
find_library(AERON_LIB NAMES aeron
    PATHS "${AERON_RUNTIME_HOME}/lib" REQUIRED)

add_library(ors_aeron STATIC src/AeronPublisher.cpp src/AeronCommandSubscriber.cpp src/ContractRuntime.cpp)
target_include_directories(ors_aeron PUBLIC include "${AERON_CPP_INCLUDE}" "${AERON_C_INCLUDE}")
target_link_libraries(ors_aeron PUBLIC ors_contract ors_ctp_core "${AERON_LIB}"
    Threads::Threads ${CMAKE_DL_LIBS} rt)
target_compile_features(ors_aeron PUBLIC cxx_std_20)

# CtpConfig has no SDK dependency, so its unit test runs without the CTP .so.
add_library(ors_ctp_config STATIC src/CtpConfig.cpp src/RuntimeConfig.cpp)
target_include_directories(ors_ctp_config PUBLIC include)
target_link_libraries(ors_ctp_config PUBLIC ors_base)

add_library(ors_ctp_core STATIC src/CtpTraderSession.cpp src/CtpMdSession.cpp
                                src/CtpQueryService.cpp src/CtpOrderRefMap.cpp
                                src/CtpAdapter.cpp src/SmokeLifecycle.cpp
                                src/TextEncoding.cpp)
target_include_directories(ors_ctp_core PUBLIC include)
target_link_libraries(ors_ctp_core PUBLIC ors_ctp_config PRIVATE CTP::trader CTP::md)
find_library(ICONV_LIBRARY NAMES iconv)
if(ICONV_LIBRARY)
  target_link_libraries(ors_ctp_core PRIVATE "${ICONV_LIBRARY}")
endif()

add_executable(ors_ctp src/main.cpp)
target_link_libraries(ors_ctp PRIVATE ors_ctp_core ors_aeron CTP::trader CTP::md
    "${AERON_LIB}" Threads::Threads ${CMAKE_DL_LIBS} rt)

# Smoke tools are deliberately separate from the production runtime binary.
add_executable(ors_ctp_smoke src/smoke/SmokeMain.cpp)
target_link_libraries(ors_ctp_smoke PRIVATE ors_ctp_core CTP::trader CTP::md)

add_executable(ctp_text_encoding_test tests/TextEncodingTest.cpp)
target_link_libraries(ctp_text_encoding_test PRIVATE ors_ctp_core)
add_test(NAME ctp_text_encoding COMMAND ctp_text_encoding_test)

add_executable(ctp_config_test tests/CtpConfigTest.cpp)
target_link_libraries(ctp_config_test PRIVATE ors_ctp_config)
add_test(NAME ctp_config COMMAND ctp_config_test)

add_executable(ctp_base_config_test tests/BaseConfigTest.cpp)
target_link_libraries(ctp_base_config_test PRIVATE ors_base)
add_test(NAME ctp_base_config COMMAND ctp_base_config_test)

add_executable(ctp_base_string_list_test tests/BaseStringListTest.cpp)
target_link_libraries(ctp_base_string_list_test PRIVATE ors_base)
add_test(NAME ctp_base_string_list COMMAND ctp_base_string_list_test)

add_executable(ctp_base_log_test tests/BaseLogTest.cpp)
target_link_libraries(ctp_base_log_test PRIVATE ors_base)
add_test(NAME ctp_base_log COMMAND ctp_base_log_test)

add_executable(ctp_runtime_config_test tests/RuntimeConfigTest.cpp)
target_link_libraries(ctp_runtime_config_test PRIVATE ors_ctp_config)
add_test(NAME ctp_runtime_config COMMAND ctp_runtime_config_test)

add_executable(ctp_price_sanitize_test tests/PriceSanitizeTest.cpp)
target_include_directories(ctp_price_sanitize_test PRIVATE include)
add_test(NAME ctp_price_sanitize COMMAND ctp_price_sanitize_test)

# Only mapDepthMarketData() runs here: no login, no network.
add_executable(ctp_md_test tests/CtpMdSessionTest.cpp)
target_link_libraries(ctp_md_test PRIVATE ors_ctp_core)
add_test(NAME ctp_md COMMAND ctp_md_test)

# Row mapping and batch aggregation; the failure case uses an unstarted session,
# so no login and no network either. The test builds real CTP rows, so it needs
# the SDK include path: CTP::trader is PRIVATE to ors_ctp_core and does not
# propagate it.
add_executable(ctp_query_test tests/CtpQueryServiceTest.cpp)
target_link_libraries(ctp_query_test PRIVATE ors_ctp_core CTP::trader)
add_test(NAME ctp_query COMMAND ctp_query_test)

# Executes the binary, so a missing/incompatible CTP .so fails here and not
# only at link time.
add_test(NAME ctp_link_smoke COMMAND ors_ctp)

# The md-smoke mode is reachable in the shipped binary: without --config it
# stops at its argument check instead of doing anything on the network.
add_test(NAME ctp_md_mode_wired COMMAND ors_ctp_smoke --mode md-smoke)
set_tests_properties(ctp_md_mode_wired PROPERTIES
  PASS_REGULAR_EXPRESSION "--config is required for --mode md-smoke")

# Query smoke mode is wired the same way: argument check only, no network.
add_test(NAME ctp_query_mode_wired COMMAND ors_ctp_smoke --mode query-smoke)
set_tests_properties(ctp_query_mode_wired PROPERTIES
  PASS_REGULAR_EXPRESSION "--config is required for --mode query-smoke")

# OrderRef limit, duplicate rejection and resolve; no login either.
add_executable(ctp_order_ref_map_test tests/CtpOrderRefMapTest.cpp)
target_link_libraries(ctp_order_ref_map_test PRIVATE ors_ctp_core)
add_test(NAME ctp_order_ref_map COMMAND ctp_order_ref_map_test)

# Insert/cancel mapping and execution reports against a fake sender. The test
# builds real CTP rows, so it needs the SDK include path that CTP::trader adds.
add_executable(ctp_adapter_test tests/CtpAdapterTest.cpp)
target_link_libraries(ctp_adapter_test PRIVATE ors_ctp_core CTP::trader)
add_test(NAME ctp_adapter COMMAND ctp_adapter_test)

add_executable(ctp_smoke_lifecycle_test tests/SmokeLifecycleTest.cpp)
target_link_libraries(ctp_smoke_lifecycle_test PRIVATE ors_ctp_core)
add_test(NAME ctp_smoke_lifecycle COMMAND ctp_smoke_lifecycle_test)

# order-smoke is dry-run unless --live, so ctest can reach the mode without a
# config, a login or an order.
add_test(NAME ctp_order_mode_wired COMMAND ors_ctp_smoke --mode order-smoke --instruments au2612
                                           --price 500 --qty 1)
set_tests_properties(ctp_order_mode_wired PROPERTIES PASS_REGULAR_EXPRESSION "DRY-RUN")

add_test(NAME ctp_order_mode_requires_config COMMAND ors_ctp_smoke --mode order-smoke --live
                                                   --instruments au2612 --price 500 --qty 1)
set_tests_properties(ctp_order_mode_requires_config PROPERTIES
  PASS_REGULAR_EXPRESSION "--config is required for --mode order-smoke")

# cancel-reject-smoke is live-only; without --live it must stop before login.
add_test(NAME ctp_cancel_reject_mode_wired COMMAND ors_ctp_smoke --mode cancel-reject-smoke
                                                   --instruments au2612 --price 735 --qty 1)
set_tests_properties(ctp_cancel_reject_mode_wired PROPERTIES
  PASS_REGULAR_EXPRESSION "--live is required for --mode cancel-reject-smoke")

# SBE codec and stream ownership; no CTP SDK, no network.
add_executable(ctp_contract_sink_test tests/ContractSinkTest.cpp)
target_link_libraries(ctp_contract_sink_test PRIVATE ors_contract)
add_test(NAME ctp_contract_sink COMMAND ctp_contract_sink_test)
