cmake_minimum_required(VERSION 3.21)

project(Candid VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# # Library sources
# set(${PROJECT_NAME}_SOURCES
# )

# Library headers
set(${PROJECT_NAME}_HEADERS
  include/hw.h
)

# Create the main library target once below, add headers/sources there.
# add_library(${PROJECT_NAME} ${${PROJECT_NAME}_HEADERS})

# add_library(${PROJECT_NAME} ${${PROJECT_NAME}_SOURCES} ${${PROJECT_NAME}_HEADERS})

################################################################################
# Main Library Target
################################################################################

add_library(${PROJECT_NAME} INTERFACE)

# Set standard properties
## NOTE: This is an INTERFACE (header-only) library. It doesn't contain
## source files and only provides headers/compile options for consumers.

# Compiler options (cross-platform)
target_compile_options(${PROJECT_NAME}
  INTERFACE
    $<$<C_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wformat=2 -Wnull-dereference -Wdouble-promotion -Werror>
)

# Include directories
target_include_directories(${PROJECT_NAME}
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link dependencies
# target_link_libraries(${PROJECT_NAME}
#   PRIVATE
#     ${LUA_LIBRARIES}
#     CLI11::CLI11
#     sol2::sol2
# )

add_executable(${PROJECT_NAME}_exec
  src/main.c
)

set_target_properties(${PROJECT_NAME}_exec PROPERTIES
  OUTPUT_NAME ${PROJECT_NAME}
  C_STANDARD 23
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF
)

target_link_libraries(${PROJECT_NAME}_exec
  PRIVATE
    ${PROJECT_NAME}
)

# Put runtime binaries into the binary directory under a `bin` folder
set_target_properties(${PROJECT_NAME}_exec PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

