cmake_minimum_required(VERSION 3.20)
project(
  aw_logger
  VERSION 1.0.0
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -w")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -g")
endif()

set(CONFIG_FILE_PATH
    "${CMAKE_SOURCE_DIR}/config/aw_logger_settings.json"
    CACHE PATH "Path to Awakelion Logger configuration file")

if(NOT EXISTS ${CONFIG_FILE_PATH})
  message(FATAL_ERROR "Configuration file not found: ${CONFIG_FILE_PATH}")
endif()

configure_file(
  "${CMAKE_SOURCE_DIR}/cmake/config_path.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/include/aw_logger/config_path.h" @ONLY)

configure_file("${CMAKE_SOURCE_DIR}/cmake/aw_logger_version.h.in"
               "${CMAKE_CURRENT_BINARY_DIR}/aw_logger_version.hpp" @ONLY)

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE include/3rdparty/nlohmann)

add_library(aw_logger INTERFACE)
target_include_directories(
  aw_logger
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/3rdparty>
            $<INSTALL_INTERFACE:include>)
target_link_libraries(aw_logger INTERFACE nlohmann_json)

option(BUILD_TESTING "Build tests" OFF)

# build test via googletest
if(BUILD_TESTING)
  enable_testing()

  # try to find GTest on the system first
  find_package(GTest QUIET)

  # fetch from github if not
  if(NOT GTest_FOUND)
    message(STATUS "GTest not found on system, downloading via FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG v1.17.0)
    set(gtest_force_shared_crt
        ON
        CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  else()
    message(STATUS "Using system GTest (version ${GTest_VERSION})")
  endif()

  file(GLOB TEST_SOURCES "test/*.cpp")

  foreach(test_source ${TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE aw_logger GTest::gtest_main)
    add_test(NAME ${test_name} COMMAND ${test_name})
  endforeach()
endif()

install(
  TARGETS aw_logger nlohmann_json
  EXPORT aw_logger-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(
  DIRECTORY include/aw_logger
  DESTINATION include
  FILES_MATCHING
  PATTERN "*.hpp")

install(FILES config/aw_logger_settings.json
        DESTINATION ${CMAKE_INSTALL_DATADIR}/aw_logger)
