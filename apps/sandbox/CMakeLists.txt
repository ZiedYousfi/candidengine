cmake_minimum_required(VERSION 3.21)

project(candid_sandbox
  VERSION 0.1.0
  LANGUAGES C
  DESCRIPTION "Candid Engine - Sandbox Application"
)

################################################################################
# Dependencies
################################################################################

# SDL3 and extensions
find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)

################################################################################
# Executable Target
################################################################################

add_executable(${PROJECT_NAME}
  src/main.c
)

set_target_properties(${PROJECT_NAME} PROPERTIES
  OUTPUT_NAME Candid
  C_STANDARD 23
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Compiler warnings (from parent cmake/)
if(TARGET candid::compiler_warnings)
  target_link_libraries(${PROJECT_NAME} PRIVATE candid::compiler_warnings)
endif()

# Link to internal libraries
target_link_libraries(${PROJECT_NAME}
  PRIVATE
    candid::renderer
    SDL3::SDL3
    $<IF:$<TARGET_EXISTS:SDL3_image::SDL3_image-shared>,SDL3_image::SDL3_image-shared,SDL3_image::SDL3_image-static>
    SDL3_ttf::SDL3_ttf
)
