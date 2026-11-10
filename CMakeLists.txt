cmake_minimum_required(VERSION 3.20)
project(vibe_shooter C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

# Simulation: fixed point only, no platform, shared by the game and the tests.
add_library(sim STATIC src/fixed.c src/game.c src/demo.c)
target_include_directories(sim PUBLIC src)

find_package(SDL3 REQUIRED CONFIG)
add_executable(shooter src/main.c src/render.c src/gfx.c src/font.c src/sound.c src/platform_sdl3.c)
target_link_libraries(shooter PRIVATE sim SDL3::SDL3 m)
add_custom_command(TARGET shooter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:shooter>/assets)

enable_testing()
add_executable(demo_test tests/demo_test.c)
target_link_libraries(demo_test PRIVATE sim)
add_test(NAME determinism COMMAND demo_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Dev tool: headless screenshots and art contact sheets.
add_executable(snapshot tools/snapshot.c src/render.c src/gfx.c src/font.c)
target_link_libraries(snapshot PRIVATE sim m)
