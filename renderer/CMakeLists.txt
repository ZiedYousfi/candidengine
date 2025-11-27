cmake_minimum_required(VERSION 3.21)

project(candid_renderer
  VERSION 0.1.0
  LANGUAGES C
  DESCRIPTION "Candid Engine - Renderer Library"
)

################################################################################
# Options
################################################################################

option(CANDID_VULKAN_SUPPORT "Enable Vulkan backend support" OFF)
option(CANDID_SHADER_COMPILATION "Enable runtime shader compilation" ON)

################################################################################
# Sources
################################################################################

set(${PROJECT_NAME}_SOURCES
  src/renderer.c
  src/mesh.c
  src/backend.c
)

set(${PROJECT_NAME}_HEADERS
  include/candid/types.h
  include/candid/mesh.h
  include/candid/shader.h
  include/candid/material.h
  include/candid/backend.h
  include/candid/renderer.h
)

# Backend Metal pour macOS uniquement
if(APPLE)
  enable_language(OBJC)
  list(APPEND ${PROJECT_NAME}_SOURCES
    src/backend_metal.m
    src/renderer_metal.m  # Legacy compatibility
  )
endif()

# Backend Vulkan (cross-platform)
if(CANDID_VULKAN_SUPPORT)
  list(APPEND ${PROJECT_NAME}_SOURCES src/backend_vulkan.c)
endif()

################################################################################
# Dependencies
################################################################################

find_package(SDL3 CONFIG REQUIRED)

if(APPLE)
  find_library(METAL_FRAMEWORK Metal REQUIRED)
  find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
endif()

if(CANDID_VULKAN_SUPPORT)
  find_package(volk CONFIG REQUIRED)
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

# Compile definitions
target_compile_definitions(${PROJECT_NAME}
  PRIVATE
    $<$<BOOL:${CANDID_VULKAN_SUPPORT}>:CANDID_VULKAN_SUPPORT>
)

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

# Vulkan support via volk
if(CANDID_VULKAN_SUPPORT)
  target_link_libraries(${PROJECT_NAME}
    PRIVATE
      volk::volk
  )
endif()

################################################################################
# Shader Compilation Support
################################################################################

# Create a custom target for shader compilation tools
if(CANDID_SHADER_COMPILATION)
  # We can add shader compilation targets here for HLSL -> SPIR-V / Metal
  # This requires external tools like:
  # - dxc (DirectX Shader Compiler) for HLSL to SPIR-V
  # - spirv-cross for SPIR-V to MSL
  # - metal for Metal shader compilation

  # Example: Find shader compiler tools
  find_program(DXC_EXECUTABLE dxc HINTS "$ENV{VULKAN_SDK}/bin")
  find_program(SPIRV_CROSS_EXECUTABLE spirv-cross HINTS "$ENV{VULKAN_SDK}/bin")

  if(DXC_EXECUTABLE AND SPIRV_CROSS_EXECUTABLE)
    message(STATUS "Shader compilation tools found:")
    message(STATUS "  DXC: ${DXC_EXECUTABLE}")
    message(STATUS "  SPIRV-Cross: ${SPIRV_CROSS_EXECUTABLE}")
  else()
    message(STATUS "Shader compilation tools not found - using pre-compiled shaders only")
  endif()
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
