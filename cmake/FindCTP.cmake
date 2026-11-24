# Locate the CTP TraderApi / MdApi SDK.
#
# The SDK is licensed and never committed to this repository. CTP_HOME must point
# at the SDK root that contains include/ and lib/; it is normally supplied by the
# environment, or explicitly with -DCTP_HOME=<path>.
#
# Result variables: CTP_FOUND, CTP_INCLUDE_DIR, CTP_TRADER_LIBRARY, CTP_MD_LIBRARY
# Imported targets: CTP::trader, CTP::md

if(NOT CTP_HOME AND DEFINED ENV{CTP_HOME})
  set(CTP_HOME "$ENV{CTP_HOME}")
endif()
set(CTP_HOME "${CTP_HOME}" CACHE PATH "CTP SDK root directory (contains include/ and lib/)")

set(CTP_INCLUDE_DIR CTP_INCLUDE_DIR-NOTFOUND)
set(CTP_TRADER_LIBRARY CTP_TRADER_LIBRARY-NOTFOUND)
set(CTP_MD_LIBRARY CTP_MD_LIBRARY-NOTFOUND)
set(_ctp_missing "")

if(NOT CTP_HOME)
  set(CTP_NOT_FOUND_MESSAGE
    "CTP_HOME is not set. Point CTP_HOME at the CTP SDK root (it must contain include/ and lib/), "
    "either with -DCTP_HOME=<path> or by exporting CTP_HOME. The SDK is not part of this repository.")
else()
  set(_ctp_include_dir "${CTP_HOME}/include")
  set(_ctp_headers_ok TRUE)
  foreach(_ctp_header ThostFtdcTraderApi.h ThostFtdcMdApi.h ThostFtdcUserApiStruct.h)
    if(NOT EXISTS "${_ctp_include_dir}/${_ctp_header}")
      set(_ctp_headers_ok FALSE)
      string(APPEND _ctp_missing "\n  - header ${_ctp_include_dir}/${_ctp_header}")
    endif()
  endforeach()
  if(_ctp_headers_ok)
    set(CTP_INCLUDE_DIR "${_ctp_include_dir}")
  endif()

  find_library(CTP_TRADER_LIBRARY NAMES thosttraderapi_se
    PATHS "${CTP_HOME}/lib" NO_DEFAULT_PATH)
  find_library(CTP_MD_LIBRARY NAMES thostmduserapi_se
    PATHS "${CTP_HOME}/lib" NO_DEFAULT_PATH)
  if(NOT CTP_TRADER_LIBRARY)
    string(APPEND _ctp_missing "\n  - library ${CTP_HOME}/lib/libthosttraderapi_se.so")
  endif()
  if(NOT CTP_MD_LIBRARY)
    string(APPEND _ctp_missing "\n  - library ${CTP_HOME}/lib/libthostmduserapi_se.so")
  endif()

  if(_ctp_missing)
    set(CTP_NOT_FOUND_MESSAGE
      "CTP SDK is incomplete under CTP_HOME='${CTP_HOME}':${_ctp_missing}")
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CTP
  REQUIRED_VARS CTP_INCLUDE_DIR CTP_TRADER_LIBRARY CTP_MD_LIBRARY
  REASON_FAILURE_MESSAGE "${CTP_NOT_FOUND_MESSAGE}")

if(CTP_FOUND)
  if(NOT TARGET CTP::trader)
    add_library(CTP::trader SHARED IMPORTED)
    set_target_properties(CTP::trader PROPERTIES
      IMPORTED_LOCATION "${CTP_TRADER_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${CTP_INCLUDE_DIR}")
  endif()
  if(NOT TARGET CTP::md)
    add_library(CTP::md SHARED IMPORTED)
    set_target_properties(CTP::md PROPERTIES
      IMPORTED_LOCATION "${CTP_MD_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${CTP_INCLUDE_DIR}")
  endif()
endif()

mark_as_advanced(CTP_INCLUDE_DIR CTP_TRADER_LIBRARY CTP_MD_LIBRARY)
