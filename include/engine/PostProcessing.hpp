#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace vkengine {

// Forward declarations
class VulkanRenderer;

// ============================================================================
// Post-Processing Types
// ============================================================================

enum class PostProcessStage {
    BeforeTonemapping,
    AfterTonemapping,
    Final
};

struct RenderTarget {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t format{0};  // VkFormat
    bool depthAttachment{false};
    std::string name;

    // Vulkan handles (managed internally)
    void* image{nullptr};
    void* imageView{nullptr};
    void* memory{nullptr};
    void* framebuffer{nullptr};
    void* sampler{nullptr};
};

// ============================================================================
// Tone Mapping
// ============================================================================

enum class ToneMappingOperator {
    None,
    Reinhard,
    ReinhardExtended,
    ACES,
    ACESApprox,
    Uncharted2,
    Lottes,
    Uchimura
};

struct ToneMappingSettings {
    ToneMappingOperator algorithm{ToneMappingOperator::ACESApprox};
    float exposure{1.0f};
    float gamma{2.2f};
    float whitePoint{11.2f};  // For Reinhard Extended
    bool autoExposure{false};
    float adaptationSpeed{1.0f};
    float minLuminance{0.01f};
    float maxLuminance{10.0f};
};

// ============================================================================
// Bloom
// ============================================================================

struct BloomSettings {
    bool enabled{true};
    float threshold{1.0f};
    float softThreshold{0.5f};
    float intensity{0.5f};
    int iterations{5};
    float radius{1.0f};
    bool anamorphic{false};
    float anamorphicRatio{0.5f};
    glm::vec3 tint{1.0f};
};

// ============================================================================
// Screen-Space Ambient Occlusion (SSAO)
// ============================================================================

struct SSAOSettings {
    bool enabled{true};
    int samples{32};
    float radius{0.5f};
    float bias{0.025f};
    float intensity{1.0f};
    float power{2.0f};
    bool blur{true};
    int blurPasses{2};
    float blurSharpness{4.0f};
    bool halfResolution{true};
};

// ============================================================================
// Screen-Space Reflections (SSR)
// ============================================================================

struct SSRSettings {
    bool enabled{false};
    int maxSteps{64};
    float stepSize{0.1f};
    float thickness{0.1f};
    float maxDistance{100.0f};
    int refinementSteps{4};
    float fadeStart{0.8f};
    float fadeEnd{1.0f};
    float roughnessThreshold{0.5f};
};

// ============================================================================
// Depth of Field
// ============================================================================

enum class DOFQuality {
    Low,
    Medium,
    High,
    Ultra
};

struct DepthOfFieldSettings {
    bool enabled{false};
    DOFQuality quality{DOFQuality::Medium};
    float focusDistance{10.0f};
    float focusRange{5.0f};
    float aperture{5.6f};  // f-stop
    float focalLength{50.0f};  // mm
    float maxBlurSize{10.0f};
    bool autofocus{false};
    glm::vec2 autofocusPoint{0.5f, 0.5f};
    int bokehSamples{64};
    bool bokehShape{false};  // Circular vs polygonal
    int bokehBlades{6};
};

// ============================================================================
// Motion Blur
// ============================================================================

struct MotionBlurSettings {
    bool enabled{false};
    float intensity{1.0f};
    int samples{8};
    float velocityScale{1.0f};
    float maxVelocity{40.0f};
    bool cameraMotionBlur{true};
    bool objectMotionBlur{true};
};

// ============================================================================
// Chromatic Aberration
// ============================================================================

struct ChromaticAberrationSettings {
    bool enabled{false};
    float intensity{0.5f};
    float offset{0.002f};
    bool radial{true};
};

// ============================================================================
// Vignette
// ============================================================================

struct VignetteSettings {
    bool enabled{false};
    float intensity{0.3f};
    float smoothness{0.5f};
    float roundness{1.0f};
    glm::vec2 center{0.5f, 0.5f};
    glm::vec3 color{0.0f};
    bool rounded{true};
};

// ============================================================================
// Film Grain
// ============================================================================

struct FilmGrainSettings {
    bool enabled{false};
    float intensity{0.1f};
    float response{0.8f};
    bool colored{false};
};

// ============================================================================
// Color Grading
// ============================================================================

struct ColorGradingSettings {
    bool enabled{true};

    // White balance
    float temperature{0.0f};  // -100 to 100
    float tint{0.0f};  // -100 to 100

    // Tone
    float saturation{1.0f};
    float contrast{1.0f};
    glm::vec3 lift{0.0f};    // Shadows
    glm::vec3 gamma{1.0f};   // Midtones
    glm::vec3 gain{1.0f};    // Highlights

    // Channel mixer
    glm::vec3 redChannel{1.0f, 0.0f, 0.0f};
    glm::vec3 greenChannel{0.0f, 1.0f, 0.0f};
    glm::vec3 blueChannel{0.0f, 0.0f, 1.0f};

    // LUT
    std::string lutTexture;
    float lutContribution{1.0f};

    // Curves (16 control points each)
    std::vector<glm::vec2> masterCurve;
    std::vector<glm::vec2> redCurve;
    std::vector<glm::vec2> greenCurve;
    std::vector<glm::vec2> blueCurve;
};

// ============================================================================
// Anti-Aliasing
// ============================================================================

enum class AntiAliasingMode {
    None,
    FXAA,
    SMAA,
    TAA
};

struct AntiAliasingSettings {
    AntiAliasingMode mode{AntiAliasingMode::TAA};

    // FXAA settings
    float fxaaQuality{0.75f};

    // TAA settings
    float taaJitterScale{1.0f};
    float taaFeedback{0.95f};
    float taaSharpening{0.25f};
    bool taaMotionVectors{true};
};

// ============================================================================
// Sharpening
// ============================================================================

struct SharpeningSettings {
    bool enabled{false};
    float intensity{0.5f};
    float threshold{0.0f};
};

// ============================================================================
// Cascaded Shadow Maps
// ============================================================================

struct CascadedShadowSettings {
    bool enabled{true};
    int cascadeCount{4};
    std::array<float, 4> cascadeSplits{0.1f, 0.25f, 0.5f, 1.0f};
    float splitLambda{0.95f};  // Logarithmic vs linear split
    std::uint32_t shadowMapSize{2048};
    float depthBias{0.0001f};
    float normalBias{0.001f};
    float pcfRadius{1.5f};
    int pcfSamples{16};
    bool softShadows{true};
    bool stabilizeCascades{true};
    float fadeRange{0.1f};
    bool visualizeCascades{false};
};

// ============================================================================
// Global Illumination
// ============================================================================

struct GlobalIlluminationSettings {
    bool enabled{false};

    // Screen-space GI
    bool ssgiEnabled{false};
    int ssgiSamples{8};
    float ssgiRadius{2.0f};
    float ssgiIntensity{1.0f};

    // Ambient light
    glm::vec3 ambientColor{0.03f};
    float ambientIntensity{1.0f};

    // Environment map
    std::string environmentMap;
    float environmentIntensity{1.0f};
    float environmentRotation{0.0f};

    // Irradiance probes (baked)
    bool useProbes{false};
    std::string probeAtlas;
};

// ============================================================================
// Post-Process Effect Base
// ============================================================================

class PostProcessEffect {
public:
    virtual ~PostProcessEffect() = default;

    [[nodiscard]] const std::string& name() const { return effectName; }
    [[nodiscard]] bool isEnabled() const { return enabled; }
    void setEnabled(bool value) { enabled = value; }
    [[nodiscard]] int order() const { return renderOrder; }
    void setOrder(int value) { renderOrder = value; }
    [[nodiscard]] PostProcessStage stage() const { return effectStage; }

    virtual void initialize(VulkanRenderer& renderer) = 0;
    virtual void cleanup() = 0;
    virtual void resize(std::uint32_t width, std::uint32_t height) = 0;
    virtual void render(VulkanRenderer& renderer, RenderTarget& input, RenderTarget& output) = 0;

protected:
    std::string effectName;
    bool enabled{true};
    int renderOrder{0};
    PostProcessStage effectStage{PostProcessStage::BeforeTonemapping};
};

// ============================================================================
// Post-Process Pipeline
// ============================================================================

class PostProcessPipeline {
public:
    PostProcessPipeline();
    ~PostProcessPipeline();

    // Initialization
    void initialize(VulkanRenderer& renderer);
    void cleanup();
    void resize(std::uint32_t width, std::uint32_t height);

    // Settings
    ToneMappingSettings& toneMapping() { return toneMappingSettings; }
    BloomSettings& bloom() { return bloomSettings; }
    SSAOSettings& ssao() { return ssaoSettings; }
    SSRSettings& ssr() { return ssrSettings; }
    DepthOfFieldSettings& depthOfField() { return dofSettings; }
    MotionBlurSettings& motionBlur() { return motionBlurSettings; }
    ChromaticAberrationSettings& chromaticAberration() { return chromaticAberrationSettings; }
    VignetteSettings& vignette() { return vignetteSettings; }
    FilmGrainSettings& filmGrain() { return filmGrainSettings; }
    ColorGradingSettings& colorGrading() { return colorGradingSettings; }
    AntiAliasingSettings& antiAliasing() { return antiAliasingSettings; }
    SharpeningSettings& sharpening() { return sharpeningSettings; }

    const ToneMappingSettings& toneMapping() const { return toneMappingSettings; }
    const BloomSettings& bloom() const { return bloomSettings; }
    const SSAOSettings& ssao() const { return ssaoSettings; }
    const SSRSettings& ssr() const { return ssrSettings; }
    const DepthOfFieldSettings& depthOfField() const { return dofSettings; }
    const MotionBlurSettings& motionBlur() const { return motionBlurSettings; }
    const ChromaticAberrationSettings& chromaticAberration() const { return chromaticAberrationSettings; }
    const VignetteSettings& vignette() const { return vignetteSettings; }
    const FilmGrainSettings& filmGrain() const { return filmGrainSettings; }
    const ColorGradingSettings& colorGrading() const { return colorGradingSettings; }
    const AntiAliasingSettings& antiAliasing() const { return antiAliasingSettings; }
    const SharpeningSettings& sharpening() const { return sharpeningSettings; }

    // Custom effects
    void addEffect(std::unique_ptr<PostProcessEffect> effect);
    void removeEffect(const std::string& name);
    [[nodiscard]] PostProcessEffect* getEffect(const std::string& name);

    // Rendering
    void render(VulkanRenderer& renderer, RenderTarget& sceneColor, RenderTarget& sceneDepth);

    // Presets
    void applyPreset(const std::string& name);
    void savePreset(const std::string& name, const std::filesystem::path& path);
    void loadPreset(const std::filesystem::path& path);

    static void defaultPreset(PostProcessPipeline& pipeline);
    static void cinematicPreset(PostProcessPipeline& pipeline);
    static void vintagePreset(PostProcessPipeline& pipeline);
    static void highContrastPreset(PostProcessPipeline& pipeline);

private:
    void createRenderTargets();
    void destroyRenderTargets();
    void renderBloom(VulkanRenderer& renderer, RenderTarget& input);
    void renderSSAO(VulkanRenderer& renderer, RenderTarget& depth, RenderTarget& normals);
    void renderSSR(VulkanRenderer& renderer, RenderTarget& color, RenderTarget& depth, RenderTarget& normals);
    void renderDOF(VulkanRenderer& renderer, RenderTarget& color, RenderTarget& depth);
    void renderMotionBlur(VulkanRenderer& renderer, RenderTarget& color, RenderTarget& velocity);
    void renderToneMapping(VulkanRenderer& renderer, RenderTarget& input, RenderTarget& output);
    void renderColorGrading(VulkanRenderer& renderer, RenderTarget& input, RenderTarget& output);
    void renderAntiAliasing(VulkanRenderer& renderer, RenderTarget& input, RenderTarget& output);
    void renderFinalPass(VulkanRenderer& renderer, RenderTarget& input);

    ToneMappingSettings toneMappingSettings;
    BloomSettings bloomSettings;
    SSAOSettings ssaoSettings;
    SSRSettings ssrSettings;
    DepthOfFieldSettings dofSettings;
    MotionBlurSettings motionBlurSettings;
    ChromaticAberrationSettings chromaticAberrationSettings;
    VignetteSettings vignetteSettings;
    FilmGrainSettings filmGrainSettings;
    ColorGradingSettings colorGradingSettings;
    AntiAliasingSettings antiAliasingSettings;
    SharpeningSettings sharpeningSettings;

    std::vector<std::unique_ptr<PostProcessEffect>> customEffects;

    // Render targets
    std::vector<RenderTarget> bloomMips;
    RenderTarget ssaoTarget;
    RenderTarget ssaoBlurTarget;
    RenderTarget ssrTarget;
    RenderTarget dofTarget;
    RenderTarget motionBlurTarget;
    RenderTarget taaHistoryTarget;
    RenderTarget intermediateTarget1;
    RenderTarget intermediateTarget2;

    // LUT textures
    void* colorGradingLUT{nullptr};
    void* defaultLUT{nullptr};

    std::uint32_t width{0};
    std::uint32_t height{0};
    bool initialized{false};
    VulkanRenderer* renderer{nullptr};

    // TAA state
    glm::vec2 jitterOffset{0.0f};
    std::uint32_t frameIndex{0};
    glm::mat4 previousViewProjection{1.0f};

    // Auto exposure
    float currentExposure{1.0f};
    float averageLuminance{0.5f};
};

// ============================================================================
// Cascaded Shadow Map System
// ============================================================================

class CascadedShadowMap {
public:
    CascadedShadowMap();
    ~CascadedShadowMap();

    void initialize(VulkanRenderer& renderer);
    void cleanup();

    void setSettings(const CascadedShadowSettings& settings);
    [[nodiscard]] const CascadedShadowSettings& settings() const { return shadowSettings; }

    // Calculate cascade splits based on camera
    void updateCascades(const glm::mat4& viewMatrix, const glm::mat4& projMatrix,
                        const glm::vec3& lightDirection, float nearPlane, float farPlane);

    // Get matrices for rendering
    [[nodiscard]] const std::array<glm::mat4, 4>& lightViewMatrices() const { return cascadeViewMatrices; }
    [[nodiscard]] const std::array<glm::mat4, 4>& lightProjMatrices() const { return cascadeProjMatrices; }
    [[nodiscard]] const std::array<float, 4>& cascadeSplitDepths() const { return splitDepths; }

    // Render shadow maps
    void beginCascadePass(VulkanRenderer& renderer, int cascadeIndex);
    void endCascadePass(VulkanRenderer& renderer);

    // Get shadow map for sampling
    [[nodiscard]] void* getShadowMapArray() const { return shadowMapArrayView; }
    [[nodiscard]] void* getShadowSampler() const { return shadowSampler; }

private:
    void calculateCascadeSplits(float nearPlane, float farPlane);
    void calculateCascadeMatrices(const glm::mat4& viewMatrix, const glm::mat4& projMatrix,
                                  const glm::vec3& lightDirection, int cascadeIndex);
    glm::mat4 calculateLightMatrix(const std::vector<glm::vec4>& frustumCorners,
                                   const glm::vec3& lightDirection);

    CascadedShadowSettings shadowSettings;

    std::array<glm::mat4, 4> cascadeViewMatrices;
    std::array<glm::mat4, 4> cascadeProjMatrices;
    std::array<float, 4> splitDepths;

    // Vulkan resources
    void* shadowMapArray{nullptr};
    void* shadowMapArrayView{nullptr};
    void* shadowMapMemory{nullptr};
    std::array<void*, 4> cascadeViews{};
    std::array<void*, 4> cascadeFramebuffers{};
    void* shadowSampler{nullptr};
    void* renderPass{nullptr};

    VulkanRenderer* rendererPtr{nullptr};
    bool initialized{false};
};

} // namespace vkengine
