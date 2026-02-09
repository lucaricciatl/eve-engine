#pragma once

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

#include <imgui.h>

#include "engine/GameEngine.hpp"
#include "engine/InputManager.hpp"
#include "engine/IRenderer.hpp"
#include "engine/assets/MeshLoader.hpp"
#include "engine/assets/TextureLoader.hpp"
#include "core/sky/Sky.hpp"
#include "core/PipelineLibrary.hpp"
#include "ui/ImGuiLayer.hpp"
#include "core/WindowManager.hpp"
#include "core/Vertex.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

using Vertex = vkcore::Vertex;
using ParticleVertex = vkcore::ParticleVertex;
using WindowConfig = vkcore::WindowConfig;
using WindowMode = vkcore::WindowMode;

static_assert(sizeof(ParticleVertex) == sizeof(float) * 10, "ParticleVertex layout mismatch");

struct alignas(16) CameraBufferObject {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 lightViewProj;
	glm::mat4 reflectionViewProj;
	glm::vec4 cameraPosition;
	glm::vec4 reflectionPlane;
	glm::vec4 lightPositions[4];
	glm::vec4 lightColors[4];
	glm::vec4 lightDirections[4];
	glm::vec4 lightSpotAngles[4];
	glm::vec4 lightAreaParams[4];
	glm::vec4 lightParams;
	glm::vec4 shadingParams;
	glm::vec4 fogColor;
	glm::vec4 fogParams;
	glm::vec4 blurParams;  // x=enabled, y=radius, z=sigma, w=type (0=gaussian, 1=box, 2=radial)
};

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

class VulkanRenderer : public vkengine::IRenderer {
public:
	struct FogSettings {
		bool enabled{false};
		glm::vec3 color{0.65f, 0.7f, 0.75f};
		float density{0.045f};
		float heightFalloff{0.15f};
		float heightOffset{0.0f};
		float startDistance{0.0f};
	};

	enum class BlurType : int {
		Gaussian = 0,
		Box = 1,
		Radial = 2
	};

	struct BlurSettings {
		bool enabled{false};
		BlurType type{BlurType::Gaussian};
		float radius{3.0f};      // Blur radius (1-10)
		float sigma{2.0f};       // Gaussian sigma
		int sampleCount{16};     // Quality samples for radial
		glm::vec2 center{0.5f, 0.5f}; // Center for radial blur
	};

	explicit VulkanRenderer(vkengine::IGameEngine& engineRef);
	void attachEngine(vkengine::IGameEngine& engineRef) override;
	void run() override;

	vkengine::IGameEngine& getEngine() noexcept { return *engine; }
	const vkengine::IGameEngine& getEngine() const noexcept { return *engine; }

	void setLightAnimationEnabled(bool enabled) noexcept { animateLight = enabled; }
	void setCustomUiCallback(std::function<void(float)> callback) { customUi = std::move(callback); }
	using CursorCallback = std::function<void(double, double)>;
	using MouseButtonCallback = std::function<void(int, int, double, double)>;
	void setCustomCursorCallback(CursorCallback callback);
	void setCustomMouseButtonCallback(MouseButtonCallback callback);
	void setWindowConfig(const WindowConfig& config);
	void setWindowMode(WindowMode mode);
	void setWindowOpacity(float opacity);
	void setSkyColor(const glm::vec3& color);
	void setSkyColor(const glm::vec4& color);
	bool setSkyFromFile(const std::filesystem::path& path);
	void setFogSettings(const FogSettings& settings) noexcept { fogSettings = settings; }
	void setFogEnabled(bool enabled) noexcept { fogSettings.enabled = enabled; }
	void setFogColor(const glm::vec3& color) noexcept { fogSettings.color = color; }
	void setFogDensity(float density) noexcept { fogSettings.density = density; }
	void setFogHeightFalloff(float falloff) noexcept { fogSettings.heightFalloff = falloff; }
	void setFogHeightOffset(float offset) noexcept { fogSettings.heightOffset = offset; }
	void setFogStartDistance(float distance) noexcept { fogSettings.startDistance = distance; }
	[[nodiscard]] const FogSettings& getFogSettings() const noexcept { return fogSettings; }

	// Blur settings
	void setBlurSettings(const BlurSettings& settings) noexcept { blurSettings = settings; }
	void setBlurEnabled(bool enabled) noexcept { blurSettings.enabled = enabled; }
	void setBlurType(BlurType type) noexcept { blurSettings.type = type; }
	void setBlurRadius(float radius) noexcept { blurSettings.radius = glm::clamp(radius, 1.0f, 10.0f); }
	void setBlurSigma(float sigma) noexcept { blurSettings.sigma = glm::clamp(sigma, 0.5f, 10.0f); }
	void setBlurCenter(const glm::vec2& center) noexcept { blurSettings.center = center; }
	[[nodiscard]] const BlurSettings& getBlurSettings() const noexcept { return blurSettings; }
	[[nodiscard]] BlurSettings& getBlurSettings() noexcept { return blurSettings; }

	struct ConsoleSettings {
		bool enabled{false};
		bool autoScroll{true};
		int maxLines{200};
	};

	void setConsoleEnabled(bool enabled) noexcept { console.enabled = enabled; }
	void setConsoleAutoScroll(bool enabled) noexcept { console.autoScroll = enabled; }
	void setConsoleMaxLines(int lines) noexcept { console.maxLines = std::max(10, lines); }
	void addConsoleLine(std::string line);
	void setConsolePrompt(std::string prompt) { console.prompt = std::move(prompt); }

	enum class ShaderStyle : uint32_t {
		Standard = 0,
		Toon = 1,
		Rim = 2,
		Heat = 3,
		Gradient = 4,
		Wireframe = 5,
		Glow = 6
	};

	void setShaderStyle(ShaderStyle style) noexcept { shaderStyle = style; }
	[[nodiscard]] ShaderStyle getShaderStyle() const noexcept { return shaderStyle; }
	bool renderSingleFrameToJpeg(const std::filesystem::path& outputPath);
	bool renderFrameToJpegAt(const std::filesystem::path& outputPath, uint32_t targetFrame, float fixedDeltaSeconds = 1.0f / 30.0f);
	void setSceneControlsEnabled(bool enabled) noexcept { sceneControlsEnabled = enabled; }
	[[nodiscard]] const WindowConfig& getWindowConfig() const noexcept { return windowConfig; }
	[[nodiscard]] vkcore::WindowManager& windowController() noexcept { return windowManager; }
	[[nodiscard]] const vkcore::WindowManager& windowController() const noexcept { return windowManager; }
	void requestExit();
	[[nodiscard]] glm::uvec2 viewportSize() const noexcept { return {swapChainExtent.width, swapChainExtent.height}; }
	[[nodiscard]] GLFWwindow* getWindow() const noexcept { return window; }

private:
	struct MeshGpuBuffers;
	struct TextureResource;

	static constexpr uint32_t WIDTH = 1280;
	static constexpr uint32_t HEIGHT = 720;
	static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;
	static constexpr uint32_t SHADOW_MAP_DIM = 2048;

	void initWindow();
	void initVulkan();
	void mainLoop();
	void cleanup();

	void recreateSwapChain();
	void cleanupSwapChain();

	void createInstance();
	void setupDebugMessenger();
	void createSurface();
	void pickPhysicalDevice();
	void createLogicalDevice();
	void createSwapChain();
	void createImageViews();
	void createRenderPass();
	void createShadowRenderPass();
	void createReflectionRenderPass();
	void createPostProcessRenderPass();
	void createDescriptorSetLayout();
	void createPipelines();
	void createPostProcessPipelines();
	void createDepthResources();
	void createShadowResources();
	void createReflectionResources();
	void createPostProcessResources();
	void destroyReflectionResources();
	void destroyShadowResources();
	void destroyPostProcessResources();
	void createFramebuffers();
	void createCommandPool();
	void createVertexBuffer();
	void createLineVertexBuffer();
	void createIndexBuffer();
	void createUniformBuffers();
	void createDescriptorPool();
	void createMaterialDescriptorPool();
	void createDescriptorSets();
	void createMaterialResources();
	void destroyMaterialResources();
	void destroyColorTextureCache();
	void destroyMeshCache();
	void destroyTextureCache();
	void destroyTextureResource(TextureResource& texture);
	void createDefaultTexture();
	void createCommandBuffers();
	void createSyncObjects();
	void allocateRenderFinishedSemaphores();
	void destroyRenderFinishedSemaphores();
	void updateDeformableMeshes();
	void initUi();
	void destroyUi();
	void rebuildUi();
	void buildUi(float deltaSeconds);
	void initGpuCollision();
	void ensureDeformableBuffers(VkDeviceSize vertexBufferSize, VkDeviceSize indexBufferSize);
	void destroyDeformableBuffers();

	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, float deltaSeconds);
	void captureSwapchainImage(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void finalizeCapture(uint32_t imageIndex);
	void updateUniformBuffer(uint32_t currentImage);
	void populateCameraBufferObject(const vkengine::Camera& camera,
								const glm::mat4& view,
								const glm::mat4& proj,
								const glm::vec3& cameraPosition,
								const glm::mat4& reflectionViewProj,
								const glm::vec4& reflectionPlane,
								CameraBufferObject& ubo);
	void updateParticleVertexBuffer(float deltaSeconds);
	void ensureCaptureBuffer(uint32_t width, uint32_t height);
	void drawFrame();
	void handleCameraInput(float deltaSeconds);
	void handleHotkeys(int key, int action);
	void cycleVisualizationMode();
	void cycleShaderStyle();
	void printHotkeyHelp() const;

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
					  VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
					 VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
					 VkImage& image, VkDeviceMemory& imageMemory);
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const;
	VkCommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	bool loadSkyTextureForUi(const std::filesystem::path& path);
	void destroySkyTexture();
	void ensureSkyTextureLoaded();
	void ensureParticleVertexCapacity(VkDeviceSize requiredSize);
	void destroyParticleBuffers();
	void destroyCaptureResources();

	MeshGpuBuffers& getOrCreateMesh(const vkengine::RenderComponent& renderComponent);
	TextureResource& getOrCreateTexture(const vkengine::RenderComponent& renderComponent);
	TextureResource& getOrCreateColorTexture(const glm::vec4& color);
	bool shouldUseColorTexture(const vkengine::RenderComponent& renderComponent) const;
	void uploadMeshToGpu(const std::string& cacheKey, const vkengine::MeshData& meshData);
	void uploadTextureToGpu(const std::string& cacheKey, const vkengine::TextureData& textureData, TextureResource& outTexture);
	VkDescriptorSet allocateMaterialDescriptor();
	void writeMaterialDescriptor(VkDescriptorSet descriptorSet, VkImageView imageView);
	std::filesystem::path resolveAssetPath(const std::string& relativePath) const;

	bool checkValidationLayerSupport() const;
	std::vector<const char*> getRequiredExtensions() const;
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDeviceHandle) const;
	bool isDeviceSuitable(VkPhysicalDevice physicalDeviceHandle) const;
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDeviceHandle) const;
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
	VkFormat findDepthFormat() const;
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
								 VkFormatFeatureFlags features) const;
	bool hasStencilComponent(VkFormat format) const;
	VkShaderModule createShaderModule(const std::vector<char>& code) const;

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
	void setupInputCallbacks();
	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
	static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

private:
	struct MeshGpuBuffers {
		VkBuffer vertexBuffer{VK_NULL_HANDLE};
		VkDeviceMemory vertexMemory{VK_NULL_HANDLE};
		VkBuffer indexBuffer{VK_NULL_HANDLE};
		VkDeviceMemory indexMemory{VK_NULL_HANDLE};
		VkIndexType indexType{VK_INDEX_TYPE_UINT32};
		uint32_t indexCount{0};
	};

	struct TextureResource {
		VkImage image{VK_NULL_HANDLE};
		VkDeviceMemory memory{VK_NULL_HANDLE};
		VkImageView view{VK_NULL_HANDLE};
		VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
		uint32_t width{0};
		uint32_t height{0};
	};

	enum class VisualizationMode : uint32_t {
		FinalLit = 0,
		Albedo = 1,
		Normals = 2,
		ShadowFactor = 3,
		FogFactor = 4
	};

	vkcore::WindowManager windowManager{};
	WindowConfig windowConfig{};
	std::atomic<bool> exitRequested{false};
	GLFWwindow* window = nullptr;
	vkcore::sky::Settings skySettings{};
	TextureResource skyTexture{};
	VkDescriptorSet skyTextureId{VK_NULL_HANDLE};
	std::filesystem::path pendingSkyTexturePath{};
	bool skyTextureRequested{false};
	bool skyTextureLoaded{false};

	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;

	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue presentQueue = VK_NULL_HANDLE;

	VkSwapchainKHR swapChain = VK_NULL_HANDLE;
	std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D swapChainExtent{};
	std::vector<VkImageView> swapChainImageViews;
	std::vector<VkFramebuffer> swapChainFramebuffers;

	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkRenderPass shadowRenderPass = VK_NULL_HANDLE;
	VkRenderPass reflectionRenderPass = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSetLayout materialDescriptorSetLayout = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline graphicsPipeline = VK_NULL_HANDLE;
	VkPipeline linePipeline = VK_NULL_HANDLE;
	VkPipeline shadowPipeline = VK_NULL_HANDLE;
	VkPipeline particlePipeline = VK_NULL_HANDLE;
	std::vector<VkPipeline> stylePipelines;
	VkImage depthImage = VK_NULL_HANDLE;
	VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
	VkImageView depthImageView = VK_NULL_HANDLE;
	VkImage shadowImage = VK_NULL_HANDLE;
	VkDeviceMemory shadowImageMemory = VK_NULL_HANDLE;
	VkImageView shadowImageView = VK_NULL_HANDLE;
	VkFramebuffer shadowFramebuffer = VK_NULL_HANDLE;
	VkSampler shadowSampler = VK_NULL_HANDLE;
	bool shadowImageReady = false;
	VkImage reflectionColorImage = VK_NULL_HANDLE;
	VkDeviceMemory reflectionColorMemory = VK_NULL_HANDLE;
	VkImageView reflectionColorView = VK_NULL_HANDLE;
	VkImage reflectionDepthImage = VK_NULL_HANDLE;
	VkDeviceMemory reflectionDepthMemory = VK_NULL_HANDLE;
	VkImageView reflectionDepthView = VK_NULL_HANDLE;
	VkFramebuffer reflectionFramebuffer = VK_NULL_HANDLE;
	TextureResource reflectionTexture{};
	bool reflectionImageReady = false;

	// Post-processing resources
	VkRenderPass postProcessRenderPass = VK_NULL_HANDLE;
	std::vector<VkFramebuffer> postProcessSwapchainFramebuffers; // Color-only framebuffers for final pass
	VkImage offscreenColorImage = VK_NULL_HANDLE;
	VkDeviceMemory offscreenColorMemory = VK_NULL_HANDLE;
	VkImageView offscreenColorView = VK_NULL_HANDLE;
	VkImage offscreenDepthImage = VK_NULL_HANDLE;
	VkDeviceMemory offscreenDepthMemory = VK_NULL_HANDLE;
	VkImageView offscreenDepthView = VK_NULL_HANDLE;
	VkFramebuffer offscreenFramebuffer = VK_NULL_HANDLE;
	VkSampler postProcessSampler = VK_NULL_HANDLE;
	VkDescriptorSetLayout postProcessDescriptorLayout = VK_NULL_HANDLE;
	VkDescriptorPool postProcessDescriptorPool = VK_NULL_HANDLE;
	VkDescriptorSet postProcessDescriptorSet = VK_NULL_HANDLE;
	VkPipelineLayout postProcessPipelineLayout = VK_NULL_HANDLE;
	VkPipeline blurHorizontalPipeline = VK_NULL_HANDLE;
	VkPipeline blurVerticalPipeline = VK_NULL_HANDLE;
	VkPipeline blitPipeline = VK_NULL_HANDLE;
	// Ping-pong buffer for two-pass blur
	VkImage pingPongImage = VK_NULL_HANDLE;
	VkDeviceMemory pingPongMemory = VK_NULL_HANDLE;
	VkImageView pingPongView = VK_NULL_HANDLE;
	VkDescriptorSet pingPongDescriptorSet = VK_NULL_HANDLE;
	VkFramebuffer pingPongFramebuffer = VK_NULL_HANDLE;
	
	// Track if post-process images need layout transition
	bool offscreenImageReady = false;

	VkCommandPool commandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> commandBuffers;

	VkBuffer vertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
	VkBuffer lineVertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory lineVertexBufferMemory = VK_NULL_HANDLE;

	VkBuffer indexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;

	VkBuffer particleVertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory particleVertexBufferMemory = VK_NULL_HANDLE;
	VkDeviceSize particleVertexCapacity = 0;
	uint32_t particleVertexCount = 0;
	std::vector<ParticleVertex> cpuParticleVertices;
	struct ParticleDrawRange {
		uint32_t offset = 0;
		uint32_t count = 0;
	};
	std::array<ParticleDrawRange, vkengine::kParticleShapeCount> particleDrawRanges{};
	struct ParticleMeshInstance {
		glm::mat4 model;
		vkengine::RenderComponent render;
	};
	std::vector<ParticleMeshInstance> particleMeshInstances;

	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;
	std::vector<VkBuffer> reflectionUniformBuffers;
	std::vector<VkDeviceMemory> reflectionUniformBuffersMemory;
	std::vector<void*> reflectionUniformBuffersMapped;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> descriptorSets;
	std::vector<VkDescriptorSet> reflectionDescriptorSets;
	VkDescriptorPool materialDescriptorPool = VK_NULL_HANDLE;
	VkSampler materialSampler = VK_NULL_HANDLE;
	TextureResource defaultTexture{};
	std::unordered_map<uint32_t, TextureResource> colorTextureCache;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	std::vector<VkFence> imagesInFlight;

	VkBuffer captureBuffer = VK_NULL_HANDLE;
	VkDeviceMemory captureBufferMemory = VK_NULL_HANDLE;
	VkDeviceSize captureBufferSize = 0;
	uint32_t captureWidth = 0;
	uint32_t captureHeight = 0;
	bool captureRequested = false;
	bool captureCompleted = false;
	bool captureFailed = false;
	uint32_t captureImageIndex = 0;
	std::filesystem::path capturePath;

	size_t currentFrame = 0;
	bool framebufferResized = false;

	std::chrono::steady_clock::time_point startTime;
	std::chrono::steady_clock::time_point lastFrameTime;

	VisualizationMode visualizationMode = VisualizationMode::FinalLit;
	bool shadowsEnabled = true;
	bool specularEnabled = true;
	bool animateLight = true;
	ShaderStyle shaderStyle = ShaderStyle::Standard;
	glm::mat4 cachedReflectionView{1.0f};
	glm::mat4 cachedReflectionProj{1.0f};
	glm::mat4 cachedReflectionViewProj{1.0f};
	glm::vec4 cachedReflectionPlane{0.0f, 1.0f, 0.0f, 0.0f};
	glm::vec3 cachedReflectionCameraPosition{0.0f};
	bool mirrorAvailable{false};

	vkui::ImGuiLayer uiLayer{};
	bool uiEnabled{true};
	bool sceneControlsEnabled{true};
	bool showUiDemo{false};
	float uiScale{1.0f};
	float smoothedFps{0.0f};
	std::function<void(float)> customUi{};
	FogSettings fogSettings{};
	BlurSettings blurSettings{};
	struct ConsoleState {
		bool enabled{false};
		bool autoScroll{true};
		int maxLines{200};
		std::string prompt{"command"};
		std::string input{};
		std::vector<std::string> lines;
		bool scrollToBottom{false};
	} console{};

	vkengine::IGameEngine* engine = nullptr;
	vkengine::InputManager inputManager{};
	std::filesystem::path projectRootPath{};
	CursorCallback customCursorCallback{};
	MouseButtonCallback customMouseButtonCallback{};

	struct DeformableDraw {
		uint32_t indexCount{0};
		uint32_t indexOffset{0};
		uint32_t vertexOffset{0};
		glm::mat4 model{};
	};
	std::vector<DeformableDraw> deformableDraws;
	std::vector<Vertex> deformableVertexScratch;
	std::vector<uint32_t> deformableIndexScratch;

	VkBuffer deformableVertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory deformableVertexMemory = VK_NULL_HANDLE;
	VkDeviceSize deformableVertexCapacity = 0;
	VkBuffer deformableIndexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory deformableIndexMemory = VK_NULL_HANDLE;
	VkDeviceSize deformableIndexCapacity = 0;
	uint32_t deformableIndexCount = 0;


	std::unordered_map<std::string, MeshGpuBuffers> meshCache;
	std::unordered_map<std::string, TextureResource> textureCache;
};

