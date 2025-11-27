cmake_minimum_required(VERSION 3.21)

project(candid_renderer
  VERSION 0.1.0
  LANGUAGES C
  DESCRIPTION "Candid Engine - Renderer Library"
)

################################################################################
# Sources
################################################################################

set(${PROJECT_NAME}_SOURCES
  src/renderer.c
)

set(${PROJECT_NAME}_HEADERS
  include/candid/renderer.h
)

# Backend Metal pour macOS uniquement
if(APPLE)
  enable_language(OBJC)
  list(APPEND ${PROJECT_NAME}_SOURCES src/renderer_metal.m)
endif()

################################################################################
# Dependencies
################################################################################

find_package(SDL3 CONFIG REQUIRED)

if(APPLE)
  find_library(METAL_FRAMEWORK Metal REQUIRED)
  find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
endif()

################################################################################
# Library Target
################################################################################

add_library(${PROJECT_NAME} ${${PROJECT_NAME}_SOURCES} ${${PROJECT_NAME}_HEADERS})
add_library(candid::renderer ALIAS ${PROJECT_NAME})

# Compiler warnings (from parent cmake/) - link the shared interface target
if(TARGET candid::compiler_warnings)
  # Link the interface target so the compile options are inherited by this target.
  target_link_libraries(${PROJECT_NAME}
    PRIVATE
      candid::compiler_warnings
  )
endif()

# Public includes (consumers use: #include <candid/renderer.h>)
target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# SDL3 is needed for the public API if renderer.h exposes SDL types
target_link_libraries(${PROJECT_NAME}
  PUBLIC
    SDL3::SDL3
)

# macOS Metal frameworks
if(APPLE)
  target_link_libraries(${PROJECT_NAME}
    PRIVATE
      ${METAL_FRAMEWORK}
      ${QUARTZCORE_FRAMEWORK}
  )
endif()

################################################################################
# Installation
################################################################################

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}Targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# If a shared interface target for compiler warnings exists, also install/export it
# so the exported CMake config for this package doesn't reference a non-exported target.
if(TARGET candid_compiler_warnings)
  install(TARGETS candid_compiler_warnings
    EXPORT ${PROJECT_NAME}Targets
    COMPONENT Devel
  )
endif()

install(DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT ${PROJECT_NAME}Targets
  FILE ${PROJECT_NAME}Targets.cmake
  NAMESPACE candid::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)
