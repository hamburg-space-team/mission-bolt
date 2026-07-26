cmake_minimum_required(VERSION 3.22)
project(bolt-tests CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Host tests run instrumented by default: UB that behaves fine on x86 and
# differently on the Cortex-M4 (signed overflow, misaligned access) fails
# the test run here instead of flying. Costs ~2x runtime on ms-scale suites
option(BOLT_SANITIZE "ASan+UBSan on the host tests" ON)
# gcov instrumentation; used by the verify gate's coverage stage in a
# separate build tree (mixing with sanitizers skews both)
option(BOLT_COVERAGE "coverage instrumentation for the host tests" OFF)

if(BOLT_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()
if(BOLT_COVERAGE)
    add_compile_options(--coverage -O0)
    add_link_options(--coverage)
endif()

enable_testing()
add_subdirectory(tests)

# --- Doxygen (optional) ----------------------------------------------------
# Adds a `docs` target when Doxygen is installed. Does not affect the host
# test build. Run with:
#     cmake --build build/tests --target docs
# Output is written to flight-software/build/doxygen/html.
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    set(DOXYGEN_INPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    set(DOXYFILE ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE}
        WORKING_DIRECTORY ${DOXYGEN_INPUT_DIR}
        COMMENT "Generating Doxygen documentation"
        VERBATIM
    )
endif()
