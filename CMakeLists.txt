# Quake on Neon Engine: a project and a native extension. See README.md.
#
# Nothing of the engine is compiled here. The engine is found by one path,
# and what is used of it is cmake/NeonSdk.cmake, the two headers of
# extensions, and a NeonRuntime that is built already.
cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(neon-quake C CXX)

set(NEON_ENGINE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../neon-engine" CACHE PATH
        "The checkout of Neon Engine this is built against")

if (NOT EXISTS "${NEON_ENGINE_DIRECTORY}/cmake/NeonSdk.cmake")
  message(FATAL_ERROR
          "Neon Engine is not at ${NEON_ENGINE_DIRECTORY}. Check it out next to this repository, or say where it "
          "is with -DNEON_ENGINE_DIRECTORY=<folder>")
endif ()

include("${NEON_ENGINE_DIRECTORY}/cmake/NeonSdk.cmake")

neon_add_project(quake
        SOURCES
        extensions/quake/quake.cpp
)
