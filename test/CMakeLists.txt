cmake_minimum_required(VERSION 3.16)
project(smoke_x_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Warning flags useful for catching parser bugs early.
add_compile_options(-Wall -Wextra -Werror -Wno-unused-parameter)

enable_testing()

# Test Smoke X parsing (no ESP-IDF dependencies)
add_executable(test_smoke_x_parser
    test_smoke_x_parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/smoke_x_parser.c
)
target_include_directories(test_smoke_x_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

target_link_libraries(test_smoke_x_parser PRIVATE m)

add_test(NAME smoke_x_parser COMMAND test_smoke_x_parser)

# Test Improv-Serial framing/parsing (no ESP-IDF dependencies)
add_executable(test_improv
    test_improv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/improv.c
)
target_include_directories(test_improv PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

add_test(NAME improv COMMAND test_improv)
