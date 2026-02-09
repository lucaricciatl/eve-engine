cmake_minimum_required(VERSION 3.24)

add_executable(PostProcessingExample main.cpp)

target_link_libraries(PostProcessingExample PRIVATE core imgui)

target_include_directories(PostProcessingExample PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(PostProcessingExample PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
