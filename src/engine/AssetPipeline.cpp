/**
 * @file AssetPipeline.cpp
 * @brief Implementation of asset loading, caching, and management system
 */

#include "engine/AssetPipeline.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <iostream>

namespace vkengine {

// ============================================================================
// TextureAsset Implementation
// ============================================================================

std::size_t TextureAsset::totalSize() const {
    std::size_t size = 0;
    for (const auto& mip : mipLevels) {
        size += mip.data.size();
    }
    return size;
}

// ============================================================================
// MeshAsset Implementation
// ============================================================================

std::size_t MeshAsset::vertexCount() const {
    std::size_t count = 0;
    for (const auto& prim : primitives) {
        count += prim.positions.size();
    }
    return count;
}

std::size_t MeshAsset::indexCount() const {
    std::size_t count = 0;
    for (const auto& prim : primitives) {
        count += prim.indices.size();
    }
    return count;
}

// ============================================================================
// ImageLoader Implementation
// ============================================================================

std::unique_ptr<TextureAsset> ImageLoader::loadPNG(const std::filesystem::path& path) {
    auto texture = std::make_unique<TextureAsset>();
    texture->name = path.stem().string();
    texture->sourcePath = path;
    texture->format = TextureFormat::RGBA8;
    texture->type = TextureType::Texture2D;
    
    // TODO: Implement PNG loading (would use stb_image or libpng)
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return nullptr;
    }
    
    // Placeholder - actual implementation would decode PNG
    texture->width = 1;
    texture->height = 1;
    TextureMipLevel mip0;
    mip0.width = 1;
    mip0.height = 1;
    mip0.data = {255, 255, 255, 255};
    texture->mipLevels.push_back(std::move(mip0));
    
    return texture;
}

std::unique_ptr<TextureAsset> ImageLoader::loadJPEG(const std::filesystem::path& path) {
    auto texture = std::make_unique<TextureAsset>();
    texture->name = path.stem().string();
    texture->sourcePath = path;
    texture->format = TextureFormat::RGB8;
    texture->type = TextureType::Texture2D;
    
    // TODO: Implement JPEG loading
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return nullptr;
    }
    
    texture->width = 1;
    texture->height = 1;
    TextureMipLevel mip0;
    mip0.width = 1;
    mip0.height = 1;
    mip0.data = {255, 255, 255};
    texture->mipLevels.push_back(std::move(mip0));
    
    return texture;
}

std::unique_ptr<TextureAsset> ImageLoader::loadHDR(const std::filesystem::path& path) {
    auto texture = std::make_unique<TextureAsset>();
    texture->name = path.stem().string();
    texture->sourcePath = path;
    texture->format = TextureFormat::RGB32F;
    texture->type = TextureType::Texture2D;
    
    // TODO: Implement HDR loading
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return nullptr;
    }
    
    texture->width = 1;
    texture->height = 1;
    
    return texture;
}

std::unique_ptr<TextureAsset> ImageLoader::loadKTX2(const std::filesystem::path& path) {
    auto texture = std::make_unique<TextureAsset>();
    texture->name = path.stem().string();
    texture->sourcePath = path;
    
    // TODO: Implement KTX2 loading
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return nullptr;
    }
    
    return texture;
}

std::unique_ptr<TextureAsset> ImageLoader::loadDDS(const std::filesystem::path& path) {
    auto texture = std::make_unique<TextureAsset>();
    texture->name = path.stem().string();
    texture->sourcePath = path;
    
    // TODO: Implement DDS loading
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return nullptr;
    }
    
    return texture;
}

std::unique_ptr<TextureAsset> ImageLoader::load(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".png") return loadPNG(path);
    if (ext == ".jpg" || ext == ".jpeg") return loadJPEG(path);
    if (ext == ".hdr") return loadHDR(path);
    if (ext == ".ktx2" || ext == ".ktx") return loadKTX2(path);
    if (ext == ".dds") return loadDDS(path);
    
    return nullptr;
}

void ImageLoader::generateMipmaps(TextureAsset& texture) {
    if (texture.mipLevels.empty()) return;
    
    std::uint32_t width = texture.width;
    std::uint32_t height = texture.height;
    
    while (width > 1 || height > 1) {
        width = std::max(1u, width / 2);
        height = std::max(1u, height / 2);
        
        TextureMipLevel mip;
        mip.width = width;
        mip.height = height;
        // TODO: Actually generate mip data by downsampling
        texture.mipLevels.push_back(std::move(mip));
    }
}

void ImageLoader::convertToFormat(TextureAsset& /*texture*/, TextureFormat /*targetFormat*/) {
    // TODO: Implement format conversion
}

void ImageLoader::flipVertically(TextureAsset& texture) {
    for (auto& mip : texture.mipLevels) {
        if (mip.data.empty() || mip.height == 0) continue;
        
        std::size_t rowSize = mip.data.size() / mip.height;
        std::vector<std::uint8_t> temp(rowSize);
        
        for (std::uint32_t y = 0; y < mip.height / 2; ++y) {
            std::uint8_t* row1 = mip.data.data() + y * rowSize;
            std::uint8_t* row2 = mip.data.data() + (mip.height - 1 - y) * rowSize;
            
            std::memcpy(temp.data(), row1, rowSize);
            std::memcpy(row1, row2, rowSize);
            std::memcpy(row2, temp.data(), rowSize);
        }
    }
}

// ============================================================================
// ModelLoader Implementation
// ============================================================================

std::unique_ptr<ModelAsset> ModelLoader::loadGLTF(const std::filesystem::path& path, const LoadOptions& options) {
    auto model = std::make_unique<ModelAsset>();
    model->name = path.stem().string();
    model->sourcePath = path;
    
    // TODO: Implement glTF loading (would use tinygltf or similar)
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return nullptr;
    }
    
    (void)options;
    return model;
}

std::unique_ptr<ModelAsset> ModelLoader::loadOBJ(const std::filesystem::path& path, const LoadOptions& options) {
    auto model = std::make_unique<ModelAsset>();
    model->name = path.stem().string();
    model->sourcePath = path;
    
    // TODO: Implement OBJ loading
    std::ifstream file(path);
    if (!file.is_open()) {
        return nullptr;
    }
    
    (void)options;
    return model;
}

std::unique_ptr<ModelAsset> ModelLoader::loadFBX(const std::filesystem::path& path, const LoadOptions& options) {
    auto model = std::make_unique<ModelAsset>();
    model->name = path.stem().string();
    model->sourcePath = path;
    
    // TODO: Implement FBX loading
    (void)options;
    return nullptr;
}

std::unique_ptr<ModelAsset> ModelLoader::loadSTL(const std::filesystem::path& path, const LoadOptions& options) {
    auto model = std::make_unique<ModelAsset>();
    model->name = path.stem().string();
    model->sourcePath = path;
    
    // TODO: Implement STL loading
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return nullptr;
    }
    
    (void)options;
    return model;
}

std::unique_ptr<ModelAsset> ModelLoader::load(const std::filesystem::path& path, const LoadOptions& options) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".gltf" || ext == ".glb") return loadGLTF(path, options);
    if (ext == ".obj") return loadOBJ(path, options);
    if (ext == ".fbx") return loadFBX(path, options);
    if (ext == ".stl") return loadSTL(path, options);
    
    return nullptr;
}

// ============================================================================
// AssetManager Implementation
// ============================================================================

AssetManager::AssetManager() = default;

AssetManager::~AssetManager() {
    shutdown();
}

void AssetManager::initialize(std::size_t numThreads) {
    if (running) return;
    
    running = true;
    
    for (std::size_t i = 0; i < numThreads; ++i) {
        workerThreads.emplace_back([this] {
            processLoadQueue();
        });
    }
}

void AssetManager::shutdown() {
    running = false;
    queueCondition.notify_all();
    
    for (auto& thread : workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    workerThreads.clear();
    
    if (fileWatcher.joinable()) {
        fileWatcher.join();
    }
}

void AssetManager::addSearchPath(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(assetMutex);
    if (std::find(searchPaths.begin(), searchPaths.end(), path) == searchPaths.end()) {
        searchPaths.push_back(path);
    }
}

void AssetManager::removeSearchPath(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(assetMutex);
    searchPaths.erase(std::remove(searchPaths.begin(), searchPaths.end(), path), searchPaths.end());
}

std::filesystem::path AssetManager::resolvePath(const std::string& relativePath) const {
    std::lock_guard<std::mutex> lock(assetMutex);
    
    for (const auto& searchPath : searchPaths) {
        auto fullPath = searchPath / relativePath;
        if (std::filesystem::exists(fullPath)) {
            return fullPath;
        }
    }
    
    return relativePath;
}

TextureHandle AssetManager::loadTexture(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(assetMutex);
    
    std::string pathStr = path.string();
    auto it = texturePathMap.find(pathStr);
    if (it != texturePathMap.end()) {
        return TextureHandle(it->second);
    }
    
    auto texture = ImageLoader::load(path);
    if (!texture) {
        return TextureHandle();
    }
    
    std::uint64_t handle = generateHandle();
    TextureEntry entry;
    entry.asset = std::move(texture);
    entry.metadata.name = path.stem().string();
    entry.metadata.path = path;
    entry.metadata.type = AssetType::Texture;
    entry.metadata.state = AssetLoadState::Loaded;
    
    textures.emplace(handle, std::move(entry));
    texturePathMap[pathStr] = handle;
    
    return TextureHandle(handle);
}

MeshHandle AssetManager::loadMesh(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(assetMutex);
    
    std::string pathStr = path.string();
    auto it = meshPathMap.find(pathStr);
    if (it != meshPathMap.end()) {
        return MeshHandle(it->second);
    }
    
    auto model = ModelLoader::load(path);
    if (!model || model->meshes.empty()) {
        return MeshHandle();
    }
    
    std::uint64_t handle = generateHandle();
    MeshEntry entry;
    entry.asset = std::make_unique<MeshAsset>(std::move(model->meshes[0]));
    entry.metadata.name = path.stem().string();
    entry.metadata.path = path;
    entry.metadata.type = AssetType::Mesh;
    entry.metadata.state = AssetLoadState::Loaded;
    
    meshes.emplace(handle, std::move(entry));
    meshPathMap[pathStr] = handle;
    
    return MeshHandle(handle);
}

ModelHandle AssetManager::loadModel(const std::filesystem::path& path, const ModelLoader::LoadOptions& options) {
    std::lock_guard<std::mutex> lock(assetMutex);
    
    std::string pathStr = path.string();
    auto it = modelPathMap.find(pathStr);
    if (it != modelPathMap.end()) {
        return ModelHandle(it->second);
    }
    
    auto model = ModelLoader::load(path, options);
    if (!model) {
        return ModelHandle();
    }
    
    std::uint64_t handle = generateHandle();
    ModelEntry entry;
    entry.asset = std::move(model);
    entry.metadata.name = path.stem().string();
    entry.metadata.path = path;
    entry.metadata.type = AssetType::Scene;
    entry.metadata.state = AssetLoadState::Loaded;
    
    models.emplace(handle, std::move(entry));
    modelPathMap[pathStr] = handle;
    
    return ModelHandle(handle);
}

std::future<TextureHandle> AssetManager::loadTextureAsync(const std::filesystem::path& path) {
    return std::async(std::launch::async, [this, path] {
        return loadTexture(path);
    });
}

std::future<MeshHandle> AssetManager::loadMeshAsync(const std::filesystem::path& path) {
    return std::async(std::launch::async, [this, path] {
        return loadMesh(path);
    });
}

std::future<ModelHandle> AssetManager::loadModelAsync(const std::filesystem::path& path, const ModelLoader::LoadOptions& options) {
    return std::async(std::launch::async, [this, path, options] {
        return loadModel(path, options);
    });
}

TextureAsset* AssetManager::getTexture(TextureHandle handle) {
    std::lock_guard<std::mutex> lock(assetMutex);
    auto it = textures.find(handle.id());
    return it != textures.end() ? it->second.asset.get() : nullptr;
}

const TextureAsset* AssetManager::getTexture(TextureHandle handle) const {
    std::lock_guard<std::mutex> lock(assetMutex);
    auto it = textures.find(handle.id());
    return it != textures.end() ? it->second.asset.get() : nullptr;
}

MeshAsset* AssetManager::getMesh(MeshHandle handle) {
    std::lock_guard<std::mutex> lock(assetMutex);
    auto it = meshes.find(handle.id());
    return it != meshes.end() ? it->second.asset.get() : nullptr;
}

const MeshAsset* AssetManager::getMesh(MeshHandle handle) const {
    std::lock_guard<std::mutex> lock(assetMutex);
    auto it = meshes.find(handle.id());
    return it != meshes.end() ? it->second.asset.get() : nullptr;
}

ModelAsset* AssetManager::getModel(ModelHandle handle) {
    std::lock_guard<std::mutex> lock(assetMutex);
    auto it = models.find(handle.id());
    return it != models.end() ? it->second.asset.get() : nullptr;
}

const ModelAsset* AssetManager::getModel(ModelHandle handle) const {
    std::lock_guard<std::mutex> lock(assetMutex);
    auto it = models.find(handle.id());
    return it != models.end() ? it->second.asset.get() : nullptr;
}

TextureHandle AssetManager::findTexture(const std::string& nameOrPath) const {
    std::lock_guard<std::mutex> lock(assetMutex);
    auto it = texturePathMap.find(nameOrPath);
    if (it != texturePathMap.end()) {
        return TextureHandle(it->second);
    }
    for (const auto& [id, entry] : textures) {
        if (entry.metadata.name == nameOrPath) {
            return TextureHandle(id);
        }
    }
    return TextureHandle();
}

MeshHandle AssetManager::findMesh(const std::string& nameOrPath) const {
    std::lock_guard<std::mutex> lock(assetMutex);
    auto it = meshPathMap.find(nameOrPath);
    if (it != meshPathMap.end()) {
        return MeshHandle(it->second);
    }
    for (const auto& [id, entry] : meshes) {
        if (entry.metadata.name == nameOrPath) {
            return MeshHandle(id);
        }
    }
    return MeshHandle();
}

ModelHandle AssetManager::findModel(const std::string& nameOrPath) const {
    std::lock_guard<std::mutex> lock(assetMutex);
    auto it = modelPathMap.find(nameOrPath);
    if (it != modelPathMap.end()) {
        return ModelHandle(it->second);
    }
    for (const auto& [id, entry] : models) {
        if (entry.metadata.name == nameOrPath) {
            return ModelHandle(id);
        }
    }
    return ModelHandle();
}

AssetLoadState AssetManager::getLoadState(TextureHandle handle) const {
    std::lock_guard<std::mutex> lock(assetMutex);
    auto it = textures.find(handle.id());
    return it != textures.end() ? it->second.metadata.state : AssetLoadState::Unloaded;
}

AssetLoadState AssetManager::getLoadState(MeshHandle handle) const {
    std::lock_guard<std::mutex> lock(assetMutex);
    auto it = meshes.find(handle.id());
    return it != meshes.end() ? it->second.metadata.state : AssetLoadState::Unloaded;
}

AssetLoadState AssetManager::getLoadState(ModelHandle handle) const {
    std::lock_guard<std::mutex> lock(assetMutex);
    auto it = models.find(handle.id());
    return it != models.end() ? it->second.metadata.state : AssetLoadState::Unloaded;
}

void AssetManager::unload(TextureHandle handle) {
    std::lock_guard<std::mutex> lock(assetMutex);
    auto it = textures.find(handle.id());
    if (it != textures.end()) {
        texturePathMap.erase(it->second.metadata.path.string());
        textures.erase(it);
    }
}

void AssetManager::unload(MeshHandle handle) {
    std::lock_guard<std::mutex> lock(assetMutex);
    auto it = meshes.find(handle.id());
    if (it != meshes.end()) {
        meshPathMap.erase(it->second.metadata.path.string());
        meshes.erase(it);
    }
}

void AssetManager::unload(ModelHandle handle) {
    std::lock_guard<std::mutex> lock(assetMutex);
    auto it = models.find(handle.id());
    if (it != models.end()) {
        modelPathMap.erase(it->second.metadata.path.string());
        models.erase(it);
    }
}

void AssetManager::unloadUnused() {
    std::lock_guard<std::mutex> lock(assetMutex);
    
    for (auto it = textures.begin(); it != textures.end();) {
        if (it->second.refCount <= 0) {
            texturePathMap.erase(it->second.metadata.path.string());
            it = textures.erase(it);
        } else {
            ++it;
        }
    }
    
    for (auto it = meshes.begin(); it != meshes.end();) {
        if (it->second.refCount <= 0) {
            meshPathMap.erase(it->second.metadata.path.string());
            it = meshes.erase(it);
        } else {
            ++it;
        }
    }
    
    for (auto it = models.begin(); it != models.end();) {
        if (it->second.refCount <= 0) {
            modelPathMap.erase(it->second.metadata.path.string());
            it = models.erase(it);
        } else {
            ++it;
        }
    }
}

void AssetManager::enableHotReload(bool enabled) {
    hotReloadEnabled = enabled;
    
    if (enabled && !fileWatcher.joinable()) {
        fileWatcher = std::thread([this] {
            fileWatcherThread();
        });
    }
}

void AssetManager::checkForChanges() {
    if (!hotReloadEnabled) return;
    
    std::lock_guard<std::mutex> lock(assetMutex);
    
    for (auto& [handle, entry] : textures) {
        if (std::filesystem::exists(entry.metadata.path)) {
            auto currentTime = std::filesystem::last_write_time(entry.metadata.path);
            if (currentTime > entry.metadata.lastModified) {
                entry.metadata.lastModified = currentTime;
                auto newTexture = ImageLoader::load(entry.metadata.path);
                if (newTexture) {
                    entry.asset = std::move(newTexture);
                    if (reloadCallback) {
                        reloadCallback(entry.metadata.path.string());
                    }
                }
            }
        }
    }
}

void AssetManager::setReloadCallback(ReloadCallback callback) {
    reloadCallback = std::move(callback);
}

AssetManager::Statistics AssetManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(assetMutex);
    
    Statistics stats;
    stats.textureCount = textures.size();
    stats.meshCount = meshes.size();
    stats.modelCount = models.size();
    
    for (const auto& [id, entry] : textures) {
        if (entry.asset) {
            stats.totalMemory += entry.asset->totalSize();
        }
    }
    
    return stats;
}

std::uint64_t AssetManager::generateHandle() {
    return nextHandle++;
}

void AssetManager::processLoadQueue() {
    while (running) {
        std::unique_lock<std::mutex> lock(queueMutex);
        queueCondition.wait(lock, [this] { return !running; });
        // TODO: Process async load requests from queue
    }
}

void AssetManager::fileWatcherThread() {
    while (running && hotReloadEnabled) {
        checkForChanges();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

// ============================================================================
// AssetBundle Implementation
// ============================================================================

void AssetBundle::addTexture(const std::string& name, const TextureAsset& /*texture*/) {
    AssetBundleEntry entry;
    entry.name = name;
    entry.type = AssetType::Texture;
    entry.offset = 0;
    entry.size = 0;
    entries.push_back(entry);
    nameToIndex[name] = entries.size() - 1;
}

void AssetBundle::addMesh(const std::string& name, const MeshAsset& /*mesh*/) {
    AssetBundleEntry entry;
    entry.name = name;
    entry.type = AssetType::Mesh;
    entry.offset = 0;
    entry.size = 0;
    entries.push_back(entry);
    nameToIndex[name] = entries.size() - 1;
}

void AssetBundle::addModel(const std::string& name, const ModelAsset& /*model*/) {
    AssetBundleEntry entry;
    entry.name = name;
    entry.type = AssetType::Scene;
    entry.offset = 0;
    entry.size = 0;
    entries.push_back(entry);
    nameToIndex[name] = entries.size() - 1;
}

bool AssetBundle::save(const std::filesystem::path& path, bool compress) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    header.assetCount = static_cast<std::uint32_t>(entries.size());
    header.compressed = compress;
    
    // Write header
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    // TODO: Write asset table and data
    
    return true;
}

bool AssetBundle::load(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    bundlePath = path;
    
    // Read header
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (std::memcmp(header.magic, "EVEB", 4) != 0) {
        return false;
    }
    
    // TODO: Read asset table
    
    return true;
}

std::vector<std::string> AssetBundle::listAssets() const {
    std::vector<std::string> names;
    names.reserve(entries.size());
    for (const auto& entry : entries) {
        names.push_back(entry.name);
    }
    return names;
}

std::vector<std::string> AssetBundle::listAssets(AssetType type) const {
    std::vector<std::string> names;
    for (const auto& entry : entries) {
        if (entry.type == type) {
            names.push_back(entry.name);
        }
    }
    return names;
}

std::unique_ptr<TextureAsset> AssetBundle::extractTexture(const std::string& name) {
    auto it = nameToIndex.find(name);
    if (it == nameToIndex.end()) {
        return nullptr;
    }
    
    const auto& entry = entries[it->second];
    if (entry.type != AssetType::Texture) {
        return nullptr;
    }
    
    // TODO: Extract and deserialize texture data
    return std::make_unique<TextureAsset>();
}

std::unique_ptr<MeshAsset> AssetBundle::extractMesh(const std::string& name) {
    auto it = nameToIndex.find(name);
    if (it == nameToIndex.end()) {
        return nullptr;
    }
    
    const auto& entry = entries[it->second];
    if (entry.type != AssetType::Mesh) {
        return nullptr;
    }
    
    // TODO: Extract and deserialize mesh data
    return std::make_unique<MeshAsset>();
}

std::unique_ptr<ModelAsset> AssetBundle::extractModel(const std::string& name) {
    auto it = nameToIndex.find(name);
    if (it == nameToIndex.end()) {
        return nullptr;
    }
    
    const auto& entry = entries[it->second];
    if (entry.type != AssetType::Scene) {
        return nullptr;
    }
    
    // TODO: Extract and deserialize model data
    return std::make_unique<ModelAsset>();
}

// ============================================================================
// Global Instance
// ============================================================================

static std::unique_ptr<AssetManager> g_assetManager;

AssetManager& getAssetManager() {
    if (!g_assetManager) {
        g_assetManager = std::make_unique<AssetManager>();
    }
    return *g_assetManager;
}

} // namespace vkengine
