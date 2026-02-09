#pragma once

#include <glm/glm.hpp>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace vkengine {

// Forward declarations
class Scene;
class GameObject;

// ============================================================================
// Audio Types
// ============================================================================

using AudioHandle = std::uint32_t;
constexpr AudioHandle InvalidAudioHandle = 0;

enum class AudioFormat {
    Unknown,
    WAV,
    OGG,
    MP3,
    FLAC
};

enum class AudioState {
    Stopped,
    Playing,
    Paused,
    Finished
};

struct AudioClip {
    std::string name;
    std::filesystem::path path;
    AudioFormat format{AudioFormat::Unknown};
    float duration{0.0f};      // Duration in seconds
    std::uint32_t sampleRate{44100};
    std::uint32_t channels{2};
    std::uint32_t bitsPerSample{16};
    std::vector<std::uint8_t> data;  // Raw PCM data
    bool streaming{false};     // Large files should stream from disk
};

struct AudioSourceSettings {
    float volume{1.0f};
    float pitch{1.0f};
    float minDistance{1.0f};   // Distance at which attenuation begins
    float maxDistance{100.0f}; // Distance at which sound is inaudible
    float rolloffFactor{1.0f}; // How quickly sound attenuates with distance
    bool loop{false};
    bool spatial{true};        // 3D positional audio
    bool playOnAwake{false};   // Auto-play when created
    float dopplerFactor{1.0f}; // Doppler effect strength
    float coneInnerAngle{360.0f};  // Inner cone for directional sounds (degrees)
    float coneOuterAngle{360.0f};  // Outer cone for directional sounds (degrees)
    float coneOuterGain{0.0f};     // Volume outside outer cone
};

// ============================================================================
// Audio Components (for ECS integration)
// ============================================================================

struct AudioSourceComponent {
    AudioHandle clipHandle{InvalidAudioHandle};
    AudioSourceSettings settings;
    AudioState state{AudioState::Stopped};
    float currentTime{0.0f};
    glm::vec3 lastPosition{0.0f};  // For velocity calculation
    glm::vec3 velocity{0.0f};
};

struct AudioListenerComponent {
    float globalVolume{1.0f};
    glm::vec3 lastPosition{0.0f};
    glm::vec3 velocity{0.0f};
    bool active{true};  // Only one listener should be active at a time
};

// ============================================================================
// Audio Buffer Pool (for efficient memory management)
// ============================================================================

class AudioBufferPool {
public:
    AudioBufferPool(std::size_t initialBuffers = 32, std::size_t bufferSize = 4096);
    ~AudioBufferPool();

    std::uint8_t* acquire();
    void release(std::uint8_t* buffer);
    [[nodiscard]] std::size_t bufferSize() const { return singleBufferSize; }

private:
    std::vector<std::unique_ptr<std::uint8_t[]>> allBuffers;
    std::vector<std::uint8_t*> freeBuffers;
    std::size_t singleBufferSize;
    std::mutex poolMutex;
};

// ============================================================================
// Audio Mixer
// ============================================================================

struct AudioChannel {
    AudioHandle sourceHandle{InvalidAudioHandle};
    const AudioClip* clip{nullptr};
    AudioSourceSettings settings;
    AudioState state{AudioState::Stopped};
    float currentSample{0.0f};
    float volume{1.0f};  // Combined volume after spatial calculations
    float pan{0.0f};     // -1 = left, 0 = center, 1 = right
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
};

class AudioMixer {
public:
    AudioMixer(std::uint32_t sampleRate = 44100, std::uint32_t channels = 2);
    ~AudioMixer();

    void setMasterVolume(float volume);
    [[nodiscard]] float masterVolume() const { return masterVolumeValue; }

    void setGroupVolume(const std::string& group, float volume);
    [[nodiscard]] float groupVolume(const std::string& group) const;

    // Mix all active channels into output buffer
    void mix(float* outputBuffer, std::size_t frameCount);

    // Channel management
    AudioHandle addChannel(const AudioClip* clip, const AudioSourceSettings& settings);
    void removeChannel(AudioHandle handle);
    AudioChannel* getChannel(AudioHandle handle);

    // Listener for 3D audio
    void setListenerPosition(const glm::vec3& position);
    void setListenerOrientation(const glm::vec3& forward, const glm::vec3& up);
    void setListenerVelocity(const glm::vec3& velocity);

private:
    float calculateSpatialVolume(const AudioChannel& channel) const;
    float calculatePan(const AudioChannel& channel) const;
    float calculateDoppler(const AudioChannel& channel) const;

    std::unordered_map<AudioHandle, AudioChannel> channels;
    std::unordered_map<std::string, float> groupVolumes;
    AudioHandle nextHandle{1};

    std::uint32_t outputSampleRate;
    std::uint32_t outputChannels;
    std::atomic<float> masterVolumeValue{1.0f};

    glm::vec3 listenerPosition{0.0f};
    glm::vec3 listenerForward{0.0f, 0.0f, -1.0f};
    glm::vec3 listenerUp{0.0f, 1.0f, 0.0f};
    glm::vec3 listenerVelocity{0.0f};

    mutable std::mutex mixerMutex;
};

// ============================================================================
// Audio Clip Loader
// ============================================================================

class AudioClipLoader {
public:
    static std::unique_ptr<AudioClip> loadWAV(const std::filesystem::path& path);
    static std::unique_ptr<AudioClip> loadOGG(const std::filesystem::path& path);
    static std::unique_ptr<AudioClip> load(const std::filesystem::path& path);

    static AudioFormat detectFormat(const std::filesystem::path& path);
};

// ============================================================================
// Audio Engine (main interface)
// ============================================================================

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    // Non-copyable
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // Initialization
    bool initialize(std::uint32_t sampleRate = 44100, std::uint32_t bufferSize = 1024);
    void shutdown();
    [[nodiscard]] bool isInitialized() const { return initialized; }

    // Clip management
    AudioHandle loadClip(const std::filesystem::path& path, bool streaming = false);
    AudioHandle loadClip(const std::string& name, const std::filesystem::path& path, bool streaming = false);
    void unloadClip(AudioHandle handle);
    void unloadClip(const std::string& name);
    [[nodiscard]] const AudioClip* getClip(AudioHandle handle) const;
    [[nodiscard]] const AudioClip* getClip(const std::string& name) const;

    // Playback (non-spatial)
    AudioHandle play(AudioHandle clipHandle, const AudioSourceSettings& settings = {});
    AudioHandle play(const std::string& clipName, const AudioSourceSettings& settings = {});
    void stop(AudioHandle playbackHandle);
    void pause(AudioHandle playbackHandle);
    void resume(AudioHandle playbackHandle);
    void setVolume(AudioHandle playbackHandle, float volume);
    void setPitch(AudioHandle playbackHandle, float pitch);
    [[nodiscard]] AudioState getState(AudioHandle playbackHandle) const;
    [[nodiscard]] float getCurrentTime(AudioHandle playbackHandle) const;
    void seek(AudioHandle playbackHandle, float timeSeconds);

    // 3D spatial audio
    AudioHandle play3D(AudioHandle clipHandle, const glm::vec3& position, const AudioSourceSettings& settings = {});
    AudioHandle play3D(const std::string& clipName, const glm::vec3& position, const AudioSourceSettings& settings = {});
    void setPosition(AudioHandle playbackHandle, const glm::vec3& position);
    void setVelocity(AudioHandle playbackHandle, const glm::vec3& velocity);
    void setDirection(AudioHandle playbackHandle, const glm::vec3& direction);

    // Listener
    void setListenerPosition(const glm::vec3& position);
    void setListenerOrientation(const glm::vec3& forward, const glm::vec3& up);
    void setListenerVelocity(const glm::vec3& velocity);

    // Global controls
    void setMasterVolume(float volume);
    [[nodiscard]] float masterVolume() const;
    void setGroupVolume(const std::string& group, float volume);
    [[nodiscard]] float groupVolume(const std::string& group) const;
    void pauseAll();
    void resumeAll();
    void stopAll();

    // Update (call each frame for spatial audio)
    void update(float deltaSeconds);

    // ECS integration
    void updateFromScene(Scene& scene, float deltaSeconds);

    // Mixer access (for advanced usage)
    [[nodiscard]] AudioMixer& mixer() { return audioMixer; }
    [[nodiscard]] const AudioMixer& mixer() const { return audioMixer; }

private:
    void audioCallback(float* outputBuffer, std::size_t frameCount);
    static void staticAudioCallback(void* userData, float* outputBuffer, std::size_t frameCount);

    bool initialized{false};
    std::uint32_t sampleRate{44100};
    std::uint32_t bufferSize{1024};

    std::unordered_map<AudioHandle, std::unique_ptr<AudioClip>> clips;
    std::unordered_map<std::string, AudioHandle> clipNameMap;
    AudioHandle nextClipHandle{1};

    AudioMixer audioMixer;
    AudioBufferPool bufferPool;

    // Platform-specific audio device handle
    void* deviceHandle{nullptr};

    mutable std::mutex engineMutex;
};

// ============================================================================
// Audio System (for ECS-based audio management)
// ============================================================================

class AudioSystem {
public:
    explicit AudioSystem(AudioEngine& engine);

    void update(Scene& scene, float deltaSeconds);

private:
    AudioEngine* audioEngine;
};

// Global audio engine instance (optional singleton pattern)
AudioEngine& getGlobalAudioEngine();

} // namespace vkengine
