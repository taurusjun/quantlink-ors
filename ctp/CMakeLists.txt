cmake_minimum_required(VERSION 3.25)

find_package(CTP REQUIRED)
find_package(Threads REQUIRED)

set(CONTRACT_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/contract" CACHE PATH
    "Directory produced by tools/gen_contract_cpp.sh")
if(NOT EXISTS "${CONTRACT_GENERATED_DIR}/com_quantlink_contract_sbe/OrsRequest.h")
  message(WARNING "C++ contract headers missing; run tools/gen_contract_cpp.sh")
endif()

find_path(SPDLOG_INCLUDE_DIR NAMES spdlog/spdlog.h HINTS /root/quantlink-trading/tbadmin/inc)
if(NOT SPDLOG_INCLUDE_DIR)
  message(FATAL_ERROR "SPDLOG_INCLUDE_DIR is required (spdlog/spdlog.h)")
endif()

set(RMS_VENDOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vendor/rms")
set(ORS_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${RMS_VENDOR_ROOT}/common"
    "${RMS_VENDOR_ROOT}/core"
    "${RMS_VENDOR_ROOT}/security"
    "${CTP_INCLUDE_DIR}"
    "${SPDLOG_INCLUDE_DIR}")

add_library(ors_base STATIC
    src/base/ConfigFile.cpp
    src/base/ContractText.cpp
    src/base/StringList.cpp
    src/base/Log.cpp
    ../third_party/inih/ini.c)
target_include_directories(ors_base PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/inih"
    "${SPDLOG_INCLUDE_DIR}")
target_link_libraries(ors_base PUBLIC Threads::Threads)
target_compile_features(ors_base PUBLIC cxx_std_20)
target_compile_definitions(ors_base PRIVATE
    INI_ALLOW_INLINE_COMMENTS=0
    INI_ALLOW_MULTILINE=0
    INI_ALLOW_NO_VALUE=0
    INI_MAX_LINE=4096)

add_library(ors_core STATIC
    core/ORSServer.cpp
    core/RuntimeGlobals.cpp
    core/ORSServerCallbacks.cpp
    core/MdSpi.cpp
    core/MDHandler.cpp
    core/OrsConfig.cpp
    core/PositionTable.cpp
    control/ClientRegistry.cpp
    query/AccountQueryState.cpp
    query/QueryMapping.cpp
    control/OpenOrderGuard.cpp
    log/Log.cpp
    transport/InMemoryOrsTransport.cpp
    vendor/rms/tinyxml2.cpp
    vendor/rms/rmschecks.cpp)
target_include_directories(ors_core PUBLIC ${ORS_INCLUDE_DIRS})
target_link_directories(ors_core PUBLIC "${CTP_HOME}/lib")
target_link_libraries(ors_core PUBLIC ors_base thosttraderapi_se thostmduserapi_se Threads::Threads)
target_compile_features(ors_core PUBLIC cxx_std_20)
target_compile_definitions(ors_core PUBLIC _USE_CTP_=1)

add_library(ors_codec STATIC contract/OrsCodec.cpp)
target_include_directories(ors_codec PUBLIC "${CONTRACT_GENERATED_DIR}")
target_link_libraries(ors_codec PUBLIC ors_core)
target_compile_features(ors_codec PUBLIC cxx_std_20)

set(AERON_HOME "" CACHE PATH "Aeron source root")
set(AERON_RUNTIME_HOME "" CACHE PATH "Aeron runtime lib root")
if(AERON_HOME AND AERON_RUNTIME_HOME)
  find_path(AERON_CPP_INCLUDE NAMES Aeron.h
      PATHS "${AERON_HOME}/aeron-client/src/main/cpp_wrapper" REQUIRED)
  find_path(AERON_C_INCLUDE NAMES aeronc.h
      PATHS "${AERON_HOME}/aeron-client/src/main/c" REQUIRED)
  find_library(AERON_LIB NAMES aeron
      PATHS "${AERON_RUNTIME_HOME}/lib" REQUIRED)
  add_library(ors_aeron_client STATIC transport/AeronClient.cpp)
  add_executable(ors_ctp main.cpp)
  target_link_libraries(ors_ctp PRIVATE ors_aeron_client ors_transport ors_core)
  target_compile_features(ors_ctp PRIVATE cxx_std_20)
  set_target_properties(ors_ctp PROPERTIES SKIP_BUILD_RPATH TRUE)

  add_executable(ors_live_smoke tools/live_smoke.cpp)
  target_link_libraries(ors_live_smoke PRIVATE ors_codec "${AERON_LIB}")
  target_include_directories(ors_live_smoke PRIVATE
      "${AERON_CPP_INCLUDE}" "${AERON_C_INCLUDE}")
  target_compile_features(ors_live_smoke PRIVATE cxx_std_20)
  target_include_directories(ors_aeron_client PUBLIC
      "${AERON_CPP_INCLUDE}" "${AERON_C_INCLUDE}")
  target_link_libraries(ors_aeron_client PUBLIC ors_transport "${AERON_LIB}")
  target_compile_features(ors_aeron_client PUBLIC cxx_std_20)
endif()

add_library(ors_transport STATIC transport/AeronOrsTransport.cpp)
target_link_libraries(ors_transport PUBLIC ors_codec)
target_compile_features(ors_transport PUBLIC cxx_std_20)

add_executable(ctp_core_link_test tests/CoreLinkTest.cpp)
target_link_libraries(ctp_core_link_test PRIVATE ors_core)
target_compile_features(ctp_core_link_test PRIVATE cxx_std_20)
set(CTP_FRAMEWORK_DIR "" CACHE PATH "macOS CTP framework directory")
if(CTP_FRAMEWORK_DIR)
  set_target_properties(ctp_core_link_test PROPERTIES BUILD_RPATH "${CTP_FRAMEWORK_DIR}")
endif()
add_test(NAME ctp_core_link COMMAND ctp_core_link_test)

add_executable(ctp_position_table_test tests/PositionTableTest.cpp)
target_link_libraries(ctp_position_table_test PRIVATE ors_core)
target_compile_features(ctp_position_table_test PRIVATE cxx_std_20)
if(CTP_FRAMEWORK_DIR)
  set_target_properties(ctp_position_table_test PROPERTIES BUILD_RPATH "${CTP_FRAMEWORK_DIR}")
endif()
if(CTP_FRAMEWORK_DIR)
  set_target_properties(ctp_position_table_test PROPERTIES BUILD_RPATH "${CTP_FRAMEWORK_DIR}")
endif()
add_test(NAME ctp_position_table COMMAND ctp_position_table_test)

add_executable(ctp_log_facade_test tests/LogFacadeTest.cpp)
target_link_libraries(ctp_log_facade_test PRIVATE ors_core)
target_compile_features(ctp_log_facade_test PRIVATE cxx_std_20)
if(CTP_FRAMEWORK_DIR)
  set_target_properties(ctp_log_facade_test PROPERTIES BUILD_RPATH "${CTP_FRAMEWORK_DIR}")
endif()
add_test(NAME ctp_log_facade COMMAND ctp_log_facade_test)

add_executable(ctp_client_registry_test tests/ClientRegistryTest.cpp)
target_link_libraries(ctp_client_registry_test PRIVATE ors_core)
target_compile_features(ctp_client_registry_test PRIVATE cxx_std_20)
if(CTP_FRAMEWORK_DIR)
  set_target_properties(ctp_client_registry_test PROPERTIES BUILD_RPATH "${CTP_FRAMEWORK_DIR}")
endif()
add_test(NAME ctp_client_registry COMMAND ctp_client_registry_test)

add_executable(ctp_account_query_state_test tests/AccountQueryStateTest.cpp)
target_link_libraries(ctp_account_query_state_test PRIVATE ors_core)
target_compile_features(ctp_account_query_state_test PRIVATE cxx_std_20)
if(CTP_FRAMEWORK_DIR)
  set_target_properties(ctp_account_query_state_test PROPERTIES BUILD_RPATH "${CTP_FRAMEWORK_DIR}")
endif()
add_test(NAME ctp_account_query_state COMMAND ctp_account_query_state_test)

add_executable(ctp_open_order_guard_test tests/OpenOrderGuardTest.cpp)
target_link_libraries(ctp_open_order_guard_test PRIVATE ors_core)
target_compile_features(ctp_open_order_guard_test PRIVATE cxx_std_20)
if(CTP_FRAMEWORK_DIR)
  set_target_properties(ctp_open_order_guard_test PROPERTIES BUILD_RPATH "${CTP_FRAMEWORK_DIR}")
endif()
add_test(NAME ctp_open_order_guard COMMAND ctp_open_order_guard_test)

add_executable(ctp_query_mapping_test tests/QueryMappingTest.cpp)
target_link_libraries(ctp_query_mapping_test PRIVATE ors_core)
target_compile_features(ctp_query_mapping_test PRIVATE cxx_std_20)
if(CTP_FRAMEWORK_DIR)
  set_target_properties(ctp_query_mapping_test PROPERTIES BUILD_RPATH "${CTP_FRAMEWORK_DIR}")
endif()
add_test(NAME ctp_query_mapping COMMAND ctp_query_mapping_test)

add_executable(ctp_ors_codec_test tests/OrsCodecTest.cpp)
target_link_libraries(ctp_ors_codec_test PRIVATE ors_codec)
target_compile_features(ctp_ors_codec_test PRIVATE cxx_std_20)
if(CTP_FRAMEWORK_DIR)
  set_target_properties(ctp_ors_codec_test PROPERTIES BUILD_RPATH "${CTP_FRAMEWORK_DIR}")
endif()
add_test(NAME ctp_ors_codec COMMAND ctp_ors_codec_test)

add_executable(ctp_aeron_transport_test tests/AeronOrsTransportTest.cpp)
target_link_libraries(ctp_aeron_transport_test PRIVATE ors_transport)
target_compile_features(ctp_aeron_transport_test PRIVATE cxx_std_20)
if(CTP_FRAMEWORK_DIR)
  set_target_properties(ctp_aeron_transport_test PROPERTIES BUILD_RPATH "${CTP_FRAMEWORK_DIR}")
endif()
add_test(NAME ctp_aeron_transport COMMAND ctp_aeron_transport_test)

add_executable(ctp_md_handler_test tests/MDHandlerTest.cpp)
target_link_libraries(ctp_md_handler_test PRIVATE ors_core)
target_compile_features(ctp_md_handler_test PRIVATE cxx_std_20)
if(CTP_FRAMEWORK_DIR)
  set_target_properties(ctp_md_handler_test PROPERTIES BUILD_RPATH "${CTP_FRAMEWORK_DIR}")
endif()
add_test(NAME ctp_md_handler COMMAND ctp_md_handler_test)

add_executable(ctp_order_parity_test tests/OrderParityTest.cpp)
target_link_libraries(ctp_order_parity_test PRIVATE ors_core)
target_compile_features(ctp_order_parity_test PRIVATE cxx_std_20)
if(CTP_FRAMEWORK_DIR)
  set_target_properties(ctp_order_parity_test PROPERTIES BUILD_RPATH "${CTP_FRAMEWORK_DIR}")
endif()
add_test(NAME ctp_order_parity COMMAND ctp_order_parity_test)
