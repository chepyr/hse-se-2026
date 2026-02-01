cmake_minimum_required(VERSION 3.16)
project(cli_shell LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# Временная заглушка
add_executable(shell src/main.cpp)

add_test(NAME dummy_test COMMAND shell)
