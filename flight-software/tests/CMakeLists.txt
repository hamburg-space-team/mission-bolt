include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)
include(Catch)

# Unit tests live next to the code they test: shared/utils/crc16.test.cpp
# checks shared/utils/crc16.hpp. One executable per test file, discovered
# at configure time (CONFIGURE_DEPENDS re-globs on build) - adding a test
# needs no CMake edit. The flight builds cannot pick these up: the
# cprojects list their sources explicitly
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
    target_include_directories(${tgt} PRIVATE ${CMAKE_SOURCE_DIR}/shared)
    target_link_libraries(${tgt} PRIVATE Catch2::Catch2WithMain)
    catch_discover_tests(${tgt})
endforeach()

list(LENGTH BOLT_UNIT_TESTS BOLT_UNIT_TEST_COUNT)
message(STATUS "bolt: ${BOLT_UNIT_TEST_COUNT} co-located unit test files")
