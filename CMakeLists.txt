cmake_minimum_required(VERSION 3.24)
project(EVE LANGUAGES CXX)

if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_VALIDATION_LAYERS "Enable Vulkan validation layers" ON)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_GetProperties(glm)
if(NOT glm_POPULATED)
    FetchContent_Populate(glm)
endif()

# ImGui (dear imgui) for UI overlay
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.6
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

FetchContent_Declare(
    lunasvg
    GIT_REPOSITORY https://github.com/sammycage/lunasvg.git
    GIT_TAG v2.3.9
)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(lunasvg)

find_program(GLSLC_EXECUTABLE NAMES glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "Unable to locate glslc. Please ensure that the Vulkan SDK is installed and glslc is on PATH or set VULKAN_SDK.")
endif()

option(AUTO_FETCH_VULKAN_LOADER "Automatically fetch Vulkan-Loader" ON)
option(AUTO_FETCH_VULKAN_HEADERS "Automatically fetch Vulkan-Headers" ON)

set(VULKAN_LOADER_ROOT ${CMAKE_SOURCE_DIR}/external/Vulkan-Loader)
if(NOT EXISTS ${VULKAN_LOADER_ROOT}/CMakeLists.txt)
    if(AUTO_FETCH_VULKAN_LOADER)
        FetchContent_Declare(
            vulkan_loader
            GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Loader.git
            GIT_TAG v1.3.290
            GIT_SHALLOW TRUE
            GIT_SUBMODULES ""
            GIT_SUBMODULES_RECURSE FALSE
        )
        FetchContent_GetProperties(vulkan_loader)
        if(NOT vulkan_loader_POPULATED)
            FetchContent_Populate(vulkan_loader)
        endif()
        set(VULKAN_LOADER_ROOT ${vulkan_loader_SOURCE_DIR})
    else()
        message(FATAL_ERROR "Vulkan-Loader repository not found. Please clone it into external/Vulkan-Loader")
    endif()
endif()

set(VULKAN_HEADERS_SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/Vulkan-Headers)
if(EXISTS ${VULKAN_LOADER_ROOT}/external/Vulkan-Headers/CMakeLists.txt)
    set(VULKAN_HEADERS_SOURCE_DIR ${VULKAN_LOADER_ROOT}/external/Vulkan-Headers)
endif()
if(NOT EXISTS ${VULKAN_HEADERS_SOURCE_DIR}/CMakeLists.txt)
    if(AUTO_FETCH_VULKAN_HEADERS)
        FetchContent_Declare(
            vulkan_headers
            GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
            GIT_TAG v1.3.290
            GIT_SHALLOW TRUE
        )
        FetchContent_GetProperties(vulkan_headers)
        if(NOT vulkan_headers_POPULATED)
            FetchContent_Populate(vulkan_headers)
        endif()
        set(VULKAN_HEADERS_SOURCE_DIR ${vulkan_headers_SOURCE_DIR})
    else()
        message(FATAL_ERROR "Vulkan-Headers repository not found. Please clone it into external/Vulkan-Headers")
    endif()
endif()

set(VULKAN_HEADERS_BUILD_DIR ${CMAKE_BINARY_DIR}/external/Vulkan-Headers)
add_subdirectory(${VULKAN_HEADERS_SOURCE_DIR} ${VULKAN_HEADERS_BUILD_DIR})

set(VULKAN_HEADERS_INCLUDE_DIR ${VULKAN_HEADERS_SOURCE_DIR}/include)

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${VULKAN_LOADER_ROOT} ${CMAKE_BINARY_DIR}/external/Vulkan-Loader)

set(SHADER_SOURCES
    # Graphics shaders (vertex + fragment)
    ${CMAKE_SOURCE_DIR}/shaders/graphics/cube.vert
    ${CMAKE_SOURCE_DIR}/shaders/graphics/cube.frag
    ${CMAKE_SOURCE_DIR}/shaders/graphics/toon.frag
    ${CMAKE_SOURCE_DIR}/shaders/graphics/rim.frag
    ${CMAKE_SOURCE_DIR}/shaders/graphics/heat.frag
    ${CMAKE_SOURCE_DIR}/shaders/graphics/gradient.frag
    ${CMAKE_SOURCE_DIR}/shaders/graphics/wireframe.frag
    ${CMAKE_SOURCE_DIR}/shaders/graphics/glow.frag
    ${CMAKE_SOURCE_DIR}/shaders/graphics/shadow.vert
    ${CMAKE_SOURCE_DIR}/shaders/graphics/particle.vert
    ${CMAKE_SOURCE_DIR}/shaders/graphics/particle.frag
    # Post-processing shaders
    ${CMAKE_SOURCE_DIR}/shaders/post/fullscreen.vert
    ${CMAKE_SOURCE_DIR}/shaders/post/blur.frag
    ${CMAKE_SOURCE_DIR}/shaders/post/bloom_extract.frag
    ${CMAKE_SOURCE_DIR}/shaders/post/bloom_blur.frag
    ${CMAKE_SOURCE_DIR}/shaders/post/bloom_composite.frag
    ${CMAKE_SOURCE_DIR}/shaders/post/ssao.frag
    ${CMAKE_SOURCE_DIR}/shaders/post/ssao_blur.frag
    ${CMAKE_SOURCE_DIR}/shaders/post/tonemapping.frag
    ${CMAKE_SOURCE_DIR}/shaders/post/color_grading.frag
    ${CMAKE_SOURCE_DIR}/shaders/post/fxaa.frag
    ${CMAKE_SOURCE_DIR}/shaders/post/taa.frag
    ${CMAKE_SOURCE_DIR}/shaders/post/dof.frag
    ${CMAKE_SOURCE_DIR}/shaders/post/motion_blur.frag
    ${CMAKE_SOURCE_DIR}/shaders/post/vignette_chromatic.frag
    # Shadow shaders
    ${CMAKE_SOURCE_DIR}/shaders/shadow/cascade_shadow.vert
    ${CMAKE_SOURCE_DIR}/shaders/shadow/cascade_shadow.frag
    # Compute shaders
    ${CMAKE_SOURCE_DIR}/shaders/compute/nbody.comp
    ${CMAKE_SOURCE_DIR}/shaders/compute/particle_billboard.comp
    ${CMAKE_SOURCE_DIR}/shaders/compute/collision_broad_phase.comp
    ${CMAKE_SOURCE_DIR}/shaders/compute/collision_broad_phase_write.comp
    ${CMAKE_SOURCE_DIR}/shaders/compute/collision_narrow_phase.comp
    ${CMAKE_SOURCE_DIR}/shaders/compute/collision_pair_gen.comp
    ${CMAKE_SOURCE_DIR}/shaders/compute/collision_prefix_sum.comp
    ${CMAKE_SOURCE_DIR}/shaders/compute/collision_bitonic_sort.comp
)
set(SHADER_OUTPUTS)
foreach(SHADER_FILE ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
    # Get relative path from shaders/ directory to preserve subdirectory structure
    file(RELATIVE_PATH SHADER_REL_PATH ${CMAKE_SOURCE_DIR}/shaders ${SHADER_FILE})
    get_filename_component(SHADER_SUBDIR ${SHADER_REL_PATH} DIRECTORY)
    set(SPIRV_FILE ${CMAKE_BINARY_DIR}/shaders/${SHADER_SUBDIR}/${SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT ${SPIRV_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders/${SHADER_SUBDIR}
        COMMAND ${GLSLC_EXECUTABLE} -o ${SPIRV_FILE} ${SHADER_FILE}
        DEPENDS ${SHADER_FILE}
        COMMENT "Compiling ${SHADER_NAME}"
    )
    list(APPEND SHADER_OUTPUTS ${SPIRV_FILE})
endforeach()

add_custom_target(Shaders DEPENDS ${SHADER_OUTPUTS})

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui
    PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    ${VULKAN_HEADERS_INCLUDE_DIR}
)
target_link_libraries(imgui PUBLIC glfw Vulkan::Loader)
if(MSVC)
    target_compile_options(imgui PRIVATE /W4 /permissive-)
else()
    target_compile_options(imgui PRIVATE -Wall -Wextra -pedantic)
endif()

set(CORE_ENGINE_SOURCES
    src/engine/GameEngine.cpp
    src/engine/Material.cpp
    src/engine/PhysicsSystem.cpp
    src/engine/GpuCollisionSystem.cpp
    src/engine/ParticleSystem.cpp
    src/engine/MolecularDynamics.cpp
    src/engine/InputManager.cpp
    src/engine/DeformableBody.cpp
    src/engine/SoftBodyVolume.cpp
    src/engine/assets/MeshLoader.cpp
    src/engine/assets/ImageWriter.cpp
    src/engine/assets/TextureLoader.cpp
    src/engine/assets/StbImage.cpp
    # New engine systems
    src/engine/Serialization.cpp
    src/engine/Audio.cpp
    src/engine/Animation.cpp
    src/engine/CameraSystem.cpp
    src/engine/Network.cpp
    src/engine/DebugTools.cpp
    src/engine/JobSystem.cpp
    src/engine/UISystem.cpp
    src/engine/Colliders.cpp
    src/engine/AssetPipeline.cpp
    src/engine/PostProcessing.cpp
    src/core/units/MetricUnits.cpp
    src/core/primitives/PrimitiveGenerator.cpp
    src/core/Vertex.cpp
    src/core/PipelineLibrary.cpp
    src/ui/ImGuiLayer.cpp
    src/ui/Ui.cpp
    src/ui/Mouse.cpp
    src/ui/Draw.cpp
    src/ui/CustomUi.cpp
    src/ui/FontLibrary.cpp
)

set(CORE_RENDERER_SOURCES
    src/core/VulkanRenderer.cpp
    src/core/VulkanRenderer.Ui.cpp
    src/core/WindowManager.cpp
    src/core/sky/Sky.cpp
    src/core/filesystem/FileSystem.cpp
)

set(SHADER_BINARY_DIR ${CMAKE_BINARY_DIR}/shaders)

add_library(core STATIC ${CORE_ENGINE_SOURCES} ${CORE_RENDERER_SOURCES})
target_include_directories(core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}
        ${glm_SOURCE_DIR}
        ${glfw_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external/stb
    ${VULKAN_HEADERS_INSTALL_DIR}/include
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_compile_definitions(core
    PUBLIC
        GLM_ENABLE_EXPERIMENTAL
    PRIVATE
        GLFW_INCLUDE_VULKAN
        SHADER_BINARY_DIR="${SHADER_BINARY_DIR}"
)

if(ENABLE_VALIDATION_LAYERS)
    target_compile_definitions(core PRIVATE ENABLE_VALIDATION_LAYERS=1)
endif()

target_link_libraries(core
    PUBLIC
        glfw
        Vulkan::Loader
        imgui
)
if(TARGET lunasvg)
    target_link_libraries(core PRIVATE lunasvg)
endif()

if(WIN32)
    target_link_libraries(core PRIVATE windowscodecs)
endif()

add_dependencies(core Shaders)

set_property(TARGET core PROPERTY FOLDER "core")
set_property(TARGET Shaders PROPERTY FOLDER "core")

if(MSVC)
    target_compile_options(core PRIVATE /W4 /permissive-)
else()
    target_compile_options(core PRIVATE -Wall -Wextra -pedantic)
endif()

add_executable(CubeExample examples/CubeExample/main.cpp)
target_link_libraries(CubeExample PRIVATE core)

add_executable(ParticleExample examples/particles/main.cpp)
target_link_libraries(ParticleExample PRIVATE core)

add_executable(MaterialsExample examples/materials/main.cpp)
target_link_libraries(MaterialsExample PRIVATE core)

add_executable(DustCubeExample examples/dust_cube/main.cpp)
target_link_libraries(DustCubeExample PRIVATE core)

add_executable(DeformableExample examples/deformable/main.cpp)
target_link_libraries(DeformableExample PRIVATE core)

add_executable(NBodyExample examples/nbody/main.cpp)
target_link_libraries(NBodyExample PRIVATE core)

add_executable(MolecularDynamicsExample examples/molecular_dynamics/main.cpp)
target_link_libraries(MolecularDynamicsExample PRIVATE core)

add_executable(LightsExample examples/lights/main.cpp)
target_link_libraries(LightsExample PRIVATE core)

add_executable(GlassExample examples/glass/main.cpp)
target_link_libraries(GlassExample PRIVATE core)

add_executable(PhysicsExample examples/physics/main.cpp)
target_link_libraries(PhysicsExample PRIVATE core)

add_executable(GpuCollisionExample examples/gpu_collision/main.cpp)
target_link_libraries(GpuCollisionExample PRIVATE core)

add_executable(EmittingBodiesExample examples/emitting_bodies/main.cpp)
target_link_libraries(EmittingBodiesExample PRIVATE core)

add_executable(LatticeExample examples/lattice/main.cpp)
target_link_libraries(LatticeExample PRIVATE core)

add_executable(DrawOverlayExample examples/draw_overlay/main.cpp)
target_link_libraries(DrawOverlayExample PRIVATE core)

add_executable(WindowFeaturesExample examples/window_features/main.cpp)
target_link_libraries(WindowFeaturesExample PRIVATE core)

add_executable(SkyImageBackgroundExample examples/sky_image_background/main.cpp)
target_link_libraries(SkyImageBackgroundExample PRIVATE core)

add_executable(VolumetricFogExample examples/volumetric_fog/main.cpp)
target_link_libraries(VolumetricFogExample PRIVATE core)

add_executable(ShaderStylesExample examples/shader_styles/main.cpp)
target_link_libraries(ShaderStylesExample PRIVATE core)

add_executable(TerminalAppExample examples/terminal_app/main.cpp)

add_executable(InAppConsoleExample examples/in_app_console/main.cpp)
target_link_libraries(InAppConsoleExample PRIVATE core)

add_executable(ReflectionsExample examples/reflections/main.cpp)
target_link_libraries(ReflectionsExample PRIVATE core)

add_executable(PostProcessingExample examples/post_processing/main.cpp)
target_link_libraries(PostProcessingExample PRIVATE core)

set(EXAMPLE_TARGETS
    CubeExample
    ParticleExample
    MaterialsExample
    DustCubeExample
    DeformableExample
    NBodyExample
    MolecularDynamicsExample
    LightsExample
    GlassExample
    PhysicsExample
    GpuCollisionExample
    EmittingBodiesExample
    LatticeExample
    DrawOverlayExample
    WindowFeaturesExample
    SkyImageBackgroundExample
    VolumetricFogExample
    ShaderStylesExample
    TerminalAppExample
    InAppConsoleExample
    ReflectionsExample
    PostProcessingExample
)
foreach(example_target IN LISTS EXAMPLE_TARGETS)
    set_property(TARGET ${example_target} PROPERTY FOLDER "examples")
endforeach()

# Copy assets next to the executable so runtime asset lookup succeeds when launched from the build tree
add_custom_command(TARGET SkyImageBackgroundExample POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:SkyImageBackgroundExample>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:SkyImageBackgroundExample>/assets"
)

if(MSVC)
    target_compile_options(CubeExample PRIVATE /W4 /permissive-)
    target_compile_options(ParticleExample PRIVATE /W4 /permissive-)
    target_compile_options(MaterialsExample PRIVATE /W4 /permissive-)
    target_compile_options(DustCubeExample PRIVATE /W4 /permissive-)
    target_compile_options(DeformableExample PRIVATE /W4 /permissive-)
    target_compile_options(NBodyExample PRIVATE /W4 /permissive-)
    target_compile_options(MolecularDynamicsExample PRIVATE /W4 /permissive-)
    target_compile_options(LightsExample PRIVATE /W4 /permissive-)
    target_compile_options(GlassExample PRIVATE /W4 /permissive-)
    target_compile_options(GpuCollisionExample PRIVATE /W4 /permissive-)
    target_compile_options(PhysicsExample PRIVATE /W4 /permissive-)
    target_compile_options(EmittingBodiesExample PRIVATE /W4 /permissive-)
    target_compile_options(LatticeExample PRIVATE /W4 /permissive-)
    target_compile_options(DrawOverlayExample PRIVATE /W4 /permissive-)
    target_compile_options(WindowFeaturesExample PRIVATE /W4 /permissive-)
    target_compile_options(SkyImageBackgroundExample PRIVATE /W4 /permissive-)
    target_compile_options(VolumetricFogExample PRIVATE /W4 /permissive-)
    target_compile_options(ShaderStylesExample PRIVATE /W4 /permissive-)
    target_compile_options(TerminalAppExample PRIVATE /W4 /permissive-)
    target_compile_options(InAppConsoleExample PRIVATE /W4 /permissive-)
    target_compile_options(PostProcessingExample PRIVATE /W4 /permissive-)
    target_compile_options(GpuCollisionExample PRIVATE /W4 /permissive-)
    target_compile_options(ReflectionsExample PRIVATE /W4 /permissive-)
else()
    target_compile_options(CubeExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(ParticleExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(MaterialsExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(DustCubeExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(DeformableExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(NBodyExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(MolecularDynamicsExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(LightsExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(GlassExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(GpuCollisionExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(PhysicsExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(EmittingBodiesExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(LatticeExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(DrawOverlayExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(WindowFeaturesExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(SkyImageBackgroundExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(VolumetricFogExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(ShaderStylesExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(TerminalAppExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(InAppConsoleExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(PostProcessingExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(ReflectionsExample PRIVATE -Wall -Wextra -pedantic)
endif()

set_source_files_properties(${SHADER_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)

# ============================
# Unit testing configuration
# ============================
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_subdirectory(tests)

# Organize external libraries into a solution folder for MSVS
set(EXTERNAL_TARGETS
    glfw
    imgui
    lunasvg
    gtest
    gtest_main
    gmock
    gmock_main
    vulkan
    asm_offset
    asm-offset
    asm_offset_gen
)
foreach(external_target IN LISTS EXTERNAL_TARGETS)
    if(TARGET ${external_target})
        get_target_property(target_alias ${external_target} ALIASED_TARGET)
        if(NOT target_alias)
            set_property(TARGET ${external_target} PROPERTY FOLDER "external")
        endif()
    endif()
endforeach()

# Additional MSVS folder organization
if(TARGET glfw)
    set_property(TARGET glfw PROPERTY FOLDER "external/GLFW3")
endif()

if(DEFINED glfw_SOURCE_DIR)
    get_property(glfw_targets DIRECTORY ${glfw_SOURCE_DIR} PROPERTY BUILDSYSTEM_TARGETS)
    foreach(glfw_target IN LISTS glfw_targets)
        if(TARGET ${glfw_target})
            get_target_property(glfw_alias ${glfw_target} ALIASED_TARGET)
            if(NOT glfw_alias)
                set_property(TARGET ${glfw_target} PROPERTY FOLDER "external/GLFW3")
            endif()
        endif()
    endforeach()
endif()

if(TARGET update_mappings)
    set_property(TARGET update_mappings PROPERTY FOLDER "external/GLFW3")
endif()

# Group Vulkan loader and asm-offset generator targets under external
if(DEFINED VULKAN_LOADER_ROOT)
    get_property(loader_targets DIRECTORY ${VULKAN_LOADER_ROOT} PROPERTY BUILDSYSTEM_TARGETS)
    foreach(loader_target IN LISTS loader_targets)
        if(TARGET ${loader_target})
            get_target_property(loader_alias ${loader_target} ALIASED_TARGET)
            if(NOT loader_alias)
                set_property(TARGET ${loader_target} PROPERTY FOLDER "external")
            endif()
        endif()
    endforeach()
endif()

if(TARGET loader_asm_gen_files)
    set_property(TARGET loader_asm_gen_files PROPERTY FOLDER "external")
endif()

set(HELPER_TARGETS
    ALL_BUILD
    ZERO_CHECK
    INSTALL
    RUN_TESTS
    PACKAGE
)
foreach(helper_target IN LISTS HELPER_TARGETS)
    if(TARGET ${helper_target})
        set_property(TARGET ${helper_target} PROPERTY FOLDER "external/Helper Targets")
    endif()
endforeach()
