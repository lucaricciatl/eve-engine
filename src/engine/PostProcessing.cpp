/**
 * @file PostProcessing.cpp
 * @brief Implementation of post-processing effects pipeline
 */

#include "engine/PostProcessing.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace vkengine {

// ============================================================================
// Helper Functions
// ============================================================================

namespace {
    constexpr float PI = 3.14159265359f;
    
    float gaussianWeight(float x, float sigma) {
        return std::exp(-(x * x) / (2.0f * sigma * sigma)) / (std::sqrt(2.0f * PI) * sigma);
    }
}

// ============================================================================
// PostProcessPipeline Implementation
// ============================================================================

PostProcessPipeline::PostProcessPipeline() = default;

PostProcessPipeline::~PostProcessPipeline() {
    cleanup();
}

void PostProcessPipeline::initialize(VulkanRenderer& rendererRef) {
    if (initialized) return;
    
    renderer = &rendererRef;
    createRenderTargets();
    initialized = true;
}

void PostProcessPipeline::cleanup() {
    if (!initialized) return;
    
    destroyRenderTargets();
    customEffects.clear();
    initialized = false;
    renderer = nullptr;
}

void PostProcessPipeline::resize(std::uint32_t newWidth, std::uint32_t newHeight) {
    if (width == newWidth && height == newHeight) return;
    
    width = newWidth;
    height = newHeight;
    
    destroyRenderTargets();
    createRenderTargets();
    
    for (auto& effect : customEffects) {
        if (effect) {
            effect->resize(width, height);
        }
    }
}

void PostProcessPipeline::addEffect(std::unique_ptr<PostProcessEffect> effect) {
    if (effect && renderer) {
        effect->initialize(*renderer);
        customEffects.push_back(std::move(effect));
        
        // Sort by render order
        std::sort(customEffects.begin(), customEffects.end(),
                  [](const auto& a, const auto& b) { return a->order() < b->order(); });
    }
}

void PostProcessPipeline::removeEffect(const std::string& name) {
    auto it = std::find_if(customEffects.begin(), customEffects.end(),
                           [&name](const auto& e) { return e->name() == name; });
    if (it != customEffects.end()) {
        (*it)->cleanup();
        customEffects.erase(it);
    }
}

PostProcessEffect* PostProcessPipeline::getEffect(const std::string& name) {
    auto it = std::find_if(customEffects.begin(), customEffects.end(),
                           [&name](const auto& e) { return e->name() == name; });
    return it != customEffects.end() ? it->get() : nullptr;
}

void PostProcessPipeline::render(VulkanRenderer& /*rendererRef*/, RenderTarget& /*sceneColor*/, RenderTarget& /*sceneDepth*/) {
    if (!initialized) return;
    
    // TODO: Implement the full post-processing pipeline
    // 1. SSAO pass (before tonemapping)
    // 2. SSR pass
    // 3. Bloom extraction and blur
    // 4. Depth of field
    // 5. Motion blur
    // 6. Custom effects (BeforeTonemapping)
    // 7. Tonemapping
    // 8. Custom effects (AfterTonemapping)
    // 9. Color grading
    // 10. Anti-aliasing
    // 11. Sharpening
    // 12. Chromatic aberration
    // 13. Vignette
    // 14. Film grain
    // 15. Custom effects (Final)
    // 16. Final composite
}

void PostProcessPipeline::applyPreset(const std::string& name) {
    if (name == "default") {
        defaultPreset(*this);
    } else if (name == "cinematic") {
        cinematicPreset(*this);
    } else if (name == "vintage") {
        vintagePreset(*this);
    } else if (name == "highcontrast") {
        highContrastPreset(*this);
    }
}

void PostProcessPipeline::savePreset(const std::string& name, const std::filesystem::path& path) {
    std::ofstream file(path);
    if (!file.is_open()) return;
    
    // TODO: Serialize all settings to JSON/binary
    file << "preset_name: " << name << "\n";
    file << "tonemapping_exposure: " << toneMappingSettings.exposure << "\n";
    file << "bloom_enabled: " << bloomSettings.enabled << "\n";
    file << "bloom_intensity: " << bloomSettings.intensity << "\n";
    // ... etc
}

void PostProcessPipeline::loadPreset(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;
    
    // TODO: Deserialize settings from file
}

void PostProcessPipeline::defaultPreset(PostProcessPipeline& pipeline) {
    pipeline.toneMappingSettings = ToneMappingSettings{};
    pipeline.bloomSettings = BloomSettings{};
    pipeline.ssaoSettings = SSAOSettings{};
    pipeline.colorGradingSettings = ColorGradingSettings{};
    pipeline.antiAliasingSettings = AntiAliasingSettings{};
}

void PostProcessPipeline::cinematicPreset(PostProcessPipeline& pipeline) {
    defaultPreset(pipeline);
    
    pipeline.toneMappingSettings.algorithm = ToneMappingOperator::ACES;
    pipeline.toneMappingSettings.exposure = 1.2f;
    
    pipeline.bloomSettings.enabled = true;
    pipeline.bloomSettings.intensity = 0.7f;
    pipeline.bloomSettings.threshold = 0.8f;
    
    pipeline.vignetteSettings.enabled = true;
    pipeline.vignetteSettings.intensity = 0.4f;
    
    pipeline.filmGrainSettings.enabled = true;
    pipeline.filmGrainSettings.intensity = 0.05f;
    
    pipeline.colorGradingSettings.contrast = 1.1f;
    pipeline.colorGradingSettings.saturation = 0.95f;
}

void PostProcessPipeline::vintagePreset(PostProcessPipeline& pipeline) {
    defaultPreset(pipeline);
    
    pipeline.toneMappingSettings.algorithm = ToneMappingOperator::Reinhard;
    
    pipeline.colorGradingSettings.saturation = 0.7f;
    pipeline.colorGradingSettings.temperature = 15.0f;
    pipeline.colorGradingSettings.tint = 5.0f;
    
    pipeline.vignetteSettings.enabled = true;
    pipeline.vignetteSettings.intensity = 0.5f;
    
    pipeline.filmGrainSettings.enabled = true;
    pipeline.filmGrainSettings.intensity = 0.15f;
    
    pipeline.chromaticAberrationSettings.enabled = true;
    pipeline.chromaticAberrationSettings.intensity = 0.3f;
}

void PostProcessPipeline::highContrastPreset(PostProcessPipeline& pipeline) {
    defaultPreset(pipeline);
    
    pipeline.toneMappingSettings.algorithm = ToneMappingOperator::ACESApprox;
    pipeline.toneMappingSettings.exposure = 1.0f;
    
    pipeline.colorGradingSettings.contrast = 1.3f;
    pipeline.colorGradingSettings.saturation = 1.1f;
    
    pipeline.sharpeningSettings.enabled = true;
    pipeline.sharpeningSettings.intensity = 0.6f;
}

void PostProcessPipeline::createRenderTargets() {
    // TODO: Create Vulkan render targets for intermediate passes
    // This would use VkImage, VkImageView, VkFramebuffer, etc.
    
    // Create bloom mip chain
    bloomMips.clear();
    std::uint32_t mipWidth = width / 2;
    std::uint32_t mipHeight = height / 2;
    
    while (mipWidth >= 8 && mipHeight >= 8 && bloomMips.size() < 8) {
        RenderTarget mip;
        mip.width = mipWidth;
        mip.height = mipHeight;
        mip.name = "bloom_mip_" + std::to_string(bloomMips.size());
        bloomMips.push_back(mip);
        
        mipWidth /= 2;
        mipHeight /= 2;
    }
    
    // Create SSAO targets
    ssaoTarget.width = width / (ssaoSettings.halfResolution ? 2 : 1);
    ssaoTarget.height = height / (ssaoSettings.halfResolution ? 2 : 1);
    ssaoTarget.name = "ssao";
    
    ssaoBlurTarget.width = ssaoTarget.width;
    ssaoBlurTarget.height = ssaoTarget.height;
    ssaoBlurTarget.name = "ssao_blur";
    
    // Create other intermediate targets
    intermediateTarget1.width = width;
    intermediateTarget1.height = height;
    intermediateTarget1.name = "intermediate1";
    
    intermediateTarget2.width = width;
    intermediateTarget2.height = height;
    intermediateTarget2.name = "intermediate2";
}

void PostProcessPipeline::destroyRenderTargets() {
    // TODO: Destroy Vulkan resources
    bloomMips.clear();
}

void PostProcessPipeline::renderBloom(VulkanRenderer& /*rendererRef*/, RenderTarget& /*input*/) {
    if (!bloomSettings.enabled || bloomMips.empty()) return;
    
    // TODO: Implement bloom rendering
    // 1. Extract bright pixels (threshold)
    // 2. Downsample and blur through mip chain
    // 3. Upsample and accumulate
}

void PostProcessPipeline::renderSSAO(VulkanRenderer& /*rendererRef*/, RenderTarget& /*depth*/, RenderTarget& /*normals*/) {
    if (!ssaoSettings.enabled) return;
    
    // TODO: Implement SSAO rendering
    // 1. Generate noise texture
    // 2. Sample hemisphere around each pixel
    // 3. Blur result
}

void PostProcessPipeline::renderSSR(VulkanRenderer& /*rendererRef*/, RenderTarget& /*color*/, RenderTarget& /*depth*/, RenderTarget& /*normals*/) {
    if (!ssrSettings.enabled) return;
    
    // TODO: Implement SSR rendering
    // 1. Ray march in screen space
    // 2. Sample reflected color
    // 3. Blend with scene based on roughness
}

void PostProcessPipeline::renderDOF(VulkanRenderer& /*rendererRef*/, RenderTarget& /*color*/, RenderTarget& /*depth*/) {
    if (!dofSettings.enabled) return;
    
    // TODO: Implement depth of field
    // 1. Calculate circle of confusion per pixel
    // 2. Separate near/far blur
    // 3. Composite
}

void PostProcessPipeline::renderMotionBlur(VulkanRenderer& /*rendererRef*/, RenderTarget& /*color*/, RenderTarget& /*velocity*/) {
    if (!motionBlurSettings.enabled) return;
    
    // TODO: Implement motion blur
    // 1. Sample along velocity vector
    // 2. Blend samples
}

void PostProcessPipeline::renderToneMapping(VulkanRenderer& /*rendererRef*/, RenderTarget& /*input*/, RenderTarget& /*output*/) {
    // TODO: Apply selected tone mapping operator
    // Supports: Reinhard, ACES, Uncharted2, etc.
}

void PostProcessPipeline::renderColorGrading(VulkanRenderer& /*rendererRef*/, RenderTarget& /*input*/, RenderTarget& /*output*/) {
    if (!colorGradingSettings.enabled) return;
    
    // TODO: Apply color grading
    // 1. White balance
    // 2. Lift/Gamma/Gain
    // 3. Channel mixer
    // 4. LUT application
}

void PostProcessPipeline::renderAntiAliasing(VulkanRenderer& /*rendererRef*/, RenderTarget& /*input*/, RenderTarget& /*output*/) {
    if (antiAliasingSettings.mode == AntiAliasingMode::None) return;
    
    // TODO: Apply selected AA mode
    // FXAA, SMAA, or TAA
}

void PostProcessPipeline::renderFinalPass(VulkanRenderer& /*rendererRef*/, RenderTarget& /*input*/) {
    // TODO: Composite final effects
    // 1. Apply sharpening
    // 2. Apply chromatic aberration
    // 3. Apply vignette
    // 4. Apply film grain
    // 5. Output to swapchain
}

// ============================================================================
// CascadedShadowMap Implementation
// ============================================================================

CascadedShadowMap::CascadedShadowMap() = default;

CascadedShadowMap::~CascadedShadowMap() {
    cleanup();
}

void CascadedShadowMap::initialize(VulkanRenderer& rendererRef) {
    if (initialized) return;
    
    rendererPtr = &rendererRef;
    
    // TODO: Create shadow map array texture
    // TODO: Create framebuffers for each cascade
    // TODO: Create render pass
    // TODO: Create sampler with comparison mode
    
    initialized = true;
}

void CascadedShadowMap::cleanup() {
    if (!initialized) return;
    
    // TODO: Destroy Vulkan resources
    
    initialized = false;
    rendererPtr = nullptr;
}

void CascadedShadowMap::setSettings(const CascadedShadowSettings& settings) {
    shadowSettings = settings;
}

void CascadedShadowMap::updateCascades(const glm::mat4& viewMatrix, const glm::mat4& projMatrix,
                                       const glm::vec3& lightDirection, float nearPlane, float farPlane) {
    calculateCascadeSplits(nearPlane, farPlane);
    
    for (int i = 0; i < shadowSettings.cascadeCount; ++i) {
        calculateCascadeMatrices(viewMatrix, projMatrix, lightDirection, i);
    }
}

void CascadedShadowMap::calculateCascadeSplits(float nearPlane, float farPlane) {
    float clipRange = farPlane - nearPlane;
    float minZ = nearPlane;
    float maxZ = farPlane;
    float range = maxZ - minZ;
    float ratio = maxZ / minZ;
    
    for (int i = 0; i < shadowSettings.cascadeCount; ++i) {
        float p = static_cast<float>(i + 1) / static_cast<float>(shadowSettings.cascadeCount);
        float logSplit = minZ * std::pow(ratio, p);
        float uniformSplit = minZ + range * p;
        float split = shadowSettings.splitLambda * logSplit + (1.0f - shadowSettings.splitLambda) * uniformSplit;
        splitDepths[i] = (split - nearPlane) / clipRange;
    }
}

void CascadedShadowMap::calculateCascadeMatrices(const glm::mat4& viewMatrix, const glm::mat4& projMatrix,
                                                  const glm::vec3& lightDirection, int cascadeIndex) {
    float prevSplitDist = cascadeIndex == 0 ? 0.0f : splitDepths[cascadeIndex - 1];
    float splitDist = splitDepths[cascadeIndex];
    
    // Calculate frustum corners for this cascade
    glm::mat4 invCam = glm::inverse(projMatrix * viewMatrix);
    
    std::vector<glm::vec4> frustumCorners;
    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
            for (int z = 0; z < 2; ++z) {
                glm::vec4 pt = invCam * glm::vec4(
                    2.0f * x - 1.0f,
                    2.0f * y - 1.0f,
                    2.0f * z - 1.0f,
                    1.0f);
                frustumCorners.push_back(pt / pt.w);
            }
        }
    }
    
    // Adjust frustum corners for cascade
    for (int i = 0; i < 4; ++i) {
        glm::vec4 dist = frustumCorners[i + 4] - frustumCorners[i];
        frustumCorners[i + 4] = frustumCorners[i] + dist * splitDist;
        frustumCorners[i] = frustumCorners[i] + dist * prevSplitDist;
    }
    
    cascadeViewMatrices[cascadeIndex] = calculateLightMatrix(frustumCorners, lightDirection);
    
    // Calculate orthographic projection bounds
    glm::vec3 minBounds(std::numeric_limits<float>::max());
    glm::vec3 maxBounds(std::numeric_limits<float>::lowest());
    
    for (const auto& corner : frustumCorners) {
        glm::vec4 lightSpace = cascadeViewMatrices[cascadeIndex] * corner;
        minBounds = glm::min(minBounds, glm::vec3(lightSpace));
        maxBounds = glm::max(maxBounds, glm::vec3(lightSpace));
    }
    
    cascadeProjMatrices[cascadeIndex] = glm::ortho(
        minBounds.x, maxBounds.x,
        minBounds.y, maxBounds.y,
        minBounds.z - 100.0f, maxBounds.z + 100.0f);
}

glm::mat4 CascadedShadowMap::calculateLightMatrix(const std::vector<glm::vec4>& frustumCorners,
                                                   const glm::vec3& lightDirection) {
    // Calculate frustum center
    glm::vec3 center(0.0f);
    for (const auto& corner : frustumCorners) {
        center += glm::vec3(corner);
    }
    center /= static_cast<float>(frustumCorners.size());
    
    glm::vec3 lightPos = center - lightDirection * 100.0f;
    return glm::lookAt(lightPos, center, glm::vec3(0.0f, 1.0f, 0.0f));
}

void CascadedShadowMap::beginCascadePass(VulkanRenderer& /*rendererRef*/, int /*cascadeIndex*/) {
    // TODO: Begin render pass for specified cascade
    // - Bind framebuffer
    // - Set viewport
    // - Clear depth
}

void CascadedShadowMap::endCascadePass(VulkanRenderer& /*rendererRef*/) {
    // TODO: End render pass
}

} // namespace vkengine
