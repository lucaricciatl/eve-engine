#pragma once

#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif

#include <glm/glm.hpp>

namespace vkengine {

inline constexpr int kMaxInputKeys = 512;

struct CameraInput {
    glm::vec3 translation{0.0f};
    glm::vec2 rotation{0.0f};
    float speedMultiplier{1.0f};
};

} // namespace vkengine
