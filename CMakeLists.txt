# EmbeddedTelnet - https://github.com/vaelen/embedded-telnet
# Copyright 2025, Andrew C. Young
# MIT License

cmake_minimum_required(VERSION 3.15)
project(EmbeddedTelnet VERSION 0.1.0 
  DESCRIPTION "Embedded Telnet Protocol Library"
  HOMEPAGE_URL "https://github.com/vaelen/embedded-telnet"
  LANGUAGES C
)
add_library(EmbeddedTelnet STATIC src/EmbeddedTelnet.c)
set_target_properties(EmbeddedTelnet PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  PUBLIC_HEADER include/EmbeddedTelnet.h
)
configure_file(EmbeddedTelnet.pc.in EmbeddedTelnet.pc @ONLY)
target_include_directories(EmbeddedTelnet
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
install(TARGETS EmbeddedTelnet
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
if(ESP_PLATFORM)
	idf_component_register(INCLUDE_DIRS src include)
	return()
endif()
