# Compiler warnings configuration - shared across all targets
# Usage: target_link_libraries(mytarget PRIVATE candid::compiler_warnings)

add_library(candid_compiler_warnings INTERFACE)
add_library(candid::compiler_warnings ALIAS candid_compiler_warnings)

target_compile_options(candid_compiler_warnings INTERFACE
  $<$<C_COMPILER_ID:MSVC>:/W4 /permissive->
  $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Wformat=2
    -Wnull-dereference
    -Wdouble-promotion
    -Werror
  >
)
