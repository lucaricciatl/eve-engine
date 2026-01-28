# Tests for the Vulkan engine core
add_executable(engine_tests
    test_main.cpp
    test_rendering.cpp
    test_physics.cpp
)

set_target_properties(engine_tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_link_libraries(engine_tests
    PRIVATE
        GTest::gtest_main
        core
)

target_include_directories(engine_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/stb
)

include(GoogleTest)
gtest_discover_tests(engine_tests)

set_property(TARGET engine_tests PROPERTY FOLDER "tests")
