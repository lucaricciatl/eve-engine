#pragma once

#include <glm/glm.hpp>

namespace vkcore {

struct alignas(16) ObjectPushConstants {
    glm::mat4 model;
    glm::vec4 baseColor;
    glm::vec4 materialParams;
    glm::vec4 emissiveParams;
};

} // namespace vkcore
