cmake_minimum_required(VERSION 3.16)
project(zlab_mqtt_message_guard LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(message_guard src/MessageGuard.cpp)
target_include_directories(message_guard PUBLIC include)
target_compile_options(message_guard PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(message_guard_tests tests/test_message_guard.cpp)
target_link_libraries(message_guard_tests PRIVATE message_guard)
target_compile_options(message_guard_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(gateway_sim examples/gateway_sim.cpp)
target_link_libraries(gateway_sim PRIVATE message_guard)
target_compile_options(gateway_sim PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_test(NAME message_guard_tests COMMAND message_guard_tests)
