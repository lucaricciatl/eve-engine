include(GoogleTest)

# Physics tests
add_executable(physics_tests
    test_main.cpp
    test_physics.cpp
)

set_target_properties(physics_tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_link_libraries(physics_tests
    PRIVATE
        GTest::gtest_main
        core
)

target_include_directories(physics_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(physics_tests)
set_property(TARGET physics_tests PROPERTY FOLDER "tests")

# Rendering tests
add_executable(rendering_tests
    test_rendering.cpp
    test_rendering_headless.cpp
)

set_target_properties(rendering_tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_link_libraries(rendering_tests
    PRIVATE
        GTest::gtest_main
        core
)

target_include_directories(rendering_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/external/stb
)

gtest_discover_tests(rendering_tests)
set_property(TARGET rendering_tests PROPERTY FOLDER "tests")

# Performance tests
add_executable(performance_tests
    test_rendering_performance.cpp
)

set_target_properties(performance_tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_link_libraries(performance_tests
    PRIVATE
        GTest::gtest_main
        core
)

target_include_directories(performance_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(performance_tests)
set_property(TARGET performance_tests PROPERTY FOLDER "tests")

# Texture generator tests
add_executable(texture_generator_tests
    test_texture_generator.cpp
)

set_target_properties(texture_generator_tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_link_libraries(texture_generator_tests
    PRIVATE
        GTest::gtest_main
        core
)

target_include_directories(texture_generator_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(texture_generator_tests)
set_property(TARGET texture_generator_tests PROPERTY FOLDER "tests")
