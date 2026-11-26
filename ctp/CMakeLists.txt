cmake_minimum_required(VERSION 3.25)

find_package(CTP REQUIRED)
find_package(Threads REQUIRED)

find_path(SPDLOG_INCLUDE_DIR NAMES spdlog/spdlog.h HINTS /root/quantlink-trading/tbadmin/inc)
if(NOT SPDLOG_INCLUDE_DIR)
  message(FATAL_ERROR "SPDLOG_INCLUDE_DIR is required (spdlog/spdlog.h)")
endif()

set(HFTBASE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vendor/hftbase")
set(ORS_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/override"
    "${HFTBASE_ROOT}/CommonUtils/include"
    "${HFTBASE_ROOT}/RMS_NEW/include"
    "${HFTBASE_ROOT}/LibSecurity/include"
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
    core/MdSpi.cpp
    core/MDHandler.cpp
    core/OrsConfig.cpp
    core/PositionTable.cpp
    control/ClientRegistry.cpp
    query/AccountQueryState.cpp
    log/Log.cpp
    transport/InMemoryOrsTransport.cpp
    vendor/hftbase/tinyxml2.cpp)
target_include_directories(ors_core PUBLIC ${ORS_INCLUDE_DIRS})
target_link_libraries(ors_core PUBLIC ors_base CTP::trader CTP::md Threads::Threads)
target_compile_features(ors_core PUBLIC cxx_std_20)
target_compile_definitions(ors_core PUBLIC _USE_CTP_=1)

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
