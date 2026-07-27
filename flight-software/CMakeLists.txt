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

include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)
include(Catch)

# Unit tests live next to the code they test: shared/utils/crc16.test.cpp
# checks shared/utils/crc16.hpp. One executable per file, re-globbed on
# build, so adding a test needs no CMake edit. The flight builds cannot
# pick these up - the cprojects list their sources explicitly
file(GLOB_RECURSE BOLT_UNIT_TESTS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/shared/*.test.cpp"
    "${CMAKE_SOURCE_DIR}/btc/app/*.test.cpp"
    "${CMAKE_SOURCE_DIR}/exp1-space-disco/app/*.test.cpp"
    "${CMAKE_SOURCE_DIR}/exp2-bouncy-castle/app/*.test.cpp"
    "${CMAKE_SOURCE_DIR}/exp3-floaty-boi/app/*.test.cpp")

foreach(src IN LISTS BOLT_UNIT_TESTS)
    # shared/utils/crc16.test.cpp -> target shared-utils-crc16
    file(RELATIVE_PATH rel "${CMAKE_SOURCE_DIR}" "${src}")
    string(REGEX REPLACE "\\.test\\.cpp$" "" rel "${rel}")
    string(REPLACE "/" "-" tgt "${rel}")
    add_executable(${tgt} "${src}")
    target_include_directories(${tgt} PRIVATE
        ${CMAKE_SOURCE_DIR}/shared
        ${CMAKE_SOURCE_DIR}/shared/comms
        ${CMAKE_SOURCE_DIR}/../interfaces/include)
    target_link_libraries(${tgt} PRIVATE Catch2::Catch2WithMain)
    catch_discover_tests(${tgt})
endforeach()

list(LENGTH BOLT_UNIT_TESTS BOLT_UNIT_TEST_COUNT)
message(STATUS "bolt: ${BOLT_UNIT_TEST_COUNT} co-located unit test files")

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
