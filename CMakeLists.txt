cmake_minimum_required(VERSION 3.16)
project(minicc VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# The compiler itself is a library so the CLI and the tests share one build.
add_library(minicc_lib
  src/ast.cpp
  src/codegen.cpp
  src/driver.cpp
  src/ir.cpp
  src/lexer.cpp
  src/optimizer.cpp
  src/parser.cpp
  src/sema.cpp)
target_include_directories(minicc_lib PUBLIC src)

if(MSVC)
  target_compile_options(minicc_lib PUBLIC /W4 /permissive-)
else()
  target_compile_options(minicc_lib PUBLIC -Wall -Wextra -Wpedantic)
endif()

option(MINICC_SANITIZE "Build with AddressSanitizer + UBSan (GCC/Clang)" OFF)
if(MINICC_SANITIZE AND NOT MSVC)
  target_compile_options(minicc_lib PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer -g)
  target_link_options(minicc_lib PUBLIC -fsanitize=address,undefined)
endif()

add_executable(minicc src/main.cpp)
target_link_libraries(minicc PRIVATE minicc_lib)

enable_testing()
add_executable(minicc_tests tests/test_main.cpp)
target_link_libraries(minicc_tests PRIVATE minicc_lib)
target_compile_definitions(minicc_tests PRIVATE MINICC_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/examples")
add_test(NAME minicc_tests COMMAND minicc_tests)
