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

find_program(GLSLC_EXECUTABLE NAMES glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "Unable to locate glslc. Please ensure that the Vulkan SDK is installed and glslc is on PATH or set VULKAN_SDK.")
endif()

set(VULKAN_LOADER_ROOT ${CMAKE_SOURCE_DIR}/external/Vulkan-Loader)
if(NOT EXISTS ${VULKAN_LOADER_ROOT}/CMakeLists.txt)
    message(FATAL_ERROR "Vulkan-Loader repository not found. Please clone it into external/Vulkan-Loader")
endif()

set(VULKAN_HEADERS_SOURCE_DIR ${VULKAN_LOADER_ROOT}/external/Vulkan-Headers CACHE PATH "" FORCE)
if(NOT EXISTS ${VULKAN_HEADERS_SOURCE_DIR}/CMakeLists.txt)
    message(FATAL_ERROR "Vulkan-Headers repository not found under ${VULKAN_LOADER_ROOT}/external")
endif()

set(VULKAN_HEADERS_INSTALL_DIR ${VULKAN_HEADERS_SOURCE_DIR}/install)
set(VulkanHeaders_DIR ${VULKAN_HEADERS_INSTALL_DIR}/share/cmake/VulkanHeaders CACHE PATH "" FORCE)
if(NOT EXISTS ${VulkanHeaders_DIR}/VulkanHeadersConfig.cmake)
    message(FATAL_ERROR "Vulkan-Headers install not found. Run cmake --build build/vkheaders --target install first.")
endif()

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${VULKAN_LOADER_ROOT} ${CMAKE_BINARY_DIR}/external/Vulkan-Loader)

set(SHADER_SOURCES
    ${CMAKE_SOURCE_DIR}/shaders/cube.vert
    ${CMAKE_SOURCE_DIR}/shaders/cube.frag
    ${CMAKE_SOURCE_DIR}/shaders/shadow.vert
    ${CMAKE_SOURCE_DIR}/shaders/particle.vert
    ${CMAKE_SOURCE_DIR}/shaders/particle.frag
    ${CMAKE_SOURCE_DIR}/shaders/nbody.comp
    ${CMAKE_SOURCE_DIR}/shaders/particle_billboard.comp
)
set(SHADER_OUTPUTS)
foreach(SHADER_FILE ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
    set(SPIRV_FILE ${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT ${SPIRV_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
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
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui
    PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    ${VULKAN_HEADERS_INSTALL_DIR}/include
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
    src/engine/ParticleSystem.cpp
    src/engine/MolecularDynamics.cpp
    src/engine/InputManager.cpp
    src/engine/DeformableBody.cpp
    src/engine/SoftBodyVolume.cpp
    src/engine/assets/MeshLoader.cpp
    src/engine/assets/ImageWriter.cpp
    src/engine/assets/TextureLoader.cpp
    src/engine/assets/StbImage.cpp
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
    src/core/WindowManager.cpp
    src/core/sky/Sky.cpp
    src/core/filesystem/FileSystem.cpp
)

set(SHADER_BINARY_DIR ${CMAKE_BINARY_DIR}/shaders)

add_library(core STATIC ${CORE_ENGINE_SOURCES} ${CORE_RENDERER_SOURCES})
target_include_directories(core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
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

if(WIN32)
    target_link_libraries(core PRIVATE windowscodecs)
endif()

add_dependencies(core Shaders)

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

# Copy assets next to the executable so runtime asset lookup succeeds when launched from the build tree
add_custom_command(TARGET SkyImageBackgroundExample POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:SkyImageBackgroundExample>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:SkyImageBackgroundExample>/assets"
)

if(MSVC)
    target_compile_options(CubeExample PRIVATE /W4 /permissive-)
    target_compile_options(ParticleExample PRIVATE /W4 /permissive-)
    target_compile_options(DeformableExample PRIVATE /W4 /permissive-)
    target_compile_options(NBodyExample PRIVATE /W4 /permissive-)
    target_compile_options(MolecularDynamicsExample PRIVATE /W4 /permissive-)
    target_compile_options(LightsExample PRIVATE /W4 /permissive-)
    target_compile_options(GlassExample PRIVATE /W4 /permissive-)
    target_compile_options(PhysicsExample PRIVATE /W4 /permissive-)
    target_compile_options(EmittingBodiesExample PRIVATE /W4 /permissive-)
    target_compile_options(LatticeExample PRIVATE /W4 /permissive-)
    target_compile_options(DrawOverlayExample PRIVATE /W4 /permissive-)
    target_compile_options(WindowFeaturesExample PRIVATE /W4 /permissive-)
    target_compile_options(SkyImageBackgroundExample PRIVATE /W4 /permissive-)
    target_compile_options(VolumetricFogExample PRIVATE /W4 /permissive-)
else()
    target_compile_options(CubeExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(ParticleExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(DeformableExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(NBodyExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(MolecularDynamicsExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(LightsExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(GlassExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(PhysicsExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(EmittingBodiesExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(LatticeExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(DrawOverlayExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(WindowFeaturesExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(SkyImageBackgroundExample PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(VolumetricFogExample PRIVATE -Wall -Wextra -pedantic)
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
