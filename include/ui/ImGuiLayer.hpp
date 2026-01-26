#pragma once

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

#include <imgui.h>

#include <cstdint>
#include <filesystem>

struct ImDrawData;

namespace vkui {

class ImGuiLayer {
public:
    ImGuiLayer() = default;

    void initialize(GLFWwindow* window,
                    VkInstance instance,
                    VkPhysicalDevice physicalDevice,
                    VkDevice device,
                    uint32_t graphicsQueueFamily,
                    VkQueue graphicsQueue,
                    VkRenderPass renderPass,
                    uint32_t minImageCount,
                    uint32_t imageCount);

    void shutdown(VkDevice device);

    void newFrame();
    void render(VkCommandBuffer commandBuffer);
    void uploadFonts(VkCommandBuffer commandBuffer);

    void onSwapchainRecreated(VkRenderPass renderPass, uint32_t minImageCount, uint32_t imageCount);
    void onSwapchainDestroyed();

    VkDescriptorSet registerTexture(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout);
    void unregisterTexture(VkDescriptorSet textureId);

    void setVisible(bool enabled) noexcept { visible = enabled; }
    [[nodiscard]] bool isVisible() const noexcept { return visible; }
    [[nodiscard]] bool isInitialized() const noexcept { return initialized; }

    void setFontDirectory(const std::filesystem::path& directory) { fontDirectory = directory; }
    [[nodiscard]] const std::filesystem::path& getFontDirectory() const noexcept { return fontDirectory; }

private:
    GLFWwindow* windowHandle{nullptr};
    VkInstance vkInstance{VK_NULL_HANDLE};
    VkPhysicalDevice vkPhysicalDevice{VK_NULL_HANDLE};
    VkDevice vkDevice{VK_NULL_HANDLE};
    VkQueue vkGraphicsQueue{VK_NULL_HANDLE};
    uint32_t vkGraphicsQueueFamily{0};
    VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
    VkRenderPass uiRenderPass{VK_NULL_HANDLE};
    uint32_t minSwapchainImageCount{2};
    uint32_t swapchainImageCount{0};
    bool visible{true};
    bool initialized{false};
    std::filesystem::path fontDirectory{};
};

} // namespace vkui
