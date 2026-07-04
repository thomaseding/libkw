cmake_minimum_required(VERSION 4.3)

# Reads warning_flags.txt and defines TEST_WARNING_FLAGS with the file content.
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/warning_flags.txt TEST_WARNING_FLAGS)

project(libkw LANGUAGES CXX)
add_library(libkw INTERFACE
    src/libkw.hpp
)
target_compile_options(libkw
    INTERFACE
        -freflection
)
target_compile_features(libkw INTERFACE cxx_std_26)
target_include_directories(libkw
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(test_basic
    test/basic.cpp
)
target_link_libraries(test_basic
    PRIVATE
        libkw
)
target_compile_options(test_basic
    PRIVATE
        ${TEST_WARNING_FLAGS}
)

add_executable(test_modal
    test/modal.cpp
)
target_link_libraries(test_modal
    PRIVATE
        libkw
)
target_compile_options(test_modal
    PRIVATE
        ${TEST_WARNING_FLAGS}
)

add_executable(test_standalone_include
    test/standalone_include.cpp
)
target_link_libraries(test_standalone_include
    PRIVATE
        libkw
)
target_compile_options(test_standalone_include
    PRIVATE
        ${TEST_WARNING_FLAGS}
)
