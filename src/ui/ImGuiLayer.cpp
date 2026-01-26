#include "ui/ImGuiLayer.hpp"
#include "ui/FontLibrary.hpp"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

namespace vkui {

void ImGuiLayer::initialize(GLFWwindow* window,
                            VkInstance instance,
                            VkPhysicalDevice physicalDevice,
                            VkDevice device,
                            uint32_t graphicsQueueFamily,
                            VkQueue graphicsQueue,
                            VkRenderPass renderPass,
                            uint32_t minImageCount,
                            uint32_t imageCount)
{
    windowHandle = window;
    vkInstance = instance;
    vkPhysicalDevice = physicalDevice;
    vkDevice = device;
    vkGraphicsQueueFamily = graphicsQueueFamily;
    vkGraphicsQueue = graphicsQueue;
    uiRenderPass = renderPass;
    minSwapchainImageCount = minImageCount;
    swapchainImageCount = imageCount;

    if (initialized) {
        return;
    }

    std::array<VkDescriptorPoolSize, 8> poolSizes{};
    poolSizes[0] = {VK_DESCRIPTOR_TYPE_SAMPLER, 1000};
    poolSizes[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000};
    poolSizes[2] = {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000};
    poolSizes[3] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000};
    poolSizes[4] = {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000};
    poolSizes[5] = {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000};
    poolSizes[6] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000};
    poolSizes[7] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000 * static_cast<uint32_t>(poolSizes.size());
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("ImGui: failed to create descriptor pool");
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    FontLibrary::loadFont(io, fontDirectory, 18.0f);

    if (!ImGui_ImplGlfw_InitForVulkan(windowHandle, true)) {
        throw std::runtime_error("ImGui: failed to init GLFW backend");
    }

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = vkInstance;
    initInfo.PhysicalDevice = vkPhysicalDevice;
    initInfo.Device = vkDevice;
    initInfo.QueueFamily = vkGraphicsQueueFamily;
    initInfo.Queue = vkGraphicsQueue;
    initInfo.DescriptorPool = descriptorPool;
    initInfo.RenderPass = uiRenderPass;
    initInfo.MinImageCount = minSwapchainImageCount;
    initInfo.ImageCount = swapchainImageCount;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        throw std::runtime_error("ImGui: failed to init Vulkan backend");
    }

    // Fonts are auto-uploaded on first frame, but we do it eagerly to avoid hitch
    ImGui_ImplVulkan_CreateFontsTexture();

    initialized = true;
}

void ImGuiLayer::shutdown(VkDevice device)
{
    if (!initialized) {
        return;
    }

    //ImGui_ImplVulkan_Shutdown();
    //ImGui_ImplGlfw_Shutdown();
    //ImGui::DestroyContext();

    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }

    initialized = false;
}

void ImGuiLayer::newFrame()
{
    if (!initialized || !visible) {
        return;
    }
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::render(VkCommandBuffer commandBuffer)
{
    if (!initialized || !visible) {
        return;
    }
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void ImGuiLayer::uploadFonts(VkCommandBuffer /*commandBuffer*/)
{
    if (!initialized) {
        return;
    }
    ImGui_ImplVulkan_CreateFontsTexture();
}

void ImGuiLayer::onSwapchainDestroyed()
{
    if (!initialized) {
        return;
    }
    ImGui_ImplVulkan_Shutdown();
    uiRenderPass = VK_NULL_HANDLE;
}

void ImGuiLayer::onSwapchainRecreated(VkRenderPass renderPass, uint32_t minImageCount, uint32_t imageCount)
{
    if (!initialized) {
        return;
    }

    uiRenderPass = renderPass;
    minSwapchainImageCount = minImageCount;
    swapchainImageCount = imageCount;

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = vkInstance;
    initInfo.PhysicalDevice = vkPhysicalDevice;
    initInfo.Device = vkDevice;
    initInfo.QueueFamily = vkGraphicsQueueFamily;
    initInfo.Queue = vkGraphicsQueue;
    initInfo.DescriptorPool = descriptorPool;
    initInfo.RenderPass = uiRenderPass;
    initInfo.MinImageCount = minSwapchainImageCount;
    initInfo.ImageCount = swapchainImageCount;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);
    ImGui_ImplVulkan_CreateFontsTexture();
}

VkDescriptorSet ImGuiLayer::registerTexture(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
{
    if (!initialized) {
        return VK_NULL_HANDLE;
    }
    return ImGui_ImplVulkan_AddTexture(sampler, imageView, imageLayout);
}

void ImGuiLayer::unregisterTexture(VkDescriptorSet textureId)
{
    if (!initialized || textureId == VK_NULL_HANDLE) {
        return;
    }
    ImGui_ImplVulkan_RemoveTexture(textureId);
}

} // namespace vkui
