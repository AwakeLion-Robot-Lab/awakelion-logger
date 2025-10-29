include(FindPackageHandleStandardArgs)

find_path(
  IXWebSocket_INCLUDE_DIR
  NAMES ixwebsocket/IXWebSocket.h
  PATHS /usr/include /usr/local/include ${CMAKE_INSTALL_PREFIX}/include
  PATH_SUFFIXES ixwebsocket)

find_library(
  IXWebSocket_LIBRARY
  NAMES ixwebsocket
  PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
        ${CMAKE_INSTALL_PREFIX}/lib)

find_package_handle_standard_args(
  IXWebSocket REQUIRED_VARS IXWebSocket_LIBRARY IXWebSocket_INCLUDE_DIR)

if(IXWebSocket_FOUND)
  set(IXWebSocket_INCLUDE_DIRS ${IXWebSocket_INCLUDE_DIR})
  set(IXWebSocket_LIBRARIES ${IXWebSocket_LIBRARY})

  if(NOT TARGET IXWebSocket::IXWebSocket)
    add_library(IXWebSocket::IXWebSocket UNKNOWN IMPORTED)
    set_target_properties(
      IXWebSocket::IXWebSocket
      PROPERTIES IMPORTED_LOCATION "${IXWebSocket_LIBRARY}"
                 INTERFACE_INCLUDE_DIRECTORIES "${IXWebSocket_INCLUDE_DIR}")
  endif()

  mark_as_advanced(IXWebSocket_INCLUDE_DIR IXWebSocket_LIBRARY)
endif()
