#include "core/VulkanRenderer.hpp"
#include "core/PipelineLibrary.hpp"
#include "core/RenderData.hpp"
#include "engine/assets/ImageWriter.hpp"
#include "engine/GpuCollisionSystem.hpp"

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <set>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

#ifndef SHADER_BINARY_DIR
#define SHADER_BINARY_DIR ""
#endif

namespace {
#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* /*pUserData*/)
{
    (void)messageType;
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << '\n';
    }
    return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                      const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                      const VkAllocationCallbacks* pAllocator,
                                      VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks* pAllocator)
{
    auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

const std::vector<Vertex> VERTICES = {
    // Front face
    {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.2f, 0.2f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.5f}, {1.0f, 0.7f, 0.2f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 0.2f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.5f}, {0.7f, 1.0f, 0.2f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    // Back face
    {{-0.5f, -0.5f, -0.5f}, {0.2f, 0.3f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
    {{-0.5f, 0.5f, -0.5f}, {0.2f, 0.6f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.2f, 0.9f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.2f, 0.5f, 1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
    // Left face
    {{-0.5f, -0.5f, -0.5f}, {0.2f, 1.0f, 0.4f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{-0.5f, -0.5f, 0.5f}, {0.2f, 1.0f, 0.7f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{-0.5f, 0.5f, 0.5f}, {0.2f, 1.0f, 0.9f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {0.2f, 1.0f, 0.6f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    // Right face
    {{0.5f, -0.5f, -0.5f}, {0.9f, 0.2f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.9f, 0.4f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
    {{0.5f, 0.5f, 0.5f}, {0.9f, 0.6f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    {{0.5f, -0.5f, 0.5f}, {0.9f, 0.8f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    // Top face
    {{-0.5f, 0.5f, -0.5f}, {0.4f, 0.2f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.5f}, {0.6f, 0.2f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.5f}, {0.8f, 0.2f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {1.0f, 0.2f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
    // Bottom face
    {{-0.5f, -0.5f, -0.5f}, {0.4f, 1.0f, 0.2f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.6f, 1.0f, 0.2f, 1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
    {{0.5f, -0.5f, 0.5f}, {0.8f, 1.0f, 0.2f, 1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
    {{-0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 0.2f, 1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
};

const std::vector<uint16_t> INDICES = {
    0, 1, 2, 2, 3, 0,        // front
    4, 5, 6, 6, 7, 4,        // back
    8, 9, 10, 10, 11, 8,     // left
    12, 13, 14, 14, 15, 12,  // right
    16, 17, 18, 18, 19, 16,  // top
    20, 21, 22, 22, 23, 20   // bottom
};

const std::vector<Vertex> LINE_VERTICES = {
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},
    {{0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},

    {{0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},
    {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},

    {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},

    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},

    {{-0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},
    {{0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},

    {{0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},
    {{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},

    {{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},
    {{-0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},

    {{-0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},
    {{-0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},

    {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},
    {{-0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},

    {{0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},
    {{0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},

    {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},
    {{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},

    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},
    {{-0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}, {}, {}},
};

std::string makeMeshCacheKey(const std::string& path)
{
    return path;
}

} // namespace

VulkanRenderer::VulkanRenderer(vkengine::IGameEngine& engineRef)
    : startTime(std::chrono::steady_clock::now())
    , lastFrameTime(startTime)
{
    attachEngine(engineRef);
}

void VulkanRenderer::attachEngine(vkengine::IGameEngine& engineRef)
{
    engine = &engineRef;
    inputManager.attachRegistry(&engine->scene().registry());
    try {
        projectRootPath = std::filesystem::current_path();
    } catch (const std::exception&) {
        projectRootPath = std::filesystem::path{};
    }
}

void VulkanRenderer::setWindowConfig(const WindowConfig& config)
{
    windowConfig = config;
    if (windowConfig.mode == WindowMode::FixedSize) {
        windowConfig.resizable = false;
    }
}

void VulkanRenderer::setWindowMode(WindowMode mode)
{
    windowManager.setMode(mode);
    windowConfig = windowManager.getConfig();
}

void VulkanRenderer::setWindowOpacity(float opacity)
{
    windowManager.setOpacity(opacity);
    windowConfig = windowManager.getConfig();
}

void VulkanRenderer::setSkyColor(const glm::vec3& color)
{
    skySettings.color = glm::vec4{color, 1.0f};
}

void VulkanRenderer::setSkyColor(const glm::vec4& color)
{
    skySettings.color = color;
}

bool VulkanRenderer::setSkyFromFile(const std::filesystem::path& path)
{
    try {
        const auto resolved = resolveAssetPath(path.string());
        if (resolved.empty() || !std::filesystem::exists(resolved)) {
            return false;
        }

        auto color = vkcore::sky::loadSkyColorFromFile(resolved);
        if (!color) {
            return false;
        }
        skySettings.color = *color;
        pendingSkyTexturePath = resolved;
        skyTextureRequested = true;
        if (device != VK_NULL_HANDLE) {
            loadSkyTextureForUi(resolved);
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void VulkanRenderer::requestExit()
{
    exitRequested.store(true);
    windowManager.requestClose();
}

void VulkanRenderer::run()
{
    if (engine == nullptr) {
        throw std::runtime_error("Renderer requires an engine before running");
    }
    exitRequested.store(false);
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

bool VulkanRenderer::renderSingleFrameToJpeg(const std::filesystem::path& outputPath)
{
    if (engine == nullptr) {
        throw std::runtime_error("Renderer has no engine attached");
    }

    capturePath = outputPath;
    captureRequested = true;
    captureCompleted = false;
    captureFailed = false;
    exitRequested.store(false);

    initWindow();
    initVulkan();
    ensureCaptureBuffer(swapChainExtent.width, swapChainExtent.height);
    drawFrame();
    vkDeviceWaitIdle(device);
    cleanup();

    return captureCompleted && !captureFailed;
}

bool VulkanRenderer::renderFrameToJpegAt(const std::filesystem::path& outputPath, uint32_t targetFrame, float fixedDeltaSeconds)
{
    if (engine == nullptr) {
        throw std::runtime_error("Renderer has no engine attached");
    }

    const uint32_t frameToCapture = std::max(1u, targetFrame);
    const float frameDeltaSeconds = fixedDeltaSeconds > 0.0f ? fixedDeltaSeconds : (1.0f / 30.0f);

    capturePath = outputPath;
    captureRequested = false;
    captureCompleted = false;
    captureFailed = false;
    exitRequested.store(false);

    initWindow();
    initVulkan();
    ensureCaptureBuffer(swapChainExtent.width, swapChainExtent.height);

    const std::chrono::steady_clock::duration frameStep =
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<float>(frameDeltaSeconds));
    for (uint32_t frame = 1; frame <= frameToCapture && !captureFailed; ++frame) {
        if (frame == frameToCapture) {
            captureRequested = true;
        }
        lastFrameTime = std::chrono::steady_clock::now() - frameStep;
        drawFrame();
        if (captureCompleted) {
            break;
        }
    }

    vkDeviceWaitIdle(device);
    cleanup();

    return captureCompleted && !captureFailed;
}

void VulkanRenderer::initWindow()
{
    if (windowConfig.width == 0) {
        windowConfig.width = WIDTH;
    }
    if (windowConfig.height == 0) {
        windowConfig.height = HEIGHT;
    }

    windowManager.setConfig(windowConfig);
    window = windowManager.createWindow();
    windowConfig = windowManager.getConfig();

    glfwSetWindowUserPointer(window, this);
    setupInputCallbacks();
    printHotkeyHelp();
}

void VulkanRenderer::initVulkan()
{
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createShadowRenderPass();
    createReflectionRenderPass();
    createPostProcessRenderPass();
    createDescriptorSetLayout();
    createMaterialDescriptorPool();
    createPipelines();
    createDepthResources();
    createShadowResources();
    createFramebuffers();
    createCommandPool();
    createMaterialResources();
    createReflectionResources();
    createPostProcessResources(); // This also creates post-process pipelines
    createVertexBuffer();
    createLineVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    initUi();
    createCommandBuffers();
    createSyncObjects();
    initGpuCollision();
}

void VulkanRenderer::initGpuCollision()
{
    // Initialize GPU collision system with Vulkan resources
    auto* gpuCollision = engine->physics().gpuCollision();
    if (gpuCollision) {
        std::filesystem::path shaderPath = SHADER_BINARY_DIR;
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        gpuCollision->initialize(device, physicalDevice, graphicsQueue, 
                                 indices.graphicsFamily.value(), shaderPath);
        gpuCollision->setEnabled(true);
    }
}

void VulkanRenderer::mainLoop()
{
    constexpr float TARGET_FPS = 30.0f;
    const auto targetFrameDuration = std::chrono::duration<float>(1.0f / TARGET_FPS);

    while (!exitRequested.load() && !windowManager.shouldClose()) {
        const auto frameStart = std::chrono::steady_clock::now();

        windowManager.pollEvents();
        drawFrame();

        const auto frameEnd = std::chrono::steady_clock::now();
        const auto frameDuration = std::chrono::duration<float>(frameEnd - frameStart);
        if (frameDuration < targetFrameDuration) {
            const auto remaining = targetFrameDuration - frameDuration;
            std::this_thread::sleep_for(remaining);
        }
    }

    vkDeviceWaitIdle(device);
}

void VulkanRenderer::cleanup()
{
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
    }
    
    // Cleanup GPU collision system before destroying Vulkan resources
    if (auto* gpuCollision = engine->physics().gpuCollision()) {
        gpuCollision->cleanup();
    }
    
    cleanupSwapChain();
	destroySkyTexture();

    // Destroy user-registered ImGui textures.
    for (auto& [ds, res] : imguiUserTextures) {
        uiLayer.unregisterTexture(ds);
        destroyTextureResource(res);
    }
    imguiUserTextures.clear();

    destroyPostProcessResources();
    
    // Destroy the post-process render pass (not destroyed in destroyPostProcessResources)
    if (postProcessRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, postProcessRenderPass, nullptr);
        postProcessRenderPass = VK_NULL_HANDLE;
    }

    destroyUi();

    if (indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, indexBuffer, nullptr);
        indexBuffer = VK_NULL_HANDLE;
    }
    if (indexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, indexBufferMemory, nullptr);
        indexBufferMemory = VK_NULL_HANDLE;
    }

    if (vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vertexBuffer = VK_NULL_HANDLE;
    }
    if (vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vertexBufferMemory = VK_NULL_HANDLE;
    }

    if (lineVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, lineVertexBuffer, nullptr);
        lineVertexBuffer = VK_NULL_HANDLE;
    }
    if (lineVertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, lineVertexBufferMemory, nullptr);
        lineVertexBufferMemory = VK_NULL_HANDLE;
    }
    lineVertexBuffer = VK_NULL_HANDLE;
    lineVertexBufferMemory = VK_NULL_HANDLE;

    destroyDeformableBuffers();
    destroyMeshCache();

    destroyParticleBuffers();
    destroyCaptureResources();
    destroyMaterialResources();

    for (uint32_t i = 0; i < framesInFlight; ++i) {
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
    if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        descriptorSetLayout = VK_NULL_HANDLE;
    }
    if (materialDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, materialDescriptorSetLayout, nullptr);
        materialDescriptorSetLayout = VK_NULL_HANDLE;
    }

    vkDestroyCommandPool(device, commandPool, nullptr);

    vkDestroyDevice(device, nullptr);

    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);

    windowManager.destroy();
    window = nullptr;
    exitRequested.store(false);
}

void VulkanRenderer::recreateSwapChain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createRenderPass();
    createShadowRenderPass();
    createReflectionRenderPass();
    createPipelines();
    createDepthResources();
    createShadowResources();
    createReflectionResources();
    createPostProcessResources(); // Recreate post-process resources for new swapchain size
    createFramebuffers();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    rebuildUi();
    createCommandBuffers();
    imagesInFlight.assign(swapChainImages.size(), VK_NULL_HANDLE);
    allocateRenderFinishedSemaphores();
}

void VulkanRenderer::cleanupSwapChain()
{
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
    }
    destroyRenderFinishedSemaphores();
    destroyCaptureResources();
    destroyReflectionResources();
    destroyPostProcessResources(); // Destroy post-process resources tied to swapchain size

    uiLayer.onSwapchainDestroyed();
    skyTextureId = VK_NULL_HANDLE;

    if (!commandBuffers.empty()) {
        vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
        commandBuffers.clear();
    }

    destroyShadowResources();

    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }

    if (depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, depthImageView, nullptr);
        depthImageView = VK_NULL_HANDLE;
    }

    if (depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, depthImage, nullptr);
        depthImage = VK_NULL_HANDLE;
    }

    if (depthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, depthImageMemory, nullptr);
        depthImageMemory = VK_NULL_HANDLE;
    }

    vkDestroySwapchainKHR(device, swapChain, nullptr);

    for (size_t i = 0; i < swapChainImages.size(); ++i) {
        if (uniformBuffersMapped.size() > i && uniformBuffersMapped[i] != nullptr) {
            vkUnmapMemory(device, uniformBuffersMemory[i]);
            uniformBuffersMapped[i] = nullptr;
        }
        vkDestroyBuffer(device, uniformBuffers[i], nullptr);
        vkFreeMemory(device, uniformBuffersMemory[i], nullptr);

        if (reflectionUniformBuffersMapped.size() > i && reflectionUniformBuffersMapped[i] != nullptr) {
            vkUnmapMemory(device, reflectionUniformBuffersMemory[i]);
            reflectionUniformBuffersMapped[i] = nullptr;
        }
        vkDestroyBuffer(device, reflectionUniformBuffers[i], nullptr);
        vkFreeMemory(device, reflectionUniformBuffersMemory[i], nullptr);
    }

    uniformBuffers.clear();
    uniformBuffersMemory.clear();
    uniformBuffersMapped.clear();
    reflectionUniformBuffers.clear();
    reflectionUniformBuffersMemory.clear();
    reflectionUniformBuffersMapped.clear();

    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
    descriptorSets.clear();
    reflectionDescriptorSets.clear();

    for (auto& pipeline : stylePipelines) {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
    }
    graphicsPipeline = VK_NULL_HANDLE;
    if (linePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, linePipeline, nullptr);
        linePipeline = VK_NULL_HANDLE;
    }
    if (shadowPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, shadowPipeline, nullptr);
        shadowPipeline = VK_NULL_HANDLE;
    }
    if (particlePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, particlePipeline, nullptr);
        particlePipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }
    if (shadowRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, shadowRenderPass, nullptr);
        shadowRenderPass = VK_NULL_HANDLE;
    }
    if (reflectionRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, reflectionRenderPass, nullptr);
        reflectionRenderPass = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::createInstance()
{
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("Validation layers requested, but not available");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Cube";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    auto extensions = getRequiredExtensions();

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;
        createInfo.pNext = &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }
}

void VulkanRenderer::setupDebugMessenger()
{
    if (!enableValidationLayers) {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;

    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("Failed to set up debug messenger");
    }
}

void VulkanRenderer::createSurface()
{
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }
}

void VulkanRenderer::pickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& candidate : devices) {
        if (isDeviceSuitable(candidate)) {
            physicalDevice = candidate;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU");
    }
}

void VulkanRenderer::createLogicalDevice()
{
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

void VulkanRenderer::createSwapChain()
{
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain");
    }

    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void VulkanRenderer::createImageViews()
{
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); ++i) {
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void VulkanRenderer::createRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }
}

void VulkanRenderer::createShadowRenderPass()
{
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 0;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &depthAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &shadowRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow render pass");
    }
}

void VulkanRenderer::createReflectionRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &reflectionRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create reflection render pass");
    }
}

void VulkanRenderer::createReflectionResources()
{
    destroyReflectionResources();

    VkFormat depthFormat = findDepthFormat();
    createImage(swapChainExtent.width, swapChainExtent.height, swapChainImageFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                reflectionColorImage, reflectionColorMemory);
    reflectionColorView = createImageView(reflectionColorImage, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

    createImage(swapChainExtent.width, swapChainExtent.height, depthFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                reflectionDepthImage, reflectionDepthMemory);
    reflectionDepthView = createImageView(reflectionDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    std::array<VkImageView, 2> attachments = {reflectionColorView, reflectionDepthView};
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = reflectionRenderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = swapChainExtent.width;
    framebufferInfo.height = swapChainExtent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &reflectionFramebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create reflection framebuffer");
    }

    reflectionTexture.image = reflectionColorImage;
    reflectionTexture.memory = reflectionColorMemory;
    reflectionTexture.view = reflectionColorView;
    reflectionTexture.width = swapChainExtent.width;
    reflectionTexture.height = swapChainExtent.height;
    if (reflectionTexture.descriptorSet == VK_NULL_HANDLE) {
        reflectionTexture.descriptorSet = allocateMaterialDescriptor();
    }
    writeMaterialDescriptor(reflectionTexture.descriptorSet, reflectionTexture.view);
    reflectionImageReady = false;
}

void VulkanRenderer::destroyReflectionResources()
{
    if (reflectionFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, reflectionFramebuffer, nullptr);
        reflectionFramebuffer = VK_NULL_HANDLE;
    }
    if (reflectionColorView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, reflectionColorView, nullptr);
        reflectionColorView = VK_NULL_HANDLE;
    }
    if (reflectionDepthView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, reflectionDepthView, nullptr);
        reflectionDepthView = VK_NULL_HANDLE;
    }
    if (reflectionColorImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, reflectionColorImage, nullptr);
        reflectionColorImage = VK_NULL_HANDLE;
    }
    if (reflectionColorMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, reflectionColorMemory, nullptr);
        reflectionColorMemory = VK_NULL_HANDLE;
    }
    if (reflectionDepthImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, reflectionDepthImage, nullptr);
        reflectionDepthImage = VK_NULL_HANDLE;
    }
    if (reflectionDepthMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, reflectionDepthMemory, nullptr);
        reflectionDepthMemory = VK_NULL_HANDLE;
    }
    reflectionTexture.image = VK_NULL_HANDLE;
    reflectionTexture.memory = VK_NULL_HANDLE;
    reflectionTexture.view = VK_NULL_HANDLE;
    reflectionTexture.width = 0;
    reflectionTexture.height = 0;
    reflectionImageReady = false;
}

void VulkanRenderer::createPostProcessRenderPass()
{
    // Render pass for post-processing - renders to swapchain image
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // We'll overwrite everything
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &postProcessRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create post-process render pass");
    }
}

void VulkanRenderer::createPostProcessResources()
{
    destroyPostProcessResources();

    // Create offscreen color image (scene is rendered here first)
    createImage(swapChainExtent.width, swapChainExtent.height, swapChainImageFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                offscreenColorImage, offscreenColorMemory);
    offscreenColorView = createImageView(offscreenColorImage, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

    // Create offscreen depth image
    VkFormat depthFormat = findDepthFormat();
    createImage(swapChainExtent.width, swapChainExtent.height, depthFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                offscreenDepthImage, offscreenDepthMemory);
    offscreenDepthView = createImageView(offscreenDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    // Create ping-pong image for two-pass blur
    createImage(swapChainExtent.width, swapChainExtent.height, swapChainImageFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                pingPongImage, pingPongMemory);
    pingPongView = createImageView(pingPongImage, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

    // Create offscreen framebuffer
    std::array<VkImageView, 2> attachments = {offscreenColorView, offscreenDepthView};
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass; // Use main render pass
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = swapChainExtent.width;
    framebufferInfo.height = swapChainExtent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &offscreenFramebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create offscreen framebuffer");
    }

    // Create ping-pong framebuffer (color only, uses post-process render pass)
    VkFramebufferCreateInfo pingPongFbInfo{};
    pingPongFbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    pingPongFbInfo.renderPass = postProcessRenderPass;
    pingPongFbInfo.attachmentCount = 1;
    pingPongFbInfo.pAttachments = &pingPongView;
    pingPongFbInfo.width = swapChainExtent.width;
    pingPongFbInfo.height = swapChainExtent.height;
    pingPongFbInfo.layers = 1;

    if (vkCreateFramebuffer(device, &pingPongFbInfo, nullptr, &pingPongFramebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ping-pong framebuffer");
    }

    // Create swapchain framebuffers for final post-process pass (color only)
    postProcessSwapchainFramebuffers.resize(swapChainImageViews.size());
    for (size_t i = 0; i < swapChainImageViews.size(); ++i) {
        VkFramebufferCreateInfo ppFbInfo{};
        ppFbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ppFbInfo.renderPass = postProcessRenderPass;
        ppFbInfo.attachmentCount = 1;
        ppFbInfo.pAttachments = &swapChainImageViews[i];
        ppFbInfo.width = swapChainExtent.width;
        ppFbInfo.height = swapChainExtent.height;
        ppFbInfo.layers = 1;

        if (vkCreateFramebuffer(device, &ppFbInfo, nullptr, &postProcessSwapchainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create post-process swapchain framebuffer");
        }
    }

    // Create sampler for post-process textures
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &postProcessSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create post-process sampler");
    }

    // Create descriptor set layout for post-process
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &postProcessDescriptorLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create post-process descriptor set layout");
    }

    // Create descriptor pool for post-process
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 2; // One for offscreen, one for ping-pong

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 2;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &postProcessDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create post-process descriptor pool");
    }

    // Allocate descriptor sets
    std::array<VkDescriptorSetLayout, 2> layouts = {postProcessDescriptorLayout, postProcessDescriptorLayout};
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = postProcessDescriptorPool;
    allocInfo.descriptorSetCount = 2;
    allocInfo.pSetLayouts = layouts.data();

    std::array<VkDescriptorSet, 2> descriptorSetsArray;
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSetsArray.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate post-process descriptor sets");
    }
    postProcessDescriptorSet = descriptorSetsArray[0];
    pingPongDescriptorSet = descriptorSetsArray[1];

    // Update descriptor sets
    VkDescriptorImageInfo offscreenImageInfo{};
    offscreenImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    offscreenImageInfo.imageView = offscreenColorView;
    offscreenImageInfo.sampler = postProcessSampler;

    VkDescriptorImageInfo pingPongImageInfo{};
    pingPongImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    pingPongImageInfo.imageView = pingPongView;
    pingPongImageInfo.sampler = postProcessSampler;

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = postProcessDescriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &offscreenImageInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = pingPongDescriptorSet;
    descriptorWrites[1].dstBinding = 0;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &pingPongImageInfo;

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

    // Create post-process pipelines now that descriptor layout exists
    createPostProcessPipelines();
}

void VulkanRenderer::destroyPostProcessResources()
{
    if (postProcessDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, postProcessDescriptorPool, nullptr);
        postProcessDescriptorPool = VK_NULL_HANDLE;
        postProcessDescriptorSet = VK_NULL_HANDLE;
        pingPongDescriptorSet = VK_NULL_HANDLE;
    }
    if (postProcessDescriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, postProcessDescriptorLayout, nullptr);
        postProcessDescriptorLayout = VK_NULL_HANDLE;
    }
    if (postProcessSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, postProcessSampler, nullptr);
        postProcessSampler = VK_NULL_HANDLE;
    }
    if (offscreenFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, offscreenFramebuffer, nullptr);
        offscreenFramebuffer = VK_NULL_HANDLE;
    }
    if (pingPongFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, pingPongFramebuffer, nullptr);
        pingPongFramebuffer = VK_NULL_HANDLE;
    }
    for (auto fb : postProcessSwapchainFramebuffers) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, fb, nullptr);
        }
    }
    postProcessSwapchainFramebuffers.clear();
    if (offscreenColorView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, offscreenColorView, nullptr);
        offscreenColorView = VK_NULL_HANDLE;
    }
    if (offscreenColorImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, offscreenColorImage, nullptr);
        offscreenColorImage = VK_NULL_HANDLE;
    }
    if (offscreenColorMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, offscreenColorMemory, nullptr);
        offscreenColorMemory = VK_NULL_HANDLE;
    }
    if (offscreenDepthView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, offscreenDepthView, nullptr);
        offscreenDepthView = VK_NULL_HANDLE;
    }
    if (offscreenDepthImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, offscreenDepthImage, nullptr);
        offscreenDepthImage = VK_NULL_HANDLE;
    }
    if (offscreenDepthMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, offscreenDepthMemory, nullptr);
        offscreenDepthMemory = VK_NULL_HANDLE;
    }
    if (pingPongView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, pingPongView, nullptr);
        pingPongView = VK_NULL_HANDLE;
    }
    if (pingPongImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, pingPongImage, nullptr);
        pingPongImage = VK_NULL_HANDLE;
    }
    if (pingPongMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, pingPongMemory, nullptr);
        pingPongMemory = VK_NULL_HANDLE;
    }
    // Note: blurVerticalPipeline and blitPipeline point to the same handle as blurHorizontalPipeline
    // so we only destroy blurHorizontalPipeline and null all three
    if (blurHorizontalPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, blurHorizontalPipeline, nullptr);
        blurHorizontalPipeline = VK_NULL_HANDLE;
        blurVerticalPipeline = VK_NULL_HANDLE;
        blitPipeline = VK_NULL_HANDLE;
    }
    if (postProcessPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, postProcessPipelineLayout, nullptr);
        postProcessPipelineLayout = VK_NULL_HANDLE;
    }
    // Note: postProcessRenderPass is NOT destroyed here because it's managed by
    // createPostProcessRenderPass() and destroyed in cleanup(). This function
    // is called during recreation (e.g., window resize) where we want to keep the render pass.
}

namespace {
std::vector<char> readShaderFile(const std::filesystem::path& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file " + filename.string());
    }
    const size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    return buffer;
}
} // anonymous namespace

void VulkanRenderer::createPostProcessPipelines()
{
    // Push constants for blur parameters
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(float) * 8; // texelSize(2) + direction(2) + radius + sigma + pad(2)

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &postProcessDescriptorLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &postProcessPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create post-process pipeline layout");
    }

    // Load shaders
    std::filesystem::path shaderPath = SHADER_BINARY_DIR;
    std::filesystem::path postShaderPath = shaderPath / "post";
    
    // Try post subdirectory first, fall back to main shader directory
    std::filesystem::path vertPath = postShaderPath / "fullscreen.vert.spv";
    std::filesystem::path fragPath = postShaderPath / "blur.frag.spv";
    if (!std::filesystem::exists(vertPath)) {
        vertPath = shaderPath / "fullscreen.vert.spv";
    }
    if (!std::filesystem::exists(fragPath)) {
        fragPath = shaderPath / "blur.frag.spv";
    }
    
    auto vertShaderCode = readShaderFile(vertPath);
    auto blurShaderCode = readShaderFile(fragPath);

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule blurShaderModule = createShaderModule(blurShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = blurShaderModule;
    fragShaderStageInfo.pName = "main";

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {vertShaderStageInfo, fragShaderStageInfo};

    // No vertex input for fullscreen triangle
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = postProcessPipelineLayout;
    pipelineInfo.renderPass = postProcessRenderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &blurHorizontalPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create blur pipeline");
    }

    // Use same pipeline for vertical pass (direction is passed via push constants)
    blurVerticalPipeline = blurHorizontalPipeline;
    blitPipeline = blurHorizontalPipeline;

    vkDestroyShaderModule(device, blurShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

void VulkanRenderer::createDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding shadowSamplerBinding{};
    shadowSamplerBinding.binding = 1;
    shadowSamplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    shadowSamplerBinding.descriptorCount = 1;
    shadowSamplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    shadowSamplerBinding.pImmutableSamplers = nullptr;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, shadowSamplerBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }

    VkDescriptorSetLayoutBinding materialTextureBinding{};
    materialTextureBinding.binding = 0;
    materialTextureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    materialTextureBinding.descriptorCount = 1;
    materialTextureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    materialTextureBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo materialLayoutInfo{};
    materialLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    materialLayoutInfo.bindingCount = 1;
    materialLayoutInfo.pBindings = &materialTextureBinding;

    if (vkCreateDescriptorSetLayout(device, &materialLayoutInfo, nullptr, &materialDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create material descriptor set layout");
    }
}

void VulkanRenderer::createPipelines()
{
    vkcore::CubePipelineInputs inputs{};
    inputs.device = device;
    inputs.swapChainExtent = swapChainExtent;
    inputs.renderPass = renderPass;
    inputs.shadowRenderPass = shadowRenderPass;
    inputs.descriptorSetLayout = descriptorSetLayout;
    inputs.materialDescriptorSetLayout = materialDescriptorSetLayout;
    inputs.shaderDirectory = SHADER_BINARY_DIR;

    auto pipelines = vkcore::createCubePipelines(inputs);
    pipelineLayout = pipelines.pipelineLayout;
    stylePipelines.assign(pipelines.stylePipelines.begin(), pipelines.stylePipelines.end());
    graphicsPipeline = pipelines.graphicsPipeline;
    linePipeline = pipelines.linePipeline;
    shadowPipeline = pipelines.shadowPipeline;
    particlePipeline = pipelines.particlePipeline;
}

void VulkanRenderer::createDepthResources()
{
    VkFormat depthFormat = findDepthFormat();

    createImage(swapChainExtent.width, swapChainExtent.height, depthFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                depthImage, depthImageMemory);

    depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanRenderer::createShadowResources()
{
    destroyShadowResources();

    VkFormat depthFormat = findDepthFormat();

    createImage(SHADOW_MAP_DIM, SHADOW_MAP_DIM, depthFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                shadowImage, shadowImageMemory);

    shadowImageView = createImageView(shadowImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = shadowRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &shadowImageView;
    framebufferInfo.width = SHADOW_MAP_DIM;
    framebufferInfo.height = SHADOW_MAP_DIM;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &shadowFramebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow framebuffer");
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &shadowSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow sampler");
    }

    shadowImageReady = false;
}

void VulkanRenderer::destroyShadowResources()
{
    if (shadowSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, shadowSampler, nullptr);
        shadowSampler = VK_NULL_HANDLE;
    }
    if (shadowFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, shadowFramebuffer, nullptr);
        shadowFramebuffer = VK_NULL_HANDLE;
    }
    if (shadowImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, shadowImageView, nullptr);
        shadowImageView = VK_NULL_HANDLE;
    }
    if (shadowImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, shadowImage, nullptr);
        shadowImage = VK_NULL_HANDLE;
    }
    if (shadowImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, shadowImageMemory, nullptr);
        shadowImageMemory = VK_NULL_HANDLE;
    }
    shadowImageReady = false;
}

void VulkanRenderer::createFramebuffers()
{
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); ++i) {
        std::array<VkImageView, 2> attachments = {swapChainImageViews[i], depthImageView};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

void VulkanRenderer::createCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
}

void VulkanRenderer::createVertexBuffer()
{
    VkDeviceSize bufferSize = sizeof(VERTICES[0]) * VERTICES.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data = nullptr;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    std::memcpy(data, VERTICES.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 vertexBuffer, vertexBufferMemory);

    copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanRenderer::createLineVertexBuffer()
{
    VkDeviceSize bufferSize = sizeof(LINE_VERTICES[0]) * LINE_VERTICES.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data = nullptr;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    std::memcpy(data, LINE_VERTICES.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 lineVertexBuffer, lineVertexBufferMemory);

    copyBuffer(stagingBuffer, lineVertexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanRenderer::createIndexBuffer()
{
    VkDeviceSize bufferSize = sizeof(INDICES[0]) * INDICES.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data = nullptr;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    std::memcpy(data, INDICES.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 indexBuffer, indexBufferMemory);

    copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanRenderer::ensureDeformableBuffers(VkDeviceSize vertexBufferSize, VkDeviceSize indexBufferSize)
{
    if (vertexBufferSize == 0 || indexBufferSize == 0) {
        return;
    }

    if (vertexBufferSize > deformableVertexCapacity) {
        if (deformableVertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, deformableVertexBuffer, nullptr);
        }
        if (deformableVertexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, deformableVertexMemory, nullptr);
        }
        createBuffer(vertexBufferSize,
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     deformableVertexBuffer, deformableVertexMemory);
        deformableVertexCapacity = vertexBufferSize;
    }

    if (indexBufferSize > deformableIndexCapacity) {
        if (deformableIndexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, deformableIndexBuffer, nullptr);
        }
        if (deformableIndexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, deformableIndexMemory, nullptr);
        }
        createBuffer(indexBufferSize,
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     deformableIndexBuffer, deformableIndexMemory);
        deformableIndexCapacity = indexBufferSize;
    }
}

void VulkanRenderer::createUniformBuffers()
{
    VkDeviceSize bufferSize = sizeof(CameraBufferObject);

    uniformBuffers.resize(swapChainImages.size());
    uniformBuffersMemory.resize(swapChainImages.size());
    uniformBuffersMapped.resize(swapChainImages.size());
    reflectionUniformBuffers.resize(swapChainImages.size());
    reflectionUniformBuffersMemory.resize(swapChainImages.size());
    reflectionUniformBuffersMapped.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); ++i) {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     uniformBuffers[i], uniformBuffersMemory[i]);
        vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);

        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     reflectionUniformBuffers[i], reflectionUniformBuffersMemory[i]);
        vkMapMemory(device, reflectionUniformBuffersMemory[i], 0, bufferSize, 0, &reflectionUniformBuffersMapped[i]);
    }
}

void VulkanRenderer::createDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(swapChainImages.size() * 2);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(swapChainImages.size() * 2);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(swapChainImages.size() * 2);

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }
}

void VulkanRenderer::createDescriptorSets()
{
    std::vector<VkDescriptorSetLayout> layouts(swapChainImages.size(), descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(swapChainImages.size());
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets");
    }

    reflectionDescriptorSets.resize(swapChainImages.size());
    if (vkAllocateDescriptorSets(device, &allocInfo, reflectionDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate reflection descriptor sets");
    }

    for (size_t i = 0; i < swapChainImages.size(); ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(CameraBufferObject);

        VkDescriptorBufferInfo reflectionBufferInfo{};
        reflectionBufferInfo.buffer = reflectionUniformBuffers[i];
        reflectionBufferInfo.offset = 0;
        reflectionBufferInfo.range = sizeof(CameraBufferObject);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = shadowImageView;
        imageInfo.sampler = shadowSampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        VkWriteDescriptorSet reflectionWrite{};
        reflectionWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        reflectionWrite.dstSet = reflectionDescriptorSets[i];
        reflectionWrite.dstBinding = 0;
        reflectionWrite.dstArrayElement = 0;
        reflectionWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        reflectionWrite.descriptorCount = 1;
        reflectionWrite.pBufferInfo = &reflectionBufferInfo;

        VkWriteDescriptorSet samplerWrite{};
        samplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        samplerWrite.dstSet = descriptorSets[i];
        samplerWrite.dstBinding = 1;
        samplerWrite.dstArrayElement = 0;
        samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerWrite.descriptorCount = 1;
        samplerWrite.pImageInfo = &imageInfo;

        VkWriteDescriptorSet reflectionSamplerWrite{};
        reflectionSamplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        reflectionSamplerWrite.dstSet = reflectionDescriptorSets[i];
        reflectionSamplerWrite.dstBinding = 1;
        reflectionSamplerWrite.dstArrayElement = 0;
        reflectionSamplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        reflectionSamplerWrite.descriptorCount = 1;
        reflectionSamplerWrite.pImageInfo = &imageInfo;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites = {descriptorWrite, samplerWrite};
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

        std::array<VkWriteDescriptorSet, 2> reflectionWrites = {reflectionWrite, reflectionSamplerWrite};
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(reflectionWrites.size()), reflectionWrites.data(), 0, nullptr);
    }
}

void VulkanRenderer::createMaterialDescriptorPool()
{
    if (materialDescriptorPool != VK_NULL_HANDLE) {
        return;
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1024;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1024;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &materialDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create material descriptor pool");
    }
}

void VulkanRenderer::createMaterialResources()
{
    if (materialSampler == VK_NULL_HANDLE) {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &materialSampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture sampler");
        }
    }

    if (defaultTexture.image == VK_NULL_HANDLE) {
        createDefaultTexture();
    }
}

void VulkanRenderer::destroyTextureResource(TextureResource& texture)
{
    if (texture.descriptorSet != VK_NULL_HANDLE && materialDescriptorPool != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(device, materialDescriptorPool, 1, &texture.descriptorSet);
        texture.descriptorSet = VK_NULL_HANDLE;
    }
    if (texture.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, texture.view, nullptr);
        texture.view = VK_NULL_HANDLE;
    }
    if (texture.image != VK_NULL_HANDLE) {
        vkDestroyImage(device, texture.image, nullptr);
        texture.image = VK_NULL_HANDLE;
    }
    if (texture.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, texture.memory, nullptr);
        texture.memory = VK_NULL_HANDLE;
    }
    texture.width = 0;
    texture.height = 0;
}

void VulkanRenderer::destroyTextureCache()
{
    for (auto& [_, resource] : textureCache) {
        destroyTextureResource(resource);
    }
    textureCache.clear();
}

void VulkanRenderer::destroyColorTextureCache()
{
    for (auto& [_, resource] : colorTextureCache) {
        destroyTextureResource(resource);
    }
    colorTextureCache.clear();
}

void VulkanRenderer::destroyMeshCache()
{
    for (auto& [_, buffers] : meshCache) {
        if (buffers.vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, buffers.vertexBuffer, nullptr);
            buffers.vertexBuffer = VK_NULL_HANDLE;
        }
        if (buffers.vertexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, buffers.vertexMemory, nullptr);
            buffers.vertexMemory = VK_NULL_HANDLE;
        }
        if (buffers.indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, buffers.indexBuffer, nullptr);
            buffers.indexBuffer = VK_NULL_HANDLE;
        }
        if (buffers.indexMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, buffers.indexMemory, nullptr);
            buffers.indexMemory = VK_NULL_HANDLE;
        }
    }
    meshCache.clear();
}

void VulkanRenderer::destroyMaterialResources()
{
    destroyTextureCache();
    destroyColorTextureCache();
    if (defaultTexture.image != VK_NULL_HANDLE) {
        destroyTextureResource(defaultTexture);
    }
    if (materialDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, materialDescriptorPool, nullptr);
        materialDescriptorPool = VK_NULL_HANDLE;
    }
    if (materialSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, materialSampler, nullptr);
        materialSampler = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::createDefaultTexture()
{
    vkengine::TextureData data{};
    data.width = 1;
    data.height = 1;
    data.channels = 4;
    data.pixels = {255, 255, 255, 255};
    uploadTextureToGpu("__default__", data, defaultTexture);
}

VkDescriptorSet VulkanRenderer::allocateMaterialDescriptor()
{
    if (materialDescriptorPool == VK_NULL_HANDLE) {
        throw std::runtime_error("Material descriptor pool not initialized");
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = materialDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &materialDescriptorSetLayout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate material descriptor set");
    }
    return descriptorSet;
}

void VulkanRenderer::writeMaterialDescriptor(VkDescriptorSet descriptorSet, VkImageView imageView)
{
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = materialSampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
}

VulkanRenderer::MeshGpuBuffers& VulkanRenderer::getOrCreateMesh(const vkengine::RenderComponent& renderComponent)
{
    if (renderComponent.meshResource.empty()) {
        throw std::runtime_error("RenderComponent missing mesh resource path for custom mesh");
    }

    const auto meshPath = resolveAssetPath(renderComponent.meshResource);
    const std::string cacheKey = makeMeshCacheKey(meshPath.string());
    auto it = meshCache.find(cacheKey);
    if (it != meshCache.end()) {
        return it->second;
    }

    const auto meshData = vkengine::loadStlMesh(meshPath);
    uploadMeshToGpu(cacheKey, meshData);
    return meshCache.at(cacheKey);
}

VulkanRenderer::TextureResource& VulkanRenderer::getOrCreateTexture(const vkengine::RenderComponent& renderComponent)
{
    if (renderComponent.albedoTexture == "__reflection__" && reflectionTexture.view != VK_NULL_HANDLE) {
        return reflectionTexture;
    }
    if (renderComponent.albedoTexture == "__default__") {
        return defaultTexture;
    }
    if (shouldUseColorTexture(renderComponent)) {
        return getOrCreateColorTexture(renderComponent.baseColor);
    }
    if (renderComponent.albedoTexture.empty()) {
        return defaultTexture;
    }

    const auto texturePath = resolveAssetPath(renderComponent.albedoTexture);
    const std::string cacheKey = texturePath.string();
    auto it = textureCache.find(cacheKey);
    if (it != textureCache.end()) {
        return it->second;
    }
    try {
        const auto textureData = vkengine::loadTexture(texturePath, true);
        TextureResource& texture = textureCache[cacheKey];
        uploadTextureToGpu(cacheKey, textureData, texture);
        return texture;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load texture '" << texturePath << "': " << e.what() << "\n";
        return defaultTexture;
    }
}

bool VulkanRenderer::shouldUseColorTexture(const vkengine::RenderComponent& renderComponent) const
{
    if (!renderComponent.albedoTexture.empty()) {
        return false;
    }
    const glm::vec3 base = glm::clamp(glm::vec3(renderComponent.baseColor), glm::vec3(0.0f), glm::vec3(1.0f));
    const float maxDelta = std::max(std::abs(base.r - 1.0f), std::max(std::abs(base.g - 1.0f), std::abs(base.b - 1.0f)));
    return maxDelta > 0.02f;
}

VulkanRenderer::TextureResource& VulkanRenderer::getOrCreateColorTexture(const glm::vec4& color)
{
    const glm::vec3 clamped = glm::clamp(glm::vec3(color), glm::vec3(0.0f), glm::vec3(1.0f));
    const auto toByte = [](float value) {
        return static_cast<std::uint8_t>(std::round(glm::clamp(value, 0.0f, 1.0f) * 255.0f));
    };
    const std::uint32_t r = toByte(clamped.r);
    const std::uint32_t g = toByte(clamped.g);
    const std::uint32_t b = toByte(clamped.b);
    const std::uint32_t key = (r << 16) | (g << 8) | b;

    auto it = colorTextureCache.find(key);
    if (it != colorTextureCache.end()) {
        return it->second;
    }

    vkengine::TextureData data{};
    data.width = 1;
    data.height = 1;
    data.channels = 4;
    data.pixels = {static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b), 255};

    TextureResource& texture = colorTextureCache[key];
    uploadTextureToGpu("__color__", data, texture);
    return texture;
}

void VulkanRenderer::uploadMeshToGpu(const std::string& cacheKey, const vkengine::MeshData& meshData)
{
    auto& buffers = meshCache[cacheKey];

    if (buffers.vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffers.vertexBuffer, nullptr);
    }
    if (buffers.vertexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, buffers.vertexMemory, nullptr);
    }
    if (buffers.indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffers.indexBuffer, nullptr);
    }
    if (buffers.indexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, buffers.indexMemory, nullptr);
    }

    if (meshData.vertices.empty() || meshData.indices.empty()) {
        buffers.indexCount = 0;
        return;
    }

    std::vector<Vertex> gpuVertices;
    gpuVertices.reserve(meshData.vertices.size());
    for (const auto& vertex : meshData.vertices) {
        Vertex v{};
        v.pos = vertex.position;
        v.color = glm::vec4(1.0f);
        v.normal = vertex.normal;
        v.uv = vertex.uv;
        gpuVertices.push_back(v);
    }

    const VkDeviceSize vertexBufferSize = sizeof(Vertex) * gpuVertices.size();
    const VkDeviceSize indexBufferSize = sizeof(uint32_t) * meshData.indices.size();

    VkBuffer vertexStaging = VK_NULL_HANDLE;
    VkDeviceMemory vertexStagingMemory = VK_NULL_HANDLE;
    createBuffer(vertexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 vertexStaging, vertexStagingMemory);

    void* mapped = nullptr;
    vkMapMemory(device, vertexStagingMemory, 0, vertexBufferSize, 0, &mapped);
    std::memcpy(mapped, gpuVertices.data(), static_cast<size_t>(vertexBufferSize));
    vkUnmapMemory(device, vertexStagingMemory);

    createBuffer(vertexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 buffers.vertexBuffer, buffers.vertexMemory);

    copyBuffer(vertexStaging, buffers.vertexBuffer, vertexBufferSize);
    vkDestroyBuffer(device, vertexStaging, nullptr);
    vkFreeMemory(device, vertexStagingMemory, nullptr);

    VkBuffer indexStaging = VK_NULL_HANDLE;
    VkDeviceMemory indexStagingMemory = VK_NULL_HANDLE;
    createBuffer(indexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 indexStaging, indexStagingMemory);

    vkMapMemory(device, indexStagingMemory, 0, indexBufferSize, 0, &mapped);
    std::memcpy(mapped, meshData.indices.data(), static_cast<size_t>(indexBufferSize));
    vkUnmapMemory(device, indexStagingMemory);

    createBuffer(indexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 buffers.indexBuffer, buffers.indexMemory);

    copyBuffer(indexStaging, buffers.indexBuffer, indexBufferSize);
    vkDestroyBuffer(device, indexStaging, nullptr);
    vkFreeMemory(device, indexStagingMemory, nullptr);

    buffers.indexType = VK_INDEX_TYPE_UINT32;
    buffers.indexCount = static_cast<uint32_t>(meshData.indices.size());
}

void VulkanRenderer::uploadTextureToGpu(const std::string& cacheKey, const vkengine::TextureData& textureData, TextureResource& outTexture)
{
    (void)cacheKey;
    if (textureData.pixels.empty()) {
        throw std::runtime_error("Texture has no pixel data");
    }

    if (outTexture.image != VK_NULL_HANDLE) {
        destroyTextureResource(outTexture);
    }

    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(textureData.pixels.size());
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    createBuffer(imageSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingMemory);

    void* mapped = nullptr;
    vkMapMemory(device, stagingMemory, 0, imageSize, 0, &mapped);
    std::memcpy(mapped, textureData.pixels.data(), static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingMemory);

    createImage(textureData.width, textureData.height, VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                outTexture.image, outTexture.memory);

    transitionImageLayout(outTexture.image, VK_FORMAT_R8G8B8A8_UNORM,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, outTexture.image, textureData.width, textureData.height);
    transitionImageLayout(outTexture.image, VK_FORMAT_R8G8B8A8_UNORM,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    outTexture.view = createImageView(outTexture.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
    outTexture.width = textureData.width;
    outTexture.height = textureData.height;

    if (outTexture.descriptorSet == VK_NULL_HANDLE) {
        outTexture.descriptorSet = allocateMaterialDescriptor();
    }
    writeMaterialDescriptor(outTexture.descriptorSet, outTexture.view);
}

std::filesystem::path VulkanRenderer::resolveAssetPath(const std::string& relativePath) const
{
    if (relativePath.empty()) {
        return {};
    }

    std::filesystem::path requested(relativePath);
    if (requested.is_absolute()) {
        return requested;
    }

    std::vector<std::filesystem::path> candidates;
    if (!projectRootPath.empty()) {
        candidates.push_back(projectRootPath);
        candidates.push_back(projectRootPath / "..");
    }
    try {
        auto cwd = std::filesystem::current_path();
        candidates.push_back(cwd);
        candidates.push_back(cwd / "..");
    } catch (const std::exception&) {
    }

    for (const auto& base : candidates) {
        std::error_code ec;
        const auto candidate = std::filesystem::weakly_canonical(base / requested, ec);
        if (!ec && std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return projectRootPath.empty() ? requested : (projectRootPath / requested);
}

void VulkanRenderer::createCommandBuffers()
{
    commandBuffers.resize(swapChainFramebuffers.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }
}

void VulkanRenderer::createSyncObjects()
{
    framesInFlight = std::clamp(framesInFlight, MIN_FRAMES_IN_FLIGHT, MAX_FRAMES_IN_FLIGHT);
    imageAvailableSemaphores.resize(framesInFlight);
    inFlightFences.resize(framesInFlight);
    imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < framesInFlight; ++i) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects");
        }
    }

    allocateRenderFinishedSemaphores();
}

void VulkanRenderer::allocateRenderFinishedSemaphores()
{
    destroyRenderFinishedSemaphores();

    if (swapChainImages.empty()) {
        return;
    }

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    renderFinishedSemaphores.resize(swapChainImages.size());
    for (auto& semaphore : renderFinishedSemaphores) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render-finished semaphore");
        }
    }
}

void VulkanRenderer::destroyRenderFinishedSemaphores()
{
    for (auto semaphore : renderFinishedSemaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, semaphore, nullptr);
        }
    }
    renderFinishedSemaphores.clear();
}

void VulkanRenderer::destroyDeformableBuffers()
{
    if (deformableVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, deformableVertexBuffer, nullptr);
        deformableVertexBuffer = VK_NULL_HANDLE;
    }
    if (deformableVertexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, deformableVertexMemory, nullptr);
        deformableVertexMemory = VK_NULL_HANDLE;
    }
    if (deformableIndexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, deformableIndexBuffer, nullptr);
        deformableIndexBuffer = VK_NULL_HANDLE;
    }
    if (deformableIndexMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, deformableIndexMemory, nullptr);
        deformableIndexMemory = VK_NULL_HANDLE;
    }
    deformableVertexCapacity = 0;
    deformableIndexCapacity = 0;
    deformableIndexCount = 0;
    deformableDraws.clear();
}

void VulkanRenderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, float /*deltaSeconds*/)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer");
    }

    auto buildObjectPushConstants = [&](const glm::mat4& modelMatrix, const vkengine::RenderComponent& render) {
        vkcore::ObjectPushConstants push{};
        push.model = modelMatrix;
        if (shouldUseColorTexture(render)) {
            push.baseColor = glm::vec4(1.0f, 1.0f, 1.0f, render.baseColor.a);
        } else {
            push.baseColor = render.baseColor;
        }
        push.materialParams = glm::vec4(render.metallic, render.roughness, render.specular, render.opacity);
        push.emissiveParams = glm::vec4(render.emissive, render.emissiveIntensity);
        const bool isMirror = render.albedoTexture == "__reflection__";
        push.mirrorParams = glm::vec4(isMirror ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
        return push;
    };

    const auto& scene = engine->scene();
    const auto& camera = scene.camera();
    const glm::mat4 mainView = camera.viewMatrix();
    const auto height = std::max<uint32_t>(1u, swapChainExtent.height);
    const float aspect = static_cast<float>(swapChainExtent.width) / static_cast<float>(height);
    const glm::mat4 mainProj = camera.projectionMatrix(aspect);
    const glm::vec3 mainCameraPosition = camera.getPosition();
    const size_t styleIndex = static_cast<size_t>(shaderStyle);

    // Transition shadow map to depth attachment layout for rendering
    VkImageMemoryBarrier toDepthBarrier{};
    toDepthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    const VkImageLayout previousShadowLayout = shadowImageReady
                                                   ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                                   : VK_IMAGE_LAYOUT_UNDEFINED;
    toDepthBarrier.oldLayout = previousShadowLayout;
    toDepthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    toDepthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toDepthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toDepthBarrier.image = shadowImage;
    toDepthBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    toDepthBarrier.subresourceRange.baseMipLevel = 0;
    toDepthBarrier.subresourceRange.levelCount = 1;
    toDepthBarrier.subresourceRange.baseArrayLayer = 0;
    toDepthBarrier.subresourceRange.layerCount = 1;
    toDepthBarrier.srcAccessMask = shadowImageReady ? VK_ACCESS_SHADER_READ_BIT : 0;
    toDepthBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkPipelineStageFlags shadowSrcStage = shadowImageReady
                                              ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                              : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    vkCmdPipelineBarrier(commandBuffer,
                         shadowSrcStage,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &toDepthBarrier);

    VkRenderPassBeginInfo shadowPassInfo{};
    shadowPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    shadowPassInfo.renderPass = shadowRenderPass;
    shadowPassInfo.framebuffer = shadowFramebuffer;
    shadowPassInfo.renderArea.offset = {0, 0};
    shadowPassInfo.renderArea.extent = {SHADOW_MAP_DIM, SHADOW_MAP_DIM};

    VkClearValue shadowClear{};
    shadowClear.depthStencil = {1.0f, 0};
    shadowPassInfo.clearValueCount = 1;
    shadowPassInfo.pClearValues = &shadowClear;

    vkCmdBeginRenderPass(commandBuffer, &shadowPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkBuffer cubeVertexBuffers[] = {vertexBuffer};
    VkDeviceSize cubeOffsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, cubeVertexBuffers, cubeOffsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                            0, 1, &descriptorSets[imageIndex], 0, nullptr);
    if (defaultTexture.descriptorSet != VK_NULL_HANDLE) {
        VkDescriptorSet materialSet = defaultTexture.descriptorSet;
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                                1, 1, &materialSet, 0, nullptr);
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline);

    const auto& objects = engine->scene().objectsCached();
    for (const auto* object : objects) {
        const auto& render = object->render();
        if (!render.visible) {
            continue;
        }

        switch (object->mesh()) {
        case vkengine::MeshType::Cube: {
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, cubeVertexBuffers, cubeOffsets);
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
            const glm::mat4 modelMatrix = object->modelMatrix();
            const auto push = buildObjectPushConstants(modelMatrix, render);
            vkCmdPushConstants(commandBuffer, pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(push), &push);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(INDICES.size()), 1, 0, 0, 0);
            break;
        }
        case vkengine::MeshType::WireCubeLines:
            break;
        case vkengine::MeshType::CustomMesh: {
            if (render.meshResource.empty()) {
                continue;
            }
            auto& meshBuffers = getOrCreateMesh(render);
            if (meshBuffers.indexCount == 0) {
                continue;
            }
            VkBuffer meshVertexBuffers[] = {meshBuffers.vertexBuffer};
            VkDeviceSize meshOffsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, meshVertexBuffers, meshOffsets);
            vkCmdBindIndexBuffer(commandBuffer, meshBuffers.indexBuffer, 0, meshBuffers.indexType);
            const glm::mat4 modelMatrix = object->modelMatrix();
            const auto push = buildObjectPushConstants(modelMatrix, render);
            vkCmdPushConstants(commandBuffer, pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(push), &push);
            vkCmdDrawIndexed(commandBuffer, meshBuffers.indexCount, 1, 0, 0, 0);
            break;
        }
        default:
            break;
        }
    }

    vkCmdBindVertexBuffers(commandBuffer, 0, 1, cubeVertexBuffers, cubeOffsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    if (deformableIndexCount > 0 &&
        deformableVertexBuffer != VK_NULL_HANDLE &&
        deformableIndexBuffer != VK_NULL_HANDLE) {
        VkBuffer deformableBuffers[] = {deformableVertexBuffer};
        VkDeviceSize deformableOffsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, deformableBuffers, deformableOffsets);
        vkCmdBindIndexBuffer(commandBuffer, deformableIndexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                                0, 1, &descriptorSets[imageIndex], 0, nullptr);

        vkengine::RenderComponent defaultRender{};

        for (const auto& draw : deformableDraws) {
            const auto push = buildObjectPushConstants(draw.model, defaultRender);
            vkCmdPushConstants(commandBuffer, pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(push), &push);
            vkCmdDrawIndexed(commandBuffer, draw.indexCount, 1, draw.indexOffset,
                             static_cast<int32_t>(draw.vertexOffset), 0);
        }

        vkCmdBindVertexBuffers(commandBuffer, 0, 1, cubeVertexBuffers, cubeOffsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    }

    vkCmdEndRenderPass(commandBuffer);

    // Transition shadow map for sampling in the lighting pass
    VkImageMemoryBarrier toSampleBarrier{};
    toSampleBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toSampleBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toSampleBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toSampleBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSampleBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSampleBarrier.image = shadowImage;
    toSampleBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    toSampleBarrier.subresourceRange.baseMipLevel = 0;
    toSampleBarrier.subresourceRange.levelCount = 1;
    toSampleBarrier.subresourceRange.baseArrayLayer = 0;
    toSampleBarrier.subresourceRange.layerCount = 1;
    toSampleBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    toSampleBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &toSampleBarrier);

    shadowImageReady = true;

    if (mirrorAvailable && reflectionRenderPass != VK_NULL_HANDLE && reflectionFramebuffer != VK_NULL_HANDLE) {
        VkImageMemoryBarrier reflectionBarrier{};
        reflectionBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        reflectionBarrier.oldLayout = reflectionImageReady ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                                           : VK_IMAGE_LAYOUT_UNDEFINED;
        reflectionBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        reflectionBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        reflectionBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        reflectionBarrier.image = reflectionColorImage;
        reflectionBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        reflectionBarrier.subresourceRange.baseMipLevel = 0;
        reflectionBarrier.subresourceRange.levelCount = 1;
        reflectionBarrier.subresourceRange.baseArrayLayer = 0;
        reflectionBarrier.subresourceRange.layerCount = 1;
        reflectionBarrier.srcAccessMask = reflectionImageReady ? VK_ACCESS_SHADER_READ_BIT : 0;
        reflectionBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkPipelineStageFlags reflectionSrcStage = reflectionImageReady
                                                      ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                                      : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        vkCmdPipelineBarrier(commandBuffer,
                             reflectionSrcStage,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &reflectionBarrier);

        VkRenderPassBeginInfo reflectionPassInfo{};
        reflectionPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        reflectionPassInfo.renderPass = reflectionRenderPass;
        reflectionPassInfo.framebuffer = reflectionFramebuffer;
        reflectionPassInfo.renderArea.offset = {0, 0};
        reflectionPassInfo.renderArea.extent = swapChainExtent;

        std::array<VkClearValue, 2> reflectionClears{};
        reflectionClears[0].color = {{skySettings.color.r, skySettings.color.g, skySettings.color.b, skySettings.color.a}};
        reflectionClears[1].depthStencil = {1.0f, 0};
        reflectionPassInfo.clearValueCount = static_cast<uint32_t>(reflectionClears.size());
        reflectionPassInfo.pClearValues = reflectionClears.data();

        vkCmdBeginRenderPass(commandBuffer, &reflectionPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkPipeline reflectionPipeline = graphicsPipeline;
        if (styleIndex < stylePipelines.size() && stylePipelines[styleIndex] != VK_NULL_HANDLE) {
            reflectionPipeline = stylePipelines[styleIndex];
        }
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, reflectionPipeline);
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, cubeVertexBuffers, cubeOffsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                                0, 1, &reflectionDescriptorSets[imageIndex], 0, nullptr);
        VkDescriptorSet reflectionBoundSet = VK_NULL_HANDLE;
        auto bindReflectionMaterial = [&](VkDescriptorSet set) {
            VkDescriptorSet target = set != VK_NULL_HANDLE ? set : defaultTexture.descriptorSet;
            if (target == VK_NULL_HANDLE || target == reflectionBoundSet) {
                return;
            }
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                                    1, 1, &target, 0, nullptr);
            reflectionBoundSet = target;
        };
        bindReflectionMaterial(defaultTexture.descriptorSet);

        for (const auto* object : scene.objectsCached()) {
            const auto& render = object->render();
            if (!render.visible || render.albedoTexture == "__reflection__") {
                continue;
            }
            switch (object->mesh()) {
            case vkengine::MeshType::Cube: {
                const auto& texture = getOrCreateTexture(render);
                bindReflectionMaterial(texture.descriptorSet);
                const glm::mat4 modelMatrix = object->modelMatrix();
                const auto push = buildObjectPushConstants(modelMatrix, render);
                vkCmdPushConstants(commandBuffer, pipelineLayout,
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, sizeof(push), &push);
                vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(INDICES.size()), 1, 0, 0, 0);
                break;
            }
            case vkengine::MeshType::CustomMesh: {
                if (render.meshResource.empty()) {
                    continue;
                }
                auto& meshBuffers = getOrCreateMesh(render);
                if (meshBuffers.indexCount == 0) {
                    continue;
                }
                VkBuffer meshVertexBuffers[] = {meshBuffers.vertexBuffer};
                VkDeviceSize meshOffsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, meshVertexBuffers, meshOffsets);
                vkCmdBindIndexBuffer(commandBuffer, meshBuffers.indexBuffer, 0, meshBuffers.indexType);
                const auto& texture = getOrCreateTexture(render);
                bindReflectionMaterial(texture.descriptorSet);
                const glm::mat4 modelMatrix = object->modelMatrix();
                const auto push = buildObjectPushConstants(modelMatrix, render);
                vkCmdPushConstants(commandBuffer, pipelineLayout,
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, sizeof(push), &push);
                vkCmdDrawIndexed(commandBuffer, meshBuffers.indexCount, 1, 0, 0, 0);
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, cubeVertexBuffers, cubeOffsets);
                vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
                break;
            }
            default:
                break;
            }
        }

        vkCmdEndRenderPass(commandBuffer);

        reflectionImageReady = true;

    }

    // Determine if we should use offscreen rendering for post-processing
    const bool usePostProcess = blurSettings.enabled && 
                                offscreenFramebuffer != VK_NULL_HANDLE && 
                                blurHorizontalPipeline != VK_NULL_HANDLE;

    // Transition offscreen image if needed for post-processing
    if (usePostProcess && !offscreenImageReady) {
        VkImageMemoryBarrier offscreenBarrier{};
        offscreenBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        offscreenBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        offscreenBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        offscreenBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        offscreenBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        offscreenBarrier.image = offscreenColorImage;
        offscreenBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        offscreenBarrier.subresourceRange.baseMipLevel = 0;
        offscreenBarrier.subresourceRange.levelCount = 1;
        offscreenBarrier.subresourceRange.baseArrayLayer = 0;
        offscreenBarrier.subresourceRange.layerCount = 1;
        offscreenBarrier.srcAccessMask = 0;
        offscreenBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &offscreenBarrier);
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    // Render to offscreen if post-processing, else directly to swapchain
    renderPassInfo.framebuffer = usePostProcess ? offscreenFramebuffer : swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{skySettings.color.r, skySettings.color.g, skySettings.color.b, skySettings.color.a}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkPipeline activePipeline = graphicsPipeline;
    if (styleIndex < stylePipelines.size() && stylePipelines[styleIndex] != VK_NULL_HANDLE) {
        activePipeline = stylePipelines[styleIndex];
    }
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, cubeVertexBuffers, cubeOffsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                            0, 1, &descriptorSets[imageIndex], 0, nullptr);

    VkDescriptorSet boundMaterialSet = VK_NULL_HANDLE;
    auto bindMaterialSet = [&](VkDescriptorSet set) {
        VkDescriptorSet target = set != VK_NULL_HANDLE ? set : defaultTexture.descriptorSet;
        if (target == VK_NULL_HANDLE || target == boundMaterialSet) {
            return;
        }
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                                1, 1, &target, 0, nullptr);
        boundMaterialSet = target;
    };
    bindMaterialSet(defaultTexture.descriptorSet);

    if (!particleMeshInstances.empty()) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                                0, 1, &descriptorSets[imageIndex], 0, nullptr);
        bindMaterialSet(defaultTexture.descriptorSet);

        for (const auto& meshInstance : particleMeshInstances) {
            const auto& render = meshInstance.render;
            if (!render.visible) {
                continue;
            }

            switch (render.mesh) {
            case vkengine::MeshType::Cube: {
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, cubeVertexBuffers, cubeOffsets);
                vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
                const auto& texture = getOrCreateTexture(render);
                bindMaterialSet(texture.descriptorSet);
                const auto push = buildObjectPushConstants(meshInstance.model, render);
                vkCmdPushConstants(commandBuffer, pipelineLayout,
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, sizeof(push), &push);
                vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(INDICES.size()), 1, 0, 0, 0);
                break;
            }
            case vkengine::MeshType::CustomMesh: {
                if (render.meshResource.empty()) {
                    continue;
                }
                auto& meshBuffers = getOrCreateMesh(render);
                if (meshBuffers.indexCount == 0) {
                    continue;
                }
                VkBuffer meshVertexBuffers[] = {meshBuffers.vertexBuffer};
                VkDeviceSize meshOffsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, meshVertexBuffers, meshOffsets);
                vkCmdBindIndexBuffer(commandBuffer, meshBuffers.indexBuffer, 0, meshBuffers.indexType);
                const auto& texture = getOrCreateTexture(render);
                bindMaterialSet(texture.descriptorSet);
                const auto push = buildObjectPushConstants(meshInstance.model, render);
                vkCmdPushConstants(commandBuffer, pipelineLayout,
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, sizeof(push), &push);
                vkCmdDrawIndexed(commandBuffer, meshBuffers.indexCount, 1, 0, 0, 0);
                break;
            }
            case vkengine::MeshType::WireCubeLines: {
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline);
                VkBuffer lineBuffers[] = {lineVertexBuffer};
                VkDeviceSize lineOffsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, lineBuffers, lineOffsets);
                const auto& texture = getOrCreateTexture(render);
                bindMaterialSet(texture.descriptorSet);
                const auto push = buildObjectPushConstants(meshInstance.model, render);
                vkCmdPushConstants(commandBuffer, pipelineLayout,
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, sizeof(push), &push);
                vkCmdDraw(commandBuffer, static_cast<uint32_t>(LINE_VERTICES.size()), 1, 0, 0);
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline);
                break;
            }
            default:
                break;
            }
        }
    }

    for (const auto* object : objects) {
        const auto& render = object->render();
        if (!render.visible) {
            continue;
        }

        switch (object->mesh()) {
        case vkengine::MeshType::Cube: {
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, cubeVertexBuffers, cubeOffsets);
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
            const auto& texture = getOrCreateTexture(render);
            bindMaterialSet(texture.descriptorSet);
            const glm::mat4 modelMatrix = object->modelMatrix();
            const auto push = buildObjectPushConstants(modelMatrix, render);
            vkCmdPushConstants(commandBuffer, pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(push), &push);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(INDICES.size()), 1, 0, 0, 0);
            break;
        }
        case vkengine::MeshType::WireCubeLines: {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline);
            VkBuffer lineBuffers[] = {lineVertexBuffer};
            VkDeviceSize lineOffsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, lineBuffers, lineOffsets);
            const auto& texture = getOrCreateTexture(render);
            bindMaterialSet(texture.descriptorSet);
            const glm::mat4 modelMatrix = object->modelMatrix();
            const auto push = buildObjectPushConstants(modelMatrix, render);
            vkCmdPushConstants(commandBuffer, pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(push), &push);
            vkCmdDraw(commandBuffer, static_cast<uint32_t>(LINE_VERTICES.size()), 1, 0, 0);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline);
            break;
        }
        case vkengine::MeshType::CustomMesh: {
            if (render.meshResource.empty()) {
                continue;
            }
            auto& meshBuffers = getOrCreateMesh(render);
            if (meshBuffers.indexCount == 0) {
                continue;
            }
            VkBuffer meshVertexBuffers[] = {meshBuffers.vertexBuffer};
            VkDeviceSize meshOffsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, meshVertexBuffers, meshOffsets);
            vkCmdBindIndexBuffer(commandBuffer, meshBuffers.indexBuffer, 0, meshBuffers.indexType);
            const auto& texture = getOrCreateTexture(render);
            bindMaterialSet(texture.descriptorSet);
            const glm::mat4 modelMatrix = object->modelMatrix();
            const auto push = buildObjectPushConstants(modelMatrix, render);
            vkCmdPushConstants(commandBuffer, pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(push), &push);
            vkCmdDrawIndexed(commandBuffer, meshBuffers.indexCount, 1, 0, 0, 0);
            break;
        }
        default:
            break;
        }
    }

    if (deformableIndexCount > 0 &&
        deformableVertexBuffer != VK_NULL_HANDLE &&
        deformableIndexBuffer != VK_NULL_HANDLE) {
        bindMaterialSet(defaultTexture.descriptorSet);
        vkengine::RenderComponent defaultRender{};
        VkBuffer deformableBuffers[] = {deformableVertexBuffer};
        VkDeviceSize deformableOffsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, deformableBuffers, deformableOffsets);
        vkCmdBindIndexBuffer(commandBuffer, deformableIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

        for (const auto& draw : deformableDraws) {
            const auto push = buildObjectPushConstants(draw.model, defaultRender);
            vkCmdPushConstants(commandBuffer, pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(push), &push);
            vkCmdDrawIndexed(commandBuffer, draw.indexCount, 1, draw.indexOffset,
                             static_cast<int32_t>(draw.vertexOffset), 0);
        }

        vkCmdBindVertexBuffers(commandBuffer, 0, 1, cubeVertexBuffers, cubeOffsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    }

    if (particlePipeline != VK_NULL_HANDLE &&
        particleVertexBuffer != VK_NULL_HANDLE &&
        particleVertexCount > 0) {
        VkBuffer particleBuffers[] = {particleVertexBuffer};
        VkDeviceSize particleOffsets[] = {0};
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particlePipeline);
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, particleBuffers, particleOffsets);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                                0, 1, &descriptorSets[imageIndex], 0, nullptr);
        bindMaterialSet(defaultTexture.descriptorSet);
        for (const auto& range : particleDrawRanges) {
            if (range.count == 0) {
                continue;
            }
            vkCmdDraw(commandBuffer, range.count, 1, range.offset, 0);
        }
    }

    uiLayer.render(commandBuffer);

    vkCmdEndRenderPass(commandBuffer);

    // Apply blur post-processing if enabled
    if (usePostProcess) {
        offscreenImageReady = true;

        // Push constant structure for blur shader
        struct BlurPushConstants {
            float texelSizeX;
            float texelSizeY;
            float directionX;
            float directionY;
            float radius;
            float sigma;
            float pad0;
            float pad1;
        };

        const float texelSizeX = 1.0f / static_cast<float>(swapChainExtent.width);
        const float texelSizeY = 1.0f / static_cast<float>(swapChainExtent.height);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapChainExtent.width);
        viewport.height = static_cast<float>(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;

        // ============ Pass 1: Horizontal blur (offscreen -> pingPong) ============
        
        // Transition offscreen to shader read
        VkImageMemoryBarrier offscreenToRead{};
        offscreenToRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        offscreenToRead.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        offscreenToRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        offscreenToRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        offscreenToRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        offscreenToRead.image = offscreenColorImage;
        offscreenToRead.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        offscreenToRead.subresourceRange.baseMipLevel = 0;
        offscreenToRead.subresourceRange.levelCount = 1;
        offscreenToRead.subresourceRange.baseArrayLayer = 0;
        offscreenToRead.subresourceRange.layerCount = 1;
        offscreenToRead.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        offscreenToRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        // Transition pingPong to color attachment
        VkImageMemoryBarrier pingPongToWrite{};
        pingPongToWrite.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        pingPongToWrite.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        pingPongToWrite.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        pingPongToWrite.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pingPongToWrite.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pingPongToWrite.image = pingPongImage;
        pingPongToWrite.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        pingPongToWrite.subresourceRange.baseMipLevel = 0;
        pingPongToWrite.subresourceRange.levelCount = 1;
        pingPongToWrite.subresourceRange.baseArrayLayer = 0;
        pingPongToWrite.subresourceRange.layerCount = 1;
        pingPongToWrite.srcAccessMask = 0;
        pingPongToWrite.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        std::array<VkImageMemoryBarrier, 2> pass1Barriers = {offscreenToRead, pingPongToWrite};
        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             0, 0, nullptr, 0, nullptr,
                             static_cast<uint32_t>(pass1Barriers.size()), pass1Barriers.data());

        // Begin horizontal blur pass
        VkRenderPassBeginInfo hPassInfo{};
        hPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        hPassInfo.renderPass = postProcessRenderPass;
        hPassInfo.framebuffer = pingPongFramebuffer;
        hPassInfo.renderArea.offset = {0, 0};
        hPassInfo.renderArea.extent = swapChainExtent;
        VkClearValue hClear{};
        hClear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        hPassInfo.clearValueCount = 1;
        hPassInfo.pClearValues = &hClear;

        vkCmdBeginRenderPass(commandBuffer, &hPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blurHorizontalPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, postProcessPipelineLayout,
                                0, 1, &postProcessDescriptorSet, 0, nullptr);
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        BlurPushConstants hPush{};
        hPush.texelSizeX = texelSizeX;
        hPush.texelSizeY = texelSizeY;
        hPush.directionX = 1.0f;  // Horizontal
        hPush.directionY = 0.0f;
        hPush.radius = blurSettings.radius;
        hPush.sigma = blurSettings.sigma;
        vkCmdPushConstants(commandBuffer, postProcessPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(hPush), &hPush);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0); // Fullscreen triangle
        vkCmdEndRenderPass(commandBuffer);

        // ============ Pass 2: Vertical blur (pingPong -> swapchain) ============

        // Transition pingPong to shader read
        VkImageMemoryBarrier pingPongToRead{};
        pingPongToRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        pingPongToRead.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        pingPongToRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        pingPongToRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pingPongToRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        pingPongToRead.image = pingPongImage;
        pingPongToRead.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        pingPongToRead.subresourceRange.baseMipLevel = 0;
        pingPongToRead.subresourceRange.levelCount = 1;
        pingPongToRead.subresourceRange.baseArrayLayer = 0;
        pingPongToRead.subresourceRange.layerCount = 1;
        pingPongToRead.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        pingPongToRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &pingPongToRead);

        // Begin vertical blur pass (render to swapchain)
        VkRenderPassBeginInfo vPassInfo{};
        vPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        vPassInfo.renderPass = postProcessRenderPass;
        vPassInfo.framebuffer = postProcessSwapchainFramebuffers[imageIndex];
        vPassInfo.renderArea.offset = {0, 0};
        vPassInfo.renderArea.extent = swapChainExtent;
        VkClearValue vClear{};
        vClear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        vPassInfo.clearValueCount = 1;
        vPassInfo.pClearValues = &vClear;

        vkCmdBeginRenderPass(commandBuffer, &vPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blurVerticalPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, postProcessPipelineLayout,
                                0, 1, &pingPongDescriptorSet, 0, nullptr);
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        BlurPushConstants vPush{};
        vPush.texelSizeX = texelSizeX;
        vPush.texelSizeY = texelSizeY;
        vPush.directionX = 0.0f;
        vPush.directionY = 1.0f;  // Vertical
        vPush.radius = blurSettings.radius;
        vPush.sigma = blurSettings.sigma;
        vkCmdPushConstants(commandBuffer, postProcessPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(vPush), &vPush);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0); // Fullscreen triangle
        vkCmdEndRenderPass(commandBuffer);

        // Transition swapchain image to present layout
        VkImageMemoryBarrier swapchainToPresent{};
        swapchainToPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        swapchainToPresent.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        swapchainToPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        swapchainToPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        swapchainToPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        swapchainToPresent.image = swapChainImages[imageIndex];
        swapchainToPresent.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        swapchainToPresent.subresourceRange.baseMipLevel = 0;
        swapchainToPresent.subresourceRange.levelCount = 1;
        swapchainToPresent.subresourceRange.baseArrayLayer = 0;
        swapchainToPresent.subresourceRange.layerCount = 1;
        swapchainToPresent.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        swapchainToPresent.dstAccessMask = 0;

        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &swapchainToPresent);

        // Reset offscreen ready flag for layout transitions
        offscreenImageReady = false;
    }

    captureSwapchainImage(commandBuffer, imageIndex);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
    }
}

void VulkanRenderer::captureSwapchainImage(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    if (!captureRequested || imageIndex != captureImageIndex || captureBuffer == VK_NULL_HANDLE) {
        return;
    }

    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = swapChainImages[imageIndex];
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.baseMipLevel = 0;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.baseArrayLayer = 0;
    toTransfer.subresourceRange.layerCount = 1;
    toTransfer.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &toTransfer);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {swapChainExtent.width, swapChainExtent.height, 1};

    vkCmdCopyImageToBuffer(commandBuffer,
                           swapChainImages[imageIndex],
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           captureBuffer,
                           1,
                           &region);

    VkImageMemoryBarrier toPresent{};
    toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.image = swapChainImages[imageIndex];
    toPresent.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toPresent.subresourceRange.baseMipLevel = 0;
    toPresent.subresourceRange.levelCount = 1;
    toPresent.subresourceRange.baseArrayLayer = 0;
    toPresent.subresourceRange.layerCount = 1;
    toPresent.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    toPresent.dstAccessMask = 0;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &toPresent);
}

void VulkanRenderer::finalizeCapture(uint32_t imageIndex)
{
    if (!captureRequested || imageIndex != captureImageIndex || captureBufferMemory == VK_NULL_HANDLE) {
        return;
    }

    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    std::vector<std::uint8_t> rgba(captureBufferSize);
    void* mapped = nullptr;
    if (vkMapMemory(device, captureBufferMemory, 0, captureBufferSize, 0, &mapped) == VK_SUCCESS) {
        std::memcpy(rgba.data(), mapped, static_cast<size_t>(captureBufferSize));
        vkUnmapMemory(device, captureBufferMemory);
    } else {
        captureFailed = true;
        captureRequested = false;
        return;
    }

    if (swapChainImageFormat == VK_FORMAT_B8G8R8A8_SRGB || swapChainImageFormat == VK_FORMAT_B8G8R8A8_UNORM) {
        for (size_t i = 0; i + 3 < rgba.size(); i += 4) {
            std::swap(rgba[i + 0], rgba[i + 2]);
        }
    }

    const uint32_t width = captureWidth;
    const uint32_t height = captureHeight;

    captureCompleted = vkengine::writeJpeg(capturePath, static_cast<int>(width), static_cast<int>(height), rgba, 92);
    captureFailed = !captureCompleted;
    captureRequested = false;
}

void VulkanRenderer::populateCameraBufferObject(const vkengine::Camera& /*camera*/,
                                               const glm::mat4& view,
                                               const glm::mat4& proj,
                                               const glm::vec3& cameraPosition,
                                               const glm::mat4& reflectionViewProj,
                                               const glm::vec4& reflectionPlane,
                                               CameraBufferObject& ubo)
{
    const auto& scene = engine->scene();

    ubo.view = view;
    ubo.proj = proj;
    ubo.cameraPosition = glm::vec4(cameraPosition, 1.0f);
    ubo.reflectionViewProj = reflectionViewProj;
    ubo.reflectionPlane = reflectionPlane;

    glm::vec3 lightPos{3.0f, 4.0f, 2.0f};
    glm::vec3 lightColor{1.0f};
    float lightIntensity = 2.0f;
    int activeLights = 0;

    for (int i = 0; i < 4; ++i) {
        ubo.lightPositions[i] = glm::vec4(0.0f);
        ubo.lightColors[i] = glm::vec4(0.0f);
        ubo.lightDirections[i] = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
        ubo.lightSpotAngles[i] = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        ubo.lightAreaParams[i] = glm::vec4(0.0f);
    }

    for (const auto& light : scene.lights()) {
        if (!light.isEnabled()) {
            continue;
        }
        if (activeLights < 4) {
            const glm::vec3 position = light.position();
            glm::vec3 direction = light.direction();
            if (!std::isfinite(direction.x) || glm::length2(direction) < 1e-4f) {
                direction = glm::vec3(0.0f, -1.0f, 0.0f);
            }
            direction = glm::normalize(direction);
            const float intensity = std::max(0.0f, light.intensity());
            const float range = std::max(0.0f, light.range());
            const float innerAngle = glm::clamp(light.innerConeAngle(), 0.0f, 3.1415926f);
            const float outerAngle = glm::clamp(light.outerConeAngle(), 0.0f, 3.1415926f);
            float innerCos = std::cos(innerAngle);
            float outerCos = std::cos(outerAngle);
            if (innerCos < outerCos) {
                std::swap(innerCos, outerCos);
            }
            const glm::vec2 areaSize = light.areaSize();
            const float halfWidth = std::max(0.0f, areaSize.x * 0.5f);
            const float halfHeight = std::max(0.0f, areaSize.y * 0.5f);

            ubo.lightPositions[activeLights] = glm::vec4(position, 1.0f);
            ubo.lightColors[activeLights] = glm::vec4(light.color(), intensity);
            ubo.lightDirections[activeLights] = glm::vec4(direction, static_cast<float>(light.type()));
            ubo.lightSpotAngles[activeLights] = glm::vec4(innerCos, outerCos, range, 0.0f);
            ubo.lightAreaParams[activeLights] = glm::vec4(halfWidth, halfHeight, range, 0.0f);
            ++activeLights;
        }
        if (activeLights == 1) {
            lightPos = light.position();
            lightColor = light.color();
            lightIntensity = light.intensity();
        }
    }

    if (activeLights == 0) {
        ubo.lightPositions[0] = glm::vec4(lightPos, 1.0f);
        ubo.lightColors[0] = glm::vec4(lightColor, std::max(0.0f, lightIntensity));
        ubo.lightDirections[0] = glm::vec4(0.0f, -1.0f, 0.0f, static_cast<float>(vkengine::LightType::Point));
        ubo.lightSpotAngles[0] = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        ubo.lightAreaParams[0] = glm::vec4(0.0f);
        activeLights = 1;
    }

    glm::vec3 animatedLightPos = lightPos;
    if (animateLight) {
        const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
        const float orbitRadius = 4.0f;
        animatedLightPos = glm::vec3(std::cos(t * 0.35f) * orbitRadius,
                                      3.5f + std::sin(t * 0.65f) * 1.0f,
                                      std::sin(t * 0.35f) * orbitRadius);
    }
    if (activeLights > 0) {
        ubo.lightPositions[0] = glm::vec4(animatedLightPos, 1.0f);
        ubo.lightColors[0] = glm::vec4(lightColor, std::max(0.0f, lightIntensity));
    }

    const glm::vec3 lightTarget = glm::vec3(0.0f);
    glm::vec3 lightDirection = animatedLightPos - lightTarget;
    const bool lightDirectionFinite = std::isfinite(lightDirection.x) &&
                                      std::isfinite(lightDirection.y) &&
                                      std::isfinite(lightDirection.z);
    if (!lightDirectionFinite || glm::length2(lightDirection) < 1e-4f) {
        lightDirection = glm::vec3(-0.4f, -1.0f, -0.3f);
    }
    lightDirection = glm::normalize(lightDirection);

    constexpr float FAR_LIGHT_DISTANCE = 75.0f;
    const glm::vec3 farLightPos = lightTarget + lightDirection * FAR_LIGHT_DISTANCE;

    ubo.lightParams = glm::vec4(static_cast<float>(activeLights), 0.0f, 0.0f, 0.0f);
    ubo.shadingParams = glm::vec4(static_cast<float>(visualizationMode),
                                  shadowsEnabled ? 1.0f : 0.0f,
                                  specularEnabled ? 1.0f : 0.0f,
                                  animateLight ? 1.0f : 0.0f);
    ubo.fogColor = glm::vec4(fogSettings.color, fogSettings.enabled ? 1.0f : 0.0f);
    ubo.fogParams = glm::vec4(fogSettings.density,
                              fogSettings.heightFalloff,
                              fogSettings.heightOffset,
                              fogSettings.startDistance);
    ubo.blurParams = glm::vec4(blurSettings.enabled ? 1.0f : 0.0f,
                               blurSettings.radius,
                               blurSettings.sigma,
                               static_cast<float>(blurSettings.type));

    glm::vec3 lightUp = glm::vec3(0.0f, 1.0f, 0.0f);
    const float alignment = std::abs(glm::dot(glm::normalize(lightTarget - farLightPos), lightUp));
    if (alignment > 0.99f) {
        lightUp = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    const glm::mat4 lightView = glm::lookAt(farLightPos, lightTarget, lightUp);
    constexpr float SHADOW_RANGE = 18.0f;
    const glm::mat4 lightProj = glm::ortho(-SHADOW_RANGE, SHADOW_RANGE,
                                           -SHADOW_RANGE, SHADOW_RANGE,
                                           1.0f, 150.0f);
    ubo.lightViewProj = lightProj * lightView;
}

void VulkanRenderer::updateUniformBuffer(uint32_t currentImage)
{
    const auto& scene = engine->scene();
    const auto& camera = scene.camera();

    const glm::mat4 view = camera.viewMatrix();
    const auto height = std::max<uint32_t>(1u, swapChainExtent.height);
    const float aspect = static_cast<float>(swapChainExtent.width) / static_cast<float>(height);
    const glm::mat4 proj = camera.projectionMatrix(aspect);
    const glm::vec3 cameraPosition = camera.getPosition();

    mirrorAvailable = false;
    glm::vec4 reflectionPlane{0.0f, 1.0f, 0.0f, 0.0f};
    glm::mat4 reflectionView = view;
    glm::mat4 reflectionProj = proj;
    glm::vec3 reflectionCameraPosition = cameraPosition;

    for (const auto* object : scene.objectsCached()) {
        const auto& render = object->render();
        if (render.albedoTexture != "__reflection__") {
            continue;
        }

        const glm::mat4 modelMatrix = object->modelMatrix();
        glm::vec3 planeNormal = glm::normalize(glm::mat3(modelMatrix) * glm::vec3(0.0f, 0.0f, 1.0f));
        if (!std::isfinite(planeNormal.x) || glm::length2(planeNormal) < 1e-4f) {
            planeNormal = glm::vec3(0.0f, 0.0f, 1.0f);
        }
        const glm::vec3 planePoint = glm::vec3(modelMatrix[3]);
        const float planeD = -glm::dot(planeNormal, planePoint);
        reflectionPlane = glm::vec4(planeNormal, planeD);

        const glm::vec3 cameraForward = camera.forwardVector();
        const glm::vec3 cameraUp = camera.upVector();

        auto reflectPoint = [&](const glm::vec3& point) {
            const float dist = glm::dot(planeNormal, point) + planeD;
            return point - 2.0f * dist * planeNormal;
        };
        auto reflectVector = [&](const glm::vec3& vec) {
            return vec - 2.0f * glm::dot(planeNormal, vec) * planeNormal;
        };

        reflectionCameraPosition = reflectPoint(cameraPosition);
        const glm::vec3 reflectedForward = reflectVector(cameraForward);
        glm::vec3 reflectedUp = reflectVector(cameraUp);
        if (glm::length2(reflectedUp) < 1e-4f) {
            reflectedUp = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        reflectionView = glm::lookAt(reflectionCameraPosition,
                                     reflectionCameraPosition + reflectedForward,
                                     reflectedUp);
        reflectionProj = proj;
        mirrorAvailable = true;
        break;
    }

    cachedReflectionView = reflectionView;
    cachedReflectionProj = reflectionProj;
    cachedReflectionViewProj = reflectionProj * reflectionView;
    cachedReflectionPlane = reflectionPlane;
    cachedReflectionCameraPosition = reflectionCameraPosition;

    CameraBufferObject ubo{};
    populateCameraBufferObject(camera, view, proj, cameraPosition, cachedReflectionViewProj, cachedReflectionPlane, ubo);

    std::memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));

    if (mirrorAvailable && reflectionUniformBuffersMapped.size() > currentImage && reflectionUniformBuffersMapped[currentImage] != nullptr) {
        CameraBufferObject reflectionUbo{};
        populateCameraBufferObject(camera,
                                   cachedReflectionView,
                                   cachedReflectionProj,
                                   cachedReflectionCameraPosition,
                                   cachedReflectionViewProj,
                                   cachedReflectionPlane,
                                   reflectionUbo);
        std::memcpy(reflectionUniformBuffersMapped[currentImage], &reflectionUbo, sizeof(reflectionUbo));
    }
}

void VulkanRenderer::updateParticleVertexBuffer(float /*deltaSeconds*/)
{
    particleVertexCount = 0;
    cpuParticleVertices.clear();
    for (auto& range : particleDrawRanges) {
        range.offset = 0;
        range.count = 0;
    }
    particleMeshInstances.clear();

    if (engine == nullptr) {
        return;
    }

    const auto& camera = engine->scene().camera();
    glm::vec3 cameraRight = glm::normalize(camera.rightVector());
    glm::vec3 cameraForward = glm::normalize(camera.forwardVector());
    glm::vec3 cameraUp = glm::normalize(glm::cross(cameraRight, cameraForward));
    if (glm::length2(cameraUp) < 1e-6f) {
        cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    std::array<std::vector<ParticleVertex>, vkengine::kParticleShapeCount> shapeBuckets;
    engine->particles().forEachAliveParticleWithEmitter([&](const vkengine::ParticleEmitter& emitter, const vkengine::Particle& particle) {
        const glm::vec3 center = particle.position;
        const float normalizedAge = particle.lifetime > 0.0f
                        ? glm::clamp(particle.age / particle.lifetime, 0.0f, 1.0f)
                        : 1.0f;
        const float fade = 1.0f - normalizedAge;
        const float hotCore = std::pow(fade, 2.6f);
        const float taper = glm::mix(1.0f, 0.35f, normalizedAge);
        const float flicker = 0.75f + 0.25f * glm::sin(20.0f * particle.age + glm::dot(particle.position, glm::vec3(11.3f, 7.1f, 5.7f)));
        const float billboardSize = std::max(0.018f, particle.scale * (0.35f + 0.95f * hotCore) * taper);
        const glm::vec3 right = cameraRight * billboardSize;
        const glm::vec3 up = cameraUp * billboardSize;

        const glm::vec4 startColor = emitter.getStartColor();
        const glm::vec4 endColor = emitter.getEndColor();
        const glm::vec4 baseColor = glm::mix(startColor, endColor, normalizedAge);
        const float alpha = glm::clamp(baseColor.a * (0.25f + 1.05f * hotCore) * flicker * fade, 0.0f, 1.0f);
        const glm::vec4 color{baseColor.r, baseColor.g, baseColor.b, alpha};

        if (emitter.renderModeValue() == vkengine::ParticleRenderMode::Mesh) {
            vkengine::RenderComponent render{};
            render.mesh = emitter.meshTypeValue();
            render.meshResource = emitter.meshResourceValue();
            render.albedoTexture = "__default__";
            render.baseColor = color;
            render.opacity = alpha;

            glm::mat4 model{1.0f};
            model = glm::translate(model, center);
            model = glm::scale(model, glm::vec3(emitter.meshScaleValue()));

            particleMeshInstances.push_back({model, std::move(render)});
            return;
        }

        const float shapeValue = static_cast<float>(emitter.shape());
        ParticleVertex v0{center - right + up, {0.0f, 1.0f}, color, shapeValue};
        ParticleVertex v1{center + right + up, {1.0f, 1.0f}, color, shapeValue};
        ParticleVertex v2{center + right - up, {1.0f, 0.0f}, color, shapeValue};
        ParticleVertex v3{center - right - up, {0.0f, 0.0f}, color, shapeValue};

        const auto shapeIndex = static_cast<std::size_t>(emitter.shape());
        if (shapeIndex >= shapeBuckets.size()) {
            return;
        }
        auto& bucket = shapeBuckets[shapeIndex];
        bucket.push_back(v0);
        bucket.push_back(v2);
        bucket.push_back(v1);

        bucket.push_back(v0);
        bucket.push_back(v3);
        bucket.push_back(v2);
    });

    uint32_t runningOffset = 0;
    for (std::size_t i = 0; i < shapeBuckets.size(); ++i) {
        auto& bucket = shapeBuckets[i];
        particleDrawRanges[i].offset = runningOffset;
        particleDrawRanges[i].count = static_cast<uint32_t>(bucket.size());
        runningOffset += particleDrawRanges[i].count;
        cpuParticleVertices.insert(cpuParticleVertices.end(), bucket.begin(), bucket.end());
    }

    if (cpuParticleVertices.empty()) {
        return;
    }

    const VkDeviceSize requiredSize = cpuParticleVertices.size() * sizeof(ParticleVertex);
    ensureParticleVertexCapacity(requiredSize);
    void* data = nullptr;
    vkMapMemory(device, particleVertexBufferMemory, 0, requiredSize, 0, &data);
    std::memcpy(data, cpuParticleVertices.data(), static_cast<size_t>(requiredSize));
    vkUnmapMemory(device, particleVertexBufferMemory);

    particleVertexCount = static_cast<uint32_t>(cpuParticleVertices.size());
}

void VulkanRenderer::ensureCaptureBuffer(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) {
        return;
    }

    const VkDeviceSize requiredSize = static_cast<VkDeviceSize>(width) * height * 4;
    if (captureBuffer != VK_NULL_HANDLE && captureBufferSize == requiredSize) {
        captureWidth = width;
        captureHeight = height;
        return;
    }

    destroyCaptureResources();
    createBuffer(requiredSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 captureBuffer,
                 captureBufferMemory);
    captureBufferSize = requiredSize;
    captureWidth = width;
    captureHeight = height;
}

void VulkanRenderer::destroyCaptureResources()
{
    if (device == VK_NULL_HANDLE) {
        captureBuffer = VK_NULL_HANDLE;
        captureBufferMemory = VK_NULL_HANDLE;
        captureBufferSize = 0;
        captureWidth = 0;
        captureHeight = 0;
        return;
    }

    if (captureBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, captureBuffer, nullptr);
        captureBuffer = VK_NULL_HANDLE;
    }
    if (captureBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, captureBufferMemory, nullptr);
        captureBufferMemory = VK_NULL_HANDLE;
    }
    captureBufferSize = 0;
    captureWidth = 0;
    captureHeight = 0;
}

void VulkanRenderer::handleCameraInput(float deltaSeconds)
{
    if (engine == nullptr) {
        return;
    }

    auto input = inputManager.consumeCameraInput();
    engine->scene().camera().applyInput(input, deltaSeconds);
}

void VulkanRenderer::updateDeformableMeshes()
{
    deformableDraws.clear();
    deformableIndexCount = 0;
    deformableVertexScratch.clear();
    deformableIndexScratch.clear();

    if (engine == nullptr) {
        return;
    }

    std::vector<glm::vec3> normals;
    const auto& objects = engine->scene().objectsCached();
    auto appendSoftBody = [&](const auto* body, const glm::vec4& pinnedColor, const glm::vec4& freeColor,
                              const vkengine::GameObject& object) {
        if (body == nullptr) {
            return;
        }
        const auto& nodes = body->nodes();
        if (nodes.empty()) {
            return;
        }

        normals.clear();
        body->computeVertexNormals(normals);

        const uint32_t vertexOffset = static_cast<uint32_t>(deformableVertexScratch.size());
        deformableVertexScratch.reserve(deformableVertexScratch.size() + nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            Vertex v{};
            v.pos = nodes[i].position;
            v.color = nodes[i].pinned ? pinnedColor : freeColor;
            v.normal = i < normals.size() ? normals[i] : glm::vec3(0.0f, 1.0f, 0.0f);
            deformableVertexScratch.push_back(v);
        }

        const uint32_t indexOffset = static_cast<uint32_t>(deformableIndexScratch.size());
        const auto& localIndices = body->indices();
        deformableIndexScratch.reserve(deformableIndexScratch.size() + localIndices.size());
        for (uint32_t idx : localIndices) {
            deformableIndexScratch.push_back(idx + vertexOffset);
        }

        deformableDraws.push_back(DeformableDraw{
            static_cast<uint32_t>(localIndices.size()),
            indexOffset,
            vertexOffset,
            object.modelMatrix(),
        });
    };

    for (const auto* object : objects) {
        switch (object->mesh()) {
        case vkengine::MeshType::DeformableCloth:
            appendSoftBody(object->deformable(),
                           glm::vec4(0.95f, 0.35f, 0.35f, 1.0f),
                           glm::vec4(0.25f, 0.7f, 1.0f, 1.0f),
                           *object);
            break;
        case vkengine::MeshType::SoftBodyVolume:
            appendSoftBody(object->softBody(),
                           glm::vec4(1.0f, 0.3f, 0.55f, 1.0f),
                           glm::vec4(0.6f, 0.9f, 0.7f, 1.0f),
                           *object);
            break;
        default:
            break;
        }
    }

    if (deformableVertexScratch.empty() || deformableIndexScratch.empty()) {
        return;
    }

    ensureDeformableBuffers(deformableVertexScratch.size() * sizeof(Vertex),
                            deformableIndexScratch.size() * sizeof(uint32_t));

    void* mapped = nullptr;
    VkDeviceSize vertexBytes = deformableVertexScratch.size() * sizeof(Vertex);
    vkMapMemory(device, deformableVertexMemory, 0, vertexBytes, 0, &mapped);
    std::memcpy(mapped, deformableVertexScratch.data(), static_cast<size_t>(vertexBytes));
    vkUnmapMemory(device, deformableVertexMemory);

    VkDeviceSize indexBytes = deformableIndexScratch.size() * sizeof(uint32_t);
    vkMapMemory(device, deformableIndexMemory, 0, indexBytes, 0, &mapped);
    std::memcpy(mapped, deformableIndexScratch.data(), static_cast<size_t>(indexBytes));
    vkUnmapMemory(device, deformableIndexMemory);

    deformableIndexCount = static_cast<uint32_t>(deformableIndexScratch.size());
}

void VulkanRenderer::drawFrame()
{
    const auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - lastFrameTime).count();
    lastFrameTime = now;
    deltaTime = std::clamp(deltaTime, 0.0f, 0.1f);
    handleCameraInput(deltaTime);
    engine->update(deltaTime);
    updateDeformableMeshes();
    updateParticleVertexBuffer(deltaTime);

    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX,
                                            imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image");
    }

    if (captureRequested) {
        captureImageIndex = imageIndex;
        ensureCaptureBuffer(swapChainExtent.width, swapChainExtent.height);
    }

    updateUniformBuffer(imageIndex);

    uiLayer.newFrame();
    buildUi(deltaTime);

    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    vkResetCommandBuffer(commandBuffers[imageIndex], /*flags*/ 0);
    recordCommandBuffer(commandBuffers[imageIndex], imageIndex, deltaTime);

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[imageIndex]};

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer");
    }

    VkSwapchainKHR swapChains[] = {swapChain};
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (captureRequested) {
        finalizeCapture(imageIndex);
    }

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swap chain image");
    }

    currentFrame = (currentFrame + 1) % framesInFlight;
}

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

void VulkanRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                                 VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void VulkanRenderer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

VkCommandBuffer VulkanRenderer::beginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate single-use command buffer");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void VulkanRenderer::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void VulkanRenderer::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    (void)format;
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("Unsupported image layout transition");
    }

    vkCmdPipelineBarrier(commandBuffer,
                         sourceStage, destinationStage,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);

    endSingleTimeCommands(commandBuffer);
}

void VulkanRenderer::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &region);

    endSingleTimeCommands(commandBuffer);
}

bool VulkanRenderer::loadSkyTextureForUi(const std::filesystem::path& path)
{
    if (path.empty() || materialSampler == VK_NULL_HANDLE) {
        return false;
    }

    try {
        destroySkyTexture();

        const auto textureData = vkengine::loadTexture(path, /*flipVertical*/ false);
        uploadTextureToGpu("__sky_background__", textureData, skyTexture);
        skyTextureLoaded = true;
        skyTextureRequested = true;
        pendingSkyTexturePath = path;

        if (uiLayer.isInitialized()) {
            skyTextureId = uiLayer.registerTexture(materialSampler, skyTexture.view,
                                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        return true;
    } catch (const std::exception&) {
        destroySkyTexture();
        skyTextureLoaded = false;
        return false;
    }
}

void VulkanRenderer::destroySkyTexture()
{
    if (skyTextureId != VK_NULL_HANDLE) {
        uiLayer.unregisterTexture(skyTextureId);
        skyTextureId = VK_NULL_HANDLE;
    }

    if (skyTexture.image != VK_NULL_HANDLE) {
        destroyTextureResource(skyTexture);
    }
    skyTextureLoaded = false;
}

void VulkanRenderer::ensureSkyTextureLoaded()
{
    if (!skyTextureRequested || pendingSkyTexturePath.empty()) {
        return;
    }

    if (!skyTextureLoaded) {
        if (!loadSkyTextureForUi(pendingSkyTexturePath)) {
            skyTextureRequested = false;
        }
        return;
    }

    if (skyTextureLoaded && skyTextureId == VK_NULL_HANDLE && uiLayer.isInitialized()) {
        skyTextureId = uiLayer.registerTexture(materialSampler, skyTexture.view,
                                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

void VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                                VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                                VkImage& image, VkDeviceMemory& imageMemory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.aspectMask = aspectFlags;

    if ((aspectFlags & VK_IMAGE_ASPECT_DEPTH_BIT) && hasStencilComponent(format)) {
        viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view");
    }

    return imageView;
}

void VulkanRenderer::ensureParticleVertexCapacity(VkDeviceSize requiredSize)
{
    if (requiredSize == 0) {
        return;
    }

    if (particleVertexBuffer != VK_NULL_HANDLE && requiredSize <= particleVertexCapacity) {
        return;
    }

    if (particleVertexBuffer != VK_NULL_HANDLE || particleVertexBufferMemory != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
    }

    destroyParticleBuffers();

    particleVertexCapacity = std::max<VkDeviceSize>(requiredSize, sizeof(ParticleVertex) * 256);
    createBuffer(particleVertexCapacity,
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 particleVertexBuffer,
                 particleVertexBufferMemory);
}

void VulkanRenderer::destroyParticleBuffers()
{
    if (particleVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, particleVertexBuffer, nullptr);
        particleVertexBuffer = VK_NULL_HANDLE;
    }
    if (particleVertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, particleVertexBufferMemory, nullptr);
        particleVertexBufferMemory = VK_NULL_HANDLE;
    }
    particleVertexCapacity = 0;
    particleVertexCount = 0;
}

bool VulkanRenderer::checkValidationLayerSupport() const
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (std::strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

std::vector<const char*> VulkanRenderer::getRequiredExtensions() const
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

QueueFamilyIndices VulkanRenderer::findQueueFamilies(VkPhysicalDevice physicalDeviceHandle) const
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDeviceHandle, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDeviceHandle, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDeviceHandle, i, surface, &presentSupport);
        if (presentSupport == VK_TRUE) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        ++i;
    }

    return indices;
}

bool VulkanRenderer::isDeviceSuitable(VkPhysicalDevice physicalDeviceHandle) const
{
    QueueFamilyIndices indices = findQueueFamilies(physicalDeviceHandle);

    bool extensionsSupported = true;
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(physicalDeviceHandle, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDeviceHandle, nullptr, &extensionCount, availableExtensions.data());

    for (const char* requiredExtension : deviceExtensions) {
        bool found = false;
        for (const auto& extension : availableExtensions) {
            if (std::strcmp(requiredExtension, extension.extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            extensionsSupported = false;
            break;
        }
    }

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDeviceHandle);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

SwapChainSupportDetails VulkanRenderer::querySwapChainSupport(VkPhysicalDevice physicalDeviceHandle) const
{
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDeviceHandle, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDeviceHandle, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDeviceHandle, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDeviceHandle, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDeviceHandle, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR VulkanRenderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const
{
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats.front();
}

VkPresentModeKHR VulkanRenderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const
{
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
}

VkFormat VulkanRenderer::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
                                            VkFormatFeatureFlags features) const
{
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported format");
}

VkFormat VulkanRenderer::findDepthFormat() const
{
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
}

bool VulkanRenderer::hasStencilComponent(VkFormat format) const
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code) const
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }

    return shaderModule;
}

// ============================================================================
// Public ImGui-texture helpers
// ============================================================================

ImTextureID VulkanRenderer::registerImGuiTexture(const vkengine::TextureData& textureData) {
    TextureResource res{};
    uploadTextureToGpu("__imgui_user__", textureData, res);
    VkDescriptorSet dsId = uiLayer.registerTexture(
        materialSampler, res.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    imguiUserTextures[dsId] = res;
    return reinterpret_cast<ImTextureID>(dsId);
}

void VulkanRenderer::updateImGuiTexture(ImTextureID handle, const vkengine::TextureData& textureData) {
    auto ds = reinterpret_cast<VkDescriptorSet>(handle);
    auto it = imguiUserTextures.find(ds);
    if (it == imguiUserTextures.end()) return;

    // Destroy old GPU resources but keep the descriptor set alive.
    TextureResource& res = it->second;
    // Unregister from ImGui (we will re-register with the new view).
    uiLayer.unregisterTexture(ds);

    // Destroy old Vulkan objects.
    destroyTextureResource(res);
    res = {};

    // Upload new data.
    uploadTextureToGpu("__imgui_user__", textureData, res);

    // Re-register with ImGui – may return a different descriptor set.
    VkDescriptorSet newDs = uiLayer.registerTexture(
        materialSampler, res.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    if (newDs != ds) {
        imguiUserTextures.erase(it);
        imguiUserTextures[newDs] = res;
    }
    // Note: the caller should use the returned ImTextureID from registerImGuiTexture
    // and call updateImGuiTexture with the same handle.  If the underlying DS changed,
    // the old handle is no longer valid – but for typical usage this is fine because
    // ImGui::Image() is called each frame with the latest handle.
}

void VulkanRenderer::destroyImGuiTexture(ImTextureID handle) {
    auto ds = reinterpret_cast<VkDescriptorSet>(handle);
    auto it = imguiUserTextures.find(ds);
    if (it == imguiUserTextures.end()) return;
    uiLayer.unregisterTexture(ds);
    destroyTextureResource(it->second);
    imguiUserTextures.erase(it);
}
