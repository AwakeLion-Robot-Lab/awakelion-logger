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
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/aw_logger")
endif()

configure_file(
  "${CMAKE_SOURCE_DIR}/cmake/config_path.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/include/aw_logger/config_path.h" @ONLY)

set(AW_LOGGER_HEADERS include)
set(AW_LOGGER_TEST example/)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
include_directories(include include/3rdparty)

add_library(nlohmann_json INTERFACE)
add_library(aw_logger STATIC ${AW_LOGGER_HEADERS} ${AW_LOGGER_TEST})

target_include_directories(nlohmann_json INTERFACE include/3rdparty/nlohmann)
target_include_directories(aw_logger PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include
                                            include)

target_link_libraries(aw_logger PUBLIC nlohmann_json)

foreach(test ${AW_LOGGER_TEST})
  get_filename_component(test_name ${test} NAME_WE)
  add_executable(${test_name} ${test})
  target_link_libraries(${test_name} aw_logger)
endforeach()

install(
  TARGETS aw_logger
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
