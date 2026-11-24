find_package(CTP REQUIRED)

set(CONTRACT_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/contract" CACHE PATH
    "Directory produced by tools/gen_contract_cpp.sh")

# SBE codec + Aeron publisher adapter; generated headers are build artifacts.
add_library(ors_contract STATIC src/ContractSink.cpp)
target_include_directories(ors_contract PUBLIC include "${CONTRACT_GENERATED_DIR}")
target_compile_features(ors_contract PUBLIC cxx_std_20)

find_package(Threads REQUIRED)
set(AERON_HOME "/opt/quantlink-fin/aeron-src" CACHE PATH "Aeron source/build tree")
find_path(AERON_CPP_INCLUDE NAMES Aeron.h
    PATHS "${AERON_HOME}/aeron-client/src/main/cpp_wrapper" REQUIRED)
find_path(AERON_C_INCLUDE NAMES aeronc.h
    PATHS "${AERON_HOME}/aeron-client/src/main/c" REQUIRED)
find_library(AERON_STATIC_LIB NAMES aeron_static
    PATHS "${AERON_HOME}/build/lib" REQUIRED)

add_library(ors_aeron STATIC src/AeronPublisher.cpp)
target_include_directories(ors_aeron PUBLIC include "${AERON_CPP_INCLUDE}" "${AERON_C_INCLUDE}")
target_link_libraries(ors_aeron PUBLIC ors_contract "${AERON_STATIC_LIB}"
    Threads::Threads ${CMAKE_DL_LIBS} rt)
target_compile_features(ors_aeron PUBLIC cxx_std_20)

# CtpConfig has no SDK dependency, so its unit test runs without the CTP .so.
add_library(ors_ctp_config STATIC src/CtpConfig.cpp)
target_include_directories(ors_ctp_config PUBLIC include)

add_library(ors_ctp_core STATIC src/CtpTraderSession.cpp src/CtpMdSession.cpp
                                src/CtpQueryService.cpp src/CtpOrderRefMap.cpp
                                src/CtpAdapter.cpp src/CtpReconciler.cpp)
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
add_test(NAME ctp_md_mode_wired COMMAND ors_ctp --mode md-smoke)
set_tests_properties(ctp_md_mode_wired PROPERTIES
  PASS_REGULAR_EXPRESSION "--config is required for --mode md-smoke")

# Query smoke mode is wired the same way: argument check only, no network.
add_test(NAME ctp_query_mode_wired COMMAND ors_ctp --mode query-smoke)
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

add_executable(ctp_reconciler_test tests/CtpReconcilerTest.cpp)
target_link_libraries(ctp_reconciler_test PRIVATE ors_ctp_core)
add_test(NAME ctp_reconciler COMMAND ctp_reconciler_test)

# order-smoke is dry-run unless --live, so ctest can reach the mode without a
# config, a login or an order.
add_test(NAME ctp_order_mode_wired COMMAND ors_ctp --mode order-smoke --instruments au2612
                                           --price 500 --qty 1)
set_tests_properties(ctp_order_mode_wired PROPERTIES PASS_REGULAR_EXPRESSION "DRY-RUN")

add_test(NAME ctp_order_mode_requires_config COMMAND ors_ctp --mode order-smoke --live
                                                   --instruments au2612 --price 500 --qty 1)
set_tests_properties(ctp_order_mode_requires_config PROPERTIES
  PASS_REGULAR_EXPRESSION "--config is required for --mode order-smoke")

# SBE codec and stream ownership; no CTP SDK, no network.
add_executable(ctp_contract_sink_test tests/ContractSinkTest.cpp)
target_link_libraries(ctp_contract_sink_test PRIVATE ors_contract)
add_test(NAME ctp_contract_sink COMMAND ctp_contract_sink_test)
