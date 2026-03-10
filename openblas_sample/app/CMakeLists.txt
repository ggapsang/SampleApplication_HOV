cmake_minimum_required(VERSION 3.2.2)

project(openblas_sample)

set(TARGET_EXE ${PROJECT_NAME})

set(SOC $ENV{SOC})
message(STATUS "SOC : ${SOC}")

include(toolchain.cmake)

message(STATUS "Compiler")
message(STATUS " - ID       \t: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS " - Version  \t: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS " - Path     \t: ${CMAKE_CXX_COMPILER}")

set(COMMON ${SDK_PATH}/common)
set(WISENET_PLATFORM_RELEASE_PATH ${COMMON}/platform/${SOC})
set(WISENET_PRODUCT_RELEASE_PATH ${COMMON}/product/${SOC})
set(APP_SUPPORT_PACKAGE ${COMMON}/AppSupportPackage/${SOC})

message("COMMON: ${COMMON}")
message("WISENET_PLATFORM_RELEASE_PATH: ${WISENET_PLATFORM_RELEASE_PATH}")
message("WISENET_PRODUCT_RELEASE_PATH: ${WISENET_PRODUCT_RELEASE_PATH}")
message("APP_SUPPORT_PACKAGE: ${APP_SUPPORT_PACKAGE}")

include_directories(${WISENET_PRODUCT_RELEASE_PATH}/includes)
include_directories(${WISENET_PRODUCT_RELEASE_PATH}/lib)
include_directories(${WISENET_PLATFORM_RELEASE_PATH}/includes)
include_directories(includes)

message("includes: ${CMAKE_CURRENT_SOURCE_DIR}/includes")

link_directories(${COMMON}/lib)
link_directories(${WISENET_PRODUCT_RELEASE_PATH}/lib)
link_directories(${WISENET_PLATFORM_RELEASE_PATH}/lib)
link_directories(${WISENET_PRODUCT_RELEASE_PATH}/3rd_party/lib)
link_directories(libs)

# OpenBLAS cross compile
set(OPENBLAS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/OpenBLAS)
set(OPENBLAS_INSTALL_DIR ${CMAKE_BINARY_DIR}/openblas_install)
file(MAKE_DIRECTORY ${OPENBLAS_INSTALL_DIR})

set(OPENBLAS_BUILD_OPTS
  "CC=${CMAKE_C_COMPILER}"
  "HOSTCC=gcc"
  "NO_FORTRAN=1"
  "NO_SHARED=1"
  "BUILD_WITHOUT_LAPACK=1"
  "DYNAMIC_ARCH=1"
  "TARGET=ARMV8"
  "PREFIX=${OPENBLAS_INSTALL_DIR}"
  "-j4"
)

if($ENV{CLEAN_OPENBLAS} STREQUAL "ON")
  message(STATUS "rebuild openblas")
  execute_process(
    COMMAND make clean
    WORKING_DIRECTORY ${OPENBLAS_SOURCE_DIR}
    RESULT_VARIABLE CLENA_RESULT
  )
else()
  message(STATUS "skip rebuild openblas")
endif()

message(STATUS "build start: OpenBLAS for AARCH64")
string(REPLACE ";" " " MAKE_CMD_STR "${OPENBLAS_BUILD_OPTS}")
message(STATUS "execute cmd : make ${MAKE_CMD_STR}")

execute_process(
  COMMAND make ${OPENBLAS_BUILD_OPTS}
  WORKING_DIRECTORY ${OPENBLAS_SOURCE_DIR}
  RESULT_VARIABLE BUILD_RESULT
  OUTPUT_VARIABLE BUILD_OUTPUT
  ERROR_VARIABLE BUILD_ERROR
  COMMAND_ECHO STDOUT
)

if(NOT BUILD_RESULT EQUAL 0)
  message(FATAL_ERROR "OpenBLAS build failed: ${BUILD_RESULT}\n${BUILD_ERROR}")
else()
  message(STATUS "OpenBLAS build done")
endif()

execute_process(
  COMMAND make ${OPENBLAS_BUILD_OPTS} install
  WORKING_DIRECTORY ${OPENBLAS_SOURCE_DIR}
  RESULT_VARIABLE INSTALL_RESULT
  OUTPUT_VARIABLE INSTALL_OUTPUT
  ERROR_VARIABLE INSTALL_ERROR
)

if(NOT INSTALL_RESULT EQUAL 0)
  message(FATAL_ERROR "OpenBLAS install failed: ${INSTALL_RESULT}\n${INSTALL_ERROR}")
else()
  message(STATUS "OpenBLAS install done")
endif()

include_directories(${OPENBLAS_INSTALL_DIR}/include)
link_directories(${OPENBLAS_INSTALL_DIR}/lib)
set(OPENBLAS_LIB openblas)

message(STATUS "OpenBLAS install path : ${OPENBLAS_INSTALL_DIR}/include")
message(STATUS "OpenBLAS lib : ${OPENBLAS_LIB}")

add_subdirectory(src)

