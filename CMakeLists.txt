# Quake on Neon Engine: a project and a native extension. See README.md.
#
# Nothing of the engine is compiled here. The engine is found by one path,
# and what is used of it is cmake/NeonSdk.cmake, the two headers of
# extensions, and a NeonRuntime that is built already.
cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(neon-quake C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(NEON_ENGINE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../neon-engine" CACHE PATH
        "The checkout of Neon Engine this is built against")

if (NOT EXISTS "${NEON_ENGINE_DIRECTORY}/cmake/NeonSdk.cmake")
  message(FATAL_ERROR
          "Neon Engine is not at ${NEON_ENGINE_DIRECTORY}. Check it out next to this repository, or say where it "
          "is with -DNEON_ENGINE_DIRECTORY=<folder>")
endif ()

include("${NEON_ENGINE_DIRECTORY}/cmake/NeonSdk.cmake")

# The formats of the game's data, read from bytes in memory: archives,
# pictures, levels, models, the game code. They know nothing of the engine,
# which is what lets them be tested without it. The extension links them.
#
# The files are found, not listed, so that a format is added by adding its
# files. A test is named after the file it tests, with `.test` in front of
# the extension, and sits next to it, as in the engine.
file(GLOB QUAKE_FORMAT_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/extensions/quake/formats/*.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/extensions/quake/formats/*.hpp"
)
set(QUAKE_FORMAT_TESTS ${QUAKE_FORMAT_FILES})
list(FILTER QUAKE_FORMAT_FILES EXCLUDE REGEX "\\.test\\.cpp$")
list(FILTER QUAKE_FORMAT_TESTS INCLUDE REGEX "\\.test\\.cpp$")

add_library(quake-formats STATIC ${QUAKE_FORMAT_FILES})
target_include_directories(quake-formats PUBLIC "${CMAKE_CURRENT_LIST_DIR}/extensions/quake")
target_compile_options(quake-formats PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
# it is linked into a library that is opened while the runtime runs
set_target_properties(quake-formats PROPERTIES POSITION_INDEPENDENT_CODE ON)

# What the game does with its data, without the engine: the functions of the
# original's engine that the game code calls, the entities of a level handed
# to the game code, how time passes for them. Like the formats it knows
# nothing of Neon Engine and is tested without it. Files and tests are found
# as for the formats.
file(GLOB QUAKE_GAME_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/extensions/quake/game/*.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/extensions/quake/game/*.hpp"
)
set(QUAKE_GAME_TESTS ${QUAKE_GAME_FILES})
list(FILTER QUAKE_GAME_FILES EXCLUDE REGEX "\\.test\\.cpp$")
list(FILTER QUAKE_GAME_TESTS INCLUDE REGEX "\\.test\\.cpp$")

add_library(quake-game STATIC ${QUAKE_GAME_FILES})
target_link_libraries(quake-game PUBLIC quake-formats)
target_compile_options(quake-game PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
set_target_properties(quake-game PROPERTIES POSITION_INDEPENDENT_CODE ON)

neon_add_project(quake
        SOURCES
        extensions/quake/quake.cpp
        extensions/quake/game-code.cpp
        extensions/quake/game-code.hpp
        extensions/quake/game-data.cpp
        extensions/quake/game-data.hpp
        extensions/quake/level-view.cpp
        extensions/quake/level-view.hpp
        extensions/quake/model-view.cpp
        extensions/quake/model-view.hpp
        extensions/quake/particle-view.cpp
        extensions/quake/particle-view.hpp
        extensions/quake/sound-view.cpp
        extensions/quake/sound-view.hpp
        extensions/quake/sprite-view.cpp
        extensions/quake/sprite-view.hpp
)
target_link_libraries(quake-extension PRIVATE quake-game)


set(QUAKE_TEST_DATA_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/extensions/quake/assets/id1" CACHE PATH
        "A folder of game data to test the formats with: the data this repository carries, or lq1 of a checkout of LibreQuake")

# Tests of the formats and of the game. They need GoogleTest, which is taken from the engine's
# checkout as sources; nothing of the engine itself is built. `ctest` runs
# them, and the target quake-tests builds them. They are not part of the
# default build.
option(QUAKE_BUILD_TESTS "Make the tests of the formats available to build" ON)
if (QUAKE_BUILD_TESTS AND EXISTS "${NEON_ENGINE_DIRECTORY}/external/googletest/CMakeLists.txt")
  enable_testing()
  include(GoogleTest)

  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
  add_subdirectory("${NEON_ENGINE_DIRECTORY}/external/googletest" "${CMAKE_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)

  add_custom_target(quake-tests)

  foreach (TEST_SOURCE IN LISTS QUAKE_FORMAT_TESTS QUAKE_GAME_TESTS)
    get_filename_component(TEST_NAME "${TEST_SOURCE}" NAME)
    string(REGEX REPLACE "\\.test\\.cpp$" "" TEST_NAME "${TEST_NAME}")
    # a test is named after the library its file is of: the folder it is in
    get_filename_component(TEST_FOLDER "${TEST_SOURCE}" DIRECTORY)
    get_filename_component(TEST_FOLDER "${TEST_FOLDER}" NAME)
    set(TEST_TARGET "quake-${TEST_FOLDER}.${TEST_NAME}.test")

    add_executable(${TEST_TARGET} EXCLUDE_FROM_ALL "${TEST_SOURCE}")
    target_compile_options(${TEST_TARGET} PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
    target_link_libraries(${TEST_TARGET} PRIVATE GTest::gmock_main quake-game)
    # Where data to test with is found: the data this repository carries,
    # unless the build says otherwise. A test that wants a file that is not
    # there skips itself.
    target_compile_definitions(${TEST_TARGET} PRIVATE
            QUAKE_TEST_DATA_DIRECTORY="${QUAKE_TEST_DATA_DIRECTORY}")
    add_dependencies(quake-tests ${TEST_TARGET})

    gtest_discover_tests(${TEST_TARGET} DISCOVERY_MODE PRE_TEST DISCOVERY_TIMEOUT 60)
  endforeach ()
endif ()
