include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(test_crc16 unit/test_crc16.cpp)

target_include_directories(test_crc16 PRIVATE
    ${CMAKE_SOURCE_DIR}/shared
)

target_link_libraries(test_crc16 PRIVATE Catch2::Catch2WithMain)

add_executable(test_error_trace unit/test_error_trace.cpp)

target_include_directories(test_error_trace PRIVATE
    ${CMAKE_SOURCE_DIR}/shared
)

target_link_libraries(test_error_trace PRIVATE Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(test_crc16)
catch_discover_tests(test_error_trace)
