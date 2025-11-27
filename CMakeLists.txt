cmake_minimum_required(VERSION 3.21)

project(CandidEngine
  VERSION 0.1.0
  LANGUAGES C
  DESCRIPTION "Candid Game Engine"
)

################################################################################
# Global Settings
################################################################################

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

################################################################################
# Shared CMake Modules
################################################################################

include(cmake/CompilerWarnings.cmake)

################################################################################
# Libraries (order matters for dependencies)
################################################################################

add_subdirectory(libs/renderer)
# add_subdirectory(libs/audio)    # Future
# add_subdirectory(libs/physics)  # Future
# add_subdirectory(libs/core)     # Future

################################################################################
# Applications
################################################################################

add_subdirectory(apps/sandbox)

