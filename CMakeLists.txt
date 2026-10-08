cmake_minimum_required(VERSION 3.22)
project(finton LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

# JSON library (header-only). Downloaded once at configure time into the build
# directory, so nothing has to be installed on the machine.
include(FetchContent)
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# Everything except main() lives in this library so tests can link it later.
# Add each new source file to this list.
add_library(finton_core
    src/lib/queue.c++
    src/lib/scheduler.c++
    src/lib/workers.c++
    src/lib/server.c++
    src/lib/http_parser.c++
)

target_include_directories(finton_core PUBLIC src/header)
target_link_libraries(finton_core PUBLIC Threads::Threads nlohmann_json::nlohmann_json)
target_compile_options(finton_core PUBLIC -Wall -Wextra -g)

# ThreadSanitizer finds data races but makes the program 5-15x slower, so
# timings from a sanitized build mean nothing. Turn it off to measure :
#   cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DFINTON_TSAN=OFF
option(FINTON_TSAN "Instrument with ThreadSanitizer" ON)
if(FINTON_TSAN)
    target_compile_options(finton_core PUBLIC -fsanitize=thread)
    target_link_options(finton_core PUBLIC -fsanitize=thread)
endif()

add_executable(finton src/main.cpp)
target_link_libraries(finton PRIVATE finton_core)

# Tests : build, then run with  ctest --test-dir build --output-on-failure
enable_testing()
add_executable(http_parser_test tests/http_parser_test.cpp)
target_link_libraries(http_parser_test PRIVATE finton_core)
add_test(NAME http_parser COMMAND http_parser_test)
