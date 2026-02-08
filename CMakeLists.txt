cmake_minimum_required(VERSION 3.16)
project(cli_shell LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CLI_BUILD_TESTS "Build unit tests" ON)

# --- Library with all shell implementation ---
add_library(shell_lib
  src/ShellApp.cpp
  src/Tokenizer.cpp
  src/Parse.cpp
  src/Environment.cpp
  src/Executor.cpp
  src/Builtins.cpp
  src/Utils.cpp
)

# Platform-specific external runner
if (WIN32)
  target_sources(shell_lib PRIVATE src/ExternalRunner_win.cpp)
else()
  target_sources(shell_lib PRIVATE src/ExternalRunner_posix.cpp)
endif()

target_include_directories(shell_lib PUBLIC
  ${CMAKE_SOURCE_DIR}/include
)

# --- App (REPL) ---
add_executable(shell src/main.cpp)
target_link_libraries(shell PRIVATE shell_lib)

# --- Tests ---
if (CLI_BUILD_TESTS)
  enable_testing()

  # Small helper program used by ExternalRunner tests
  add_executable(fixture_env tests/fixture_env.cpp)

  add_executable(shell_tests tests/shell_tests.cpp)
  target_link_libraries(shell_tests PRIVATE shell_lib)

  # Pass the full path to the helper exe into the test binary
  target_compile_definitions(shell_tests PRIVATE FIXTURE_PATH="$<TARGET_FILE:fixture_env>")

  add_test(NAME shell_tests COMMAND shell_tests)
endif()
