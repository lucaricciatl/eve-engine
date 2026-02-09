#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace vkengine {

// Forward declarations
class Skeleton;
class AnimationClip;

// ============================================================================
// Asset Types
// ============================================================================

enum class AssetType {
    Unknown,
    Texture,
    Mesh,
    Material,
    Animation,
    Audio,
    Shader,
    Prefab,
    Scene,
    Font
};

enum class AssetLoadState {
    Unloaded,
    Loading,
    Loaded,
    Failed
};

// ============================================================================
// Texture Data (extended)
// ============================================================================

enum class TextureFormat {
    Unknown,
    R8,
    RG8,
    RGB8,
    RGBA8,
    R16F,
    RG16F,
    RGB16F,
    RGBA16F,
    R32F,
    RG32F,
    RGB32F,
    RGBA32F,
    BC1,  // DXT1
    BC3,  // DXT5
    BC5,  // Normal maps
    BC7,  // High quality
    ASTC_4x4,
    ASTC_6x6,
    ASTC_8x8
};

enum class TextureType {
    Texture2D,
    TextureCube,
    Texture2DArray,
    Texture3D
};

struct TextureMipLevel {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t depth{1};
    std::vector<std::uint8_t> data;
};

struct TextureAsset {
    std::string name;
    std::filesystem::path sourcePath;
    TextureType type{TextureType::Texture2D};
    TextureFormat format{TextureFormat::RGBA8};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t depth{1};
    std::uint32_t arrayLayers{1};
    std::vector<TextureMipLevel> mipLevels;
    bool sRGB{true};
    bool generateMips{true};

    [[nodiscard]] std::size_t totalSize() const;
    [[nodiscard]] std::uint32_t mipCount() const { return static_cast<std::uint32_t>(mipLevels.size()); }
};

// ============================================================================
// Mesh Data (extended for glTF support)
// ============================================================================

struct MeshPrimitive {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec4> tangents;
    std::vector<glm::vec2> texCoords0;
    std::vector<glm::vec2> texCoords1;
    std::vector<glm::vec4> colors;
    std::vector<glm::uvec4> joints;   // Bone indices for skinning
    std::vector<glm::vec4> weights;   // Bone weights for skinning
    std::vector<std::uint32_t> indices;
    std::string materialName;
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
};

struct MeshAsset {
    std::string name;
    std::filesystem::path sourcePath;
    std::vector<MeshPrimitive> primitives;
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};

    // Skinning data
    std::shared_ptr<Skeleton> skeleton;
    std::vector<glm::mat4> inverseBindMatrices;

    [[nodiscard]] std::size_t vertexCount() const;
    [[nodiscard]] std::size_t indexCount() const;
    [[nodiscard]] bool hasSkinning() const { return skeleton != nullptr; }
};

// ============================================================================
// Material Asset
// ============================================================================

struct MaterialAsset {
    std::string name;
    glm::vec4 baseColorFactor{1.0f};
    float metallicFactor{0.0f};
    float roughnessFactor{0.6f};
    glm::vec3 emissiveFactor{0.0f};
    float emissiveIntensity{1.0f};
    float alphaCutoff{0.5f};
    bool doubleSided{false};

    enum class AlphaMode { Opaque, Mask, Blend } alphaMode{AlphaMode::Opaque};

    std::string baseColorTexture;
    std::string metallicRoughnessTexture;
    std::string normalTexture;
    std::string occlusionTexture;
    std::string emissiveTexture;

    float normalScale{1.0f};
    float occlusionStrength{1.0f};

    // Extensions
    bool unlit{false};
    float ior{1.5f};  // Index of refraction
    float transmission{0.0f};
    glm::vec3 sheenColor{0.0f};
    float sheenRoughness{0.0f};
    float clearcoat{0.0f};
    float clearcoatRoughness{0.0f};
};

// ============================================================================
// Model Asset (contains meshes, materials, animations)
// ============================================================================

struct ModelNode {
    std::string name;
    glm::vec3 translation{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    std::vector<std::size_t> meshIndices;
    std::vector<std::size_t> childIndices;
    std::int32_t skinIndex{-1};
};

struct ModelAsset {
    std::string name;
    std::filesystem::path sourcePath;
    std::vector<MeshAsset> meshes;
    std::vector<MaterialAsset> materials;
    std::vector<TextureAsset> textures;
    std::vector<ModelNode> nodes;
    std::vector<std::size_t> rootNodeIndices;
    std::shared_ptr<Skeleton> skeleton;
    std::vector<std::shared_ptr<AnimationClip>> animations;
};

// ============================================================================
// Image Loaders
// ============================================================================

class ImageLoader {
public:
    static std::unique_ptr<TextureAsset> loadPNG(const std::filesystem::path& path);
    static std::unique_ptr<TextureAsset> loadJPEG(const std::filesystem::path& path);
    static std::unique_ptr<TextureAsset> loadHDR(const std::filesystem::path& path);
    static std::unique_ptr<TextureAsset> loadKTX2(const std::filesystem::path& path);
    static std::unique_ptr<TextureAsset> loadDDS(const std::filesystem::path& path);

    // Auto-detect format
    static std::unique_ptr<TextureAsset> load(const std::filesystem::path& path);

    // Utility functions
    static void generateMipmaps(TextureAsset& texture);
    static void convertToFormat(TextureAsset& texture, TextureFormat targetFormat);
    static void flipVertically(TextureAsset& texture);
};

// ============================================================================
// Model Loaders
// ============================================================================

class ModelLoader {
public:
    struct LoadOptions {
        bool loadTextures{true};
        bool loadAnimations{true};
        bool generateTangents{true};
        bool flipUVs{false};
        bool flipWindingOrder{false};
        float scaleFactor{1.0f};
        glm::vec3 upAxis{0.0f, 1.0f, 0.0f};
    };

    static std::unique_ptr<ModelAsset> loadGLTF(const std::filesystem::path& path, const LoadOptions& options = {});
    static std::unique_ptr<ModelAsset> loadOBJ(const std::filesystem::path& path, const LoadOptions& options = {});
    static std::unique_ptr<ModelAsset> loadFBX(const std::filesystem::path& path, const LoadOptions& options = {});
    static std::unique_ptr<ModelAsset> loadSTL(const std::filesystem::path& path, const LoadOptions& options = {});

    // Auto-detect format
    static std::unique_ptr<ModelAsset> load(const std::filesystem::path& path, const LoadOptions& options = {});
};

// ============================================================================
// Asset Handle
// ============================================================================

template<typename T>
class AssetHandle {
public:
    AssetHandle() = default;
    explicit AssetHandle(std::uint64_t id) : handleId(id) {}

    [[nodiscard]] bool isValid() const { return handleId != 0; }
    [[nodiscard]] std::uint64_t id() const { return handleId; }

    bool operator==(const AssetHandle& other) const { return handleId == other.handleId; }
    bool operator!=(const AssetHandle& other) const { return handleId != other.handleId; }

    explicit operator bool() const { return isValid(); }

private:
    std::uint64_t handleId{0};
};

using TextureHandle = AssetHandle<TextureAsset>;
using MeshHandle = AssetHandle<MeshAsset>;
using MaterialHandle = AssetHandle<MaterialAsset>;
using ModelHandle = AssetHandle<ModelAsset>;

// ============================================================================
// Asset Database Entry
// ============================================================================

struct AssetMetadata {
    std::string name;
    std::filesystem::path path;
    AssetType type{AssetType::Unknown};
    AssetLoadState state{AssetLoadState::Unloaded};
    std::chrono::file_clock::time_point lastModified;
    std::uint64_t fileSize{0};
    std::string hash;  // For change detection
    std::vector<std::string> dependencies;
    std::vector<std::string> tags;
};

// ============================================================================
// Asset Manager
// ============================================================================

class AssetManager {
public:
    AssetManager();
    ~AssetManager();

    // Non-copyable
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    // Initialization
    void initialize(std::size_t workerThreads = 4);
    void shutdown();

    // Asset directory management
    void addSearchPath(const std::filesystem::path& path);
    void removeSearchPath(const std::filesystem::path& path);
    [[nodiscard]] std::filesystem::path resolvePath(const std::string& relativePath) const;

    // Synchronous loading
    TextureHandle loadTexture(const std::filesystem::path& path);
    MeshHandle loadMesh(const std::filesystem::path& path);
    ModelHandle loadModel(const std::filesystem::path& path, const ModelLoader::LoadOptions& options = {});

    // Asynchronous loading
    std::future<TextureHandle> loadTextureAsync(const std::filesystem::path& path);
    std::future<MeshHandle> loadMeshAsync(const std::filesystem::path& path);
    std::future<ModelHandle> loadModelAsync(const std::filesystem::path& path, const ModelLoader::LoadOptions& options = {});

    // Asset access
    [[nodiscard]] TextureAsset* getTexture(TextureHandle handle);
    [[nodiscard]] const TextureAsset* getTexture(TextureHandle handle) const;
    [[nodiscard]] MeshAsset* getMesh(MeshHandle handle);
    [[nodiscard]] const MeshAsset* getMesh(MeshHandle handle) const;
    [[nodiscard]] ModelAsset* getModel(ModelHandle handle);
    [[nodiscard]] const ModelAsset* getModel(ModelHandle handle) const;

    // Handle lookup by name/path
    [[nodiscard]] TextureHandle findTexture(const std::string& nameOrPath) const;
    [[nodiscard]] MeshHandle findMesh(const std::string& nameOrPath) const;
    [[nodiscard]] ModelHandle findModel(const std::string& nameOrPath) const;

    // Asset state
    [[nodiscard]] AssetLoadState getLoadState(TextureHandle handle) const;
    [[nodiscard]] AssetLoadState getLoadState(MeshHandle handle) const;
    [[nodiscard]] AssetLoadState getLoadState(ModelHandle handle) const;

    // Unloading
    void unload(TextureHandle handle);
    void unload(MeshHandle handle);
    void unload(ModelHandle handle);
    void unloadUnused();  // Unload assets with no references

    // Hot reloading
    void enableHotReload(bool enabled);
    void checkForChanges();  // Call periodically to detect file changes
    using ReloadCallback = std::function<void(const std::string& assetPath)>;
    void setReloadCallback(ReloadCallback callback);

    // Statistics
    struct Statistics {
        std::size_t textureCount{0};
        std::size_t meshCount{0};
        std::size_t modelCount{0};
        std::size_t totalMemory{0};
        std::size_t pendingLoads{0};
    };
    [[nodiscard]] Statistics getStatistics() const;

private:
    struct TextureEntry {
        std::unique_ptr<TextureAsset> asset;
        AssetMetadata metadata;
        std::atomic<int> refCount{0};
        
        TextureEntry() = default;
        TextureEntry(TextureEntry&& other) noexcept
            : asset(std::move(other.asset))
            , metadata(std::move(other.metadata))
            , refCount(other.refCount.load()) {}
        TextureEntry& operator=(TextureEntry&& other) noexcept {
            asset = std::move(other.asset);
            metadata = std::move(other.metadata);
            refCount.store(other.refCount.load());
            return *this;
        }
        TextureEntry(const TextureEntry&) = delete;
        TextureEntry& operator=(const TextureEntry&) = delete;
    };

    struct MeshEntry {
        std::unique_ptr<MeshAsset> asset;
        AssetMetadata metadata;
        std::atomic<int> refCount{0};
        
        MeshEntry() = default;
        MeshEntry(MeshEntry&& other) noexcept
            : asset(std::move(other.asset))
            , metadata(std::move(other.metadata))
            , refCount(other.refCount.load()) {}
        MeshEntry& operator=(MeshEntry&& other) noexcept {
            asset = std::move(other.asset);
            metadata = std::move(other.metadata);
            refCount.store(other.refCount.load());
            return *this;
        }
        MeshEntry(const MeshEntry&) = delete;
        MeshEntry& operator=(const MeshEntry&) = delete;
    };

    struct ModelEntry {
        std::unique_ptr<ModelAsset> asset;
        AssetMetadata metadata;
        std::atomic<int> refCount{0};
        
        ModelEntry() = default;
        ModelEntry(ModelEntry&& other) noexcept
            : asset(std::move(other.asset))
            , metadata(std::move(other.metadata))
            , refCount(other.refCount.load()) {}
        ModelEntry& operator=(ModelEntry&& other) noexcept {
            asset = std::move(other.asset);
            metadata = std::move(other.metadata);
            refCount.store(other.refCount.load());
            return *this;
        }
        ModelEntry(const ModelEntry&) = delete;
        ModelEntry& operator=(const ModelEntry&) = delete;
    };

    std::uint64_t generateHandle();
    void processLoadQueue();
    void fileWatcherThread();

    std::unordered_map<std::uint64_t, TextureEntry> textures;
    std::unordered_map<std::uint64_t, MeshEntry> meshes;
    std::unordered_map<std::uint64_t, ModelEntry> models;

    std::unordered_map<std::string, std::uint64_t> texturePathMap;
    std::unordered_map<std::string, std::uint64_t> meshPathMap;
    std::unordered_map<std::string, std::uint64_t> modelPathMap;

    std::vector<std::filesystem::path> searchPaths;
    std::atomic<std::uint64_t> nextHandle{1};

    // Thread pool for async loading
    std::vector<std::thread> workerThreads;
    std::atomic<bool> running{false};
    std::mutex queueMutex;
    std::condition_variable queueCondition;

    // Hot reloading
    bool hotReloadEnabled{false};
    std::thread fileWatcher;
    std::unordered_map<std::string, std::chrono::file_clock::time_point> watchedFiles;
    ReloadCallback reloadCallback;

    mutable std::mutex assetMutex;
};

// ============================================================================
// Asset Bundle (for packaging)
// ============================================================================

struct AssetBundleHeader {
    char magic[4]{'E', 'V', 'E', 'B'};
    std::uint32_t version{1};
    std::uint32_t assetCount{0};
    std::uint64_t tableOffset{0};
    std::uint64_t dataOffset{0};
    bool compressed{false};
};

struct AssetBundleEntry {
    std::string name;
    AssetType type;
    std::uint64_t offset;
    std::uint64_t size;
    std::uint64_t compressedSize;
    std::string hash;
};

class AssetBundle {
public:
    AssetBundle() = default;

    // Creating bundles
    void addTexture(const std::string& name, const TextureAsset& texture);
    void addMesh(const std::string& name, const MeshAsset& mesh);
    void addModel(const std::string& name, const ModelAsset& model);
    bool save(const std::filesystem::path& path, bool compress = true);

    // Loading bundles
    bool load(const std::filesystem::path& path);
    [[nodiscard]] std::vector<std::string> listAssets() const;
    [[nodiscard]] std::vector<std::string> listAssets(AssetType type) const;

    std::unique_ptr<TextureAsset> extractTexture(const std::string& name);
    std::unique_ptr<MeshAsset> extractMesh(const std::string& name);
    std::unique_ptr<ModelAsset> extractModel(const std::string& name);

private:
    AssetBundleHeader header;
    std::vector<AssetBundleEntry> entries;
    std::filesystem::path bundlePath;
    std::unordered_map<std::string, std::size_t> nameToIndex;
};

// ============================================================================
// Global Asset Manager Instance
// ============================================================================

AssetManager& getAssetManager();

} // namespace vkengine
