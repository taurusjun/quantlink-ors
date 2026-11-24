find_package(CTP REQUIRED)

# CtpConfig has no SDK dependency, so its unit test runs without the CTP .so.
add_library(ors_ctp_config STATIC src/CtpConfig.cpp)
target_include_directories(ors_ctp_config PUBLIC include)

add_library(ors_ctp_core STATIC src/CtpTraderSession.cpp)
target_include_directories(ors_ctp_core PUBLIC include)
target_link_libraries(ors_ctp_core PUBLIC ors_ctp_config PRIVATE CTP::trader)

add_executable(ors_ctp src/main.cpp)
target_link_libraries(ors_ctp PRIVATE ors_ctp_core CTP::trader CTP::md)

add_executable(ctp_config_test tests/CtpConfigTest.cpp)
target_link_libraries(ctp_config_test PRIVATE ors_ctp_config)
add_test(NAME ctp_config COMMAND ctp_config_test)

# Executes the binary, so a missing/incompatible CTP .so fails here and not
# only at link time.
add_test(NAME ctp_link_smoke COMMAND ors_ctp)
