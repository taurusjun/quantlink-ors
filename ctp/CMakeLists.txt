find_package(CTP REQUIRED)

# shortcut: INTERFACE until CTP sources exist (Task 2+); switch to STATIC then.
add_library(ors_ctp_core INTERFACE)
target_link_libraries(ors_ctp_core INTERFACE CTP::trader CTP::md)

add_executable(ors_ctp src/main.cpp)
target_link_libraries(ors_ctp PRIVATE ors_ctp_core CTP::trader CTP::md)

add_executable(ctp_config_test tests/CtpConfigTest.cpp)
target_compile_definitions(ctp_config_test PRIVATE
  ORS_CTP_CMAKE_COMMAND="${CMAKE_COMMAND}"
  ORS_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  ORS_TEST_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}/ctp_config_test_build")
add_test(NAME ctp_config COMMAND ctp_config_test)
