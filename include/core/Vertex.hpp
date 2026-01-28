#pragma once

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

#include <array>
#include <glm/glm.hpp>

namespace vkcore {

struct Vertex {
    glm::vec3 pos;
    glm::vec4 color;
    glm::vec3 normal;
    glm::vec2 uv{0.0f};

    static VkVertexInputBindingDescription bindingDescription();
    static std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions();
};

struct ParticleVertex {
    glm::vec3 pos;
    glm::vec2 uv;
    glm::vec4 color;
    float shape{0.0f};

    static VkVertexInputBindingDescription bindingDescription();
    static std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions();
};

} // namespace vkcore
