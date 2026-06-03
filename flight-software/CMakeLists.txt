cmake_minimum_required(VERSION 3.22)
project(bolt-tests CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
