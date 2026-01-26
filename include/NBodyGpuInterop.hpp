#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace vkengine {

struct NBodyParticleState {
    glm::vec4 positionAndMass{0.0f};
    glm::vec4 velocityAndAge{0.0f};
};

struct NBodyGpuParams {
    float gravityConstant{25.0f};
    float softening{0.2f};
    float damping{0.9993f};
    float colorCycleSeconds{8.0f};
    float particleScale{0.04f};
    float centerPullStrength{-0.0025f};
    float centerRadius{100.0f};
    float confinementFalloff{0.005f};
    float maxVelocity{150.0f};
    float maxDistance{400.0f};
    std::uint32_t solverSubsteps{4};
    std::uint32_t padding{0};
};

class INBodyGpuProvider {
public:
    virtual ~INBodyGpuProvider() = default;

    virtual const std::vector<NBodyParticleState>& gpuInitialStates() const noexcept = 0;
    virtual const NBodyGpuParams& gpuSimParams() const noexcept = 0;
};

} // namespace vkengine
