cmake_minimum_required(VERSION 3.22)
project(finton LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

# Everything except main() lives in this library so tests can link it later.
# Add each new source file to this list.
add_library(finton_core
    src/lib/queue.c++
    src/lib/scheduler.c++
    src/lib/workers.c++
)

target_include_directories(finton_core PUBLIC src/header)
target_link_libraries(finton_core PUBLIC Threads::Threads)
target_compile_options(finton_core PRIVATE -Wall -Wextra -fsanitize=thread -g)

add_executable(finton src/main.cpp)
target_link_libraries(finton PRIVATE finton_core)
