cmake_minimum_required(VERSION 3.20)
project(
  aw_logger
  VERSION 0.1.0
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 编译器特定配置
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

set(AW_LOGGER_HEADERS
    include/aw_logger/log_macro.hpp
    include/aw_logger/fmt_base.hpp
    include/aw_logger/log_event.hpp
    include/aw_logger/formatter.hpp
    include/aw_logger/logger.hpp
    include/aw_logger/exception.hpp
    include/aw_logger/ring_buffer.hpp
    include/aw_logger/appender.hpp
    include/aw_logger/impl/log_event_impl.hpp
    include/aw_logger/impl/formatter_impl.hpp
    include/aw_logger/impl/logger_impl.hpp
    include/aw_logger/impl/ring_buffer_impl.hpp
    include/aw_logger/impl/console_appender_impl.hpp
    include/aw_logger/aw_logger.hpp
    # include/aw_logger/impl/file_appender_impl.hpp
    # include/aw_logger/impl/websocket_appender_impl.hpp
)

set(AW_LOGGER_EXAMPLES example/hello_aw_logger.cpp)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
include_directories(include include/3rdparty)

add_library(nlohmann_json INTERFACE)
add_library(aw_logger STATIC ${AW_LOGGER_HEADERS} ${AW_LOGGER_EXAMPLES})

target_include_directories(nlohmann_json INTERFACE include/3rdparty/nlohmann)
target_include_directories(aw_logger PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include
                                            include)

target_link_libraries(aw_logger PUBLIC nlohmann_json)

foreach(example ${AW_LOGGER_EXAMPLES})
  get_filename_component(example_name ${example} NAME_WE)
  add_executable(${example_name} ${example})
  target_link_libraries(${example_name} aw_logger)
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
