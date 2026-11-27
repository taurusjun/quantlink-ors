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
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${RMS_VENDOR_ROOT}/common"
    "${RMS_VENDOR_ROOT}/core"
    "${RMS_VENDOR_ROOT}/security"
    "${CTP_INCLUDE_DIR}"
    "${SPDLOG_INCLUDE_DIR}")

# ---------------------------------------------------------------------------
# Libraries. Dependencies point downward only:
# base <- rms/md <- engine <- codec <- transport
# ---------------------------------------------------------------------------

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

add_library(ors_rms STATIC
    vendor/rms/tinyxml2.cpp
    vendor/rms/rmschecks.cpp)
target_include_directories(ors_rms PUBLIC ${ORS_INCLUDE_DIRS})
target_link_libraries(ors_rms PUBLIC ors_base)
target_compile_features(ors_rms PUBLIC cxx_std_20)
target_compile_definitions(ors_rms PUBLIC _USE_CTP_=1)
target_compile_options(ors_rms PRIVATE -Wall -Wextra -Wpedantic)


add_library(ors_engine STATIC
    core/ORSServer.cpp
    core/MdSpi.cpp
    core/MDHandler.cpp
    core/ORSServerCallbacks.cpp
    core/OrsConfig.cpp
    core/PositionTable.cpp
    control/ClientRegistry.cpp
    control/OpenOrderGuard.cpp
    execution/OffsetPolicy.cpp
    query/AccountQueryState.cpp
    query/QueryMapping.cpp
    log/Log.cpp
    transport/InMemoryOrsTransport.cpp)
target_include_directories(ors_engine PUBLIC ${ORS_INCLUDE_DIRS})
target_link_directories(ors_engine PUBLIC "${CTP_HOME}/lib")
target_link_libraries(ors_engine PUBLIC
    ors_base ors_rms
    thosttraderapi_se thostmduserapi_se
    Threads::Threads)
target_compile_features(ors_engine PUBLIC cxx_std_20)
target_compile_definitions(ors_engine PUBLIC _USE_CTP_=1)
target_compile_options(ors_engine PRIVATE -Wall -Wextra -Wpedantic)

# Compatibility facade for tests and consumers. It owns no sources.
add_library(ors_core INTERFACE)
target_include_directories(ors_core INTERFACE ${ORS_INCLUDE_DIRS})
target_link_libraries(ors_core INTERFACE ors_engine)
target_compile_features(ors_core INTERFACE cxx_std_20)

add_library(ors_codec STATIC contract/OrsCodec.cpp)
target_include_directories(ors_codec PUBLIC "${CONTRACT_GENERATED_DIR}")
target_link_libraries(ors_codec PUBLIC ors_core)
target_compile_features(ors_codec PUBLIC cxx_std_20)

add_library(ors_transport STATIC transport/AeronOrsTransport.cpp)
target_link_libraries(ors_transport PUBLIC ors_codec)
target_compile_features(ors_transport PUBLIC cxx_std_20)

# ---------------------------------------------------------------------------
# Aeron / executable targets.
# ---------------------------------------------------------------------------

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
  target_include_directories(ors_aeron_client PUBLIC
      "${AERON_CPP_INCLUDE}" "${AERON_C_INCLUDE}")
  target_link_libraries(ors_aeron_client PUBLIC ors_transport "${AERON_LIB}")
  target_compile_features(ors_aeron_client PUBLIC cxx_std_20)

  add_executable(ors_ctp main.cpp)
  target_link_libraries(ors_ctp PRIVATE ors_aeron_client ors_transport ors_core)
  target_compile_features(ors_ctp PRIVATE cxx_std_20)
  set_target_properties(ors_ctp PROPERTIES SKIP_BUILD_RPATH TRUE)

  add_executable(ors_live_smoke tools/live_smoke.cpp)
  target_link_libraries(ors_live_smoke PRIVATE ors_codec "${AERON_LIB}")
  target_include_directories(ors_live_smoke PRIVATE
      "${AERON_CPP_INCLUDE}" "${AERON_C_INCLUDE}")
  target_compile_features(ors_live_smoke PRIVATE cxx_std_20)
endif()

# ---------------------------------------------------------------------------
# Tests.
# ---------------------------------------------------------------------------

function(ors_add_test name source)
  cmake_parse_arguments(T "" "" "LIBS" ${ARGN})
  if(NOT T_LIBS)
    set(T_LIBS ors_core)
  endif()
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE ${T_LIBS})
  target_compile_features(${name} PRIVATE cxx_std_20)
  if(CTP_FRAMEWORK_DIR)
    set_target_properties(${name} PROPERTIES BUILD_RPATH "${CTP_FRAMEWORK_DIR}")
  endif()
  add_test(NAME ${name} COMMAND ${name})
endfunction()

ors_add_test(ctp_core_link tests/CoreLinkTest.cpp)
ors_add_test(ctp_position_table tests/PositionTableTest.cpp)
ors_add_test(ctp_log_facade tests/LogFacadeTest.cpp)
ors_add_test(ctp_client_registry tests/ClientRegistryTest.cpp)
ors_add_test(ctp_account_query_state tests/AccountQueryStateTest.cpp)
ors_add_test(ctp_open_order_guard tests/OpenOrderGuardTest.cpp)
ors_add_test(ctp_query_mapping tests/QueryMappingTest.cpp)
ors_add_test(ctp_ors_codec tests/OrsCodecTest.cpp LIBS ors_codec)
ors_add_test(ctp_aeron_transport tests/AeronOrsTransportTest.cpp LIBS ors_transport)
ors_add_test(ctp_md_handler tests/MDHandlerTest.cpp)
ors_add_test(ctp_offset_policy tests/OffsetPolicyTest.cpp)
ors_add_test(ctp_order_parity tests/OrderParityTest.cpp)
