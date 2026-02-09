/**
 * @file Audio.cpp
 * @brief Implementation of audio system components
 */

#include "engine/Audio.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <cstring>

namespace vkengine {

// ============================================================================
// AudioBufferPool Implementation
// ============================================================================

AudioBufferPool::AudioBufferPool(std::size_t initialBuffers, std::size_t bufferSize)
    : singleBufferSize(bufferSize) {
    for (std::size_t i = 0; i < initialBuffers; ++i) {
        auto buffer = std::make_unique<std::uint8_t[]>(bufferSize);
        freeBuffers.push_back(buffer.get());
        allBuffers.push_back(std::move(buffer));
    }
}

AudioBufferPool::~AudioBufferPool() = default;

std::uint8_t* AudioBufferPool::acquire() {
    std::lock_guard<std::mutex> lock(poolMutex);
    
    if (freeBuffers.empty()) {
        auto buffer = std::make_unique<std::uint8_t[]>(singleBufferSize);
        std::uint8_t* ptr = buffer.get();
        allBuffers.push_back(std::move(buffer));
        return ptr;
    }
    
    std::uint8_t* buffer = freeBuffers.back();
    freeBuffers.pop_back();
    return buffer;
}

void AudioBufferPool::release(std::uint8_t* buffer) {
    if (!buffer) return;
    
    std::lock_guard<std::mutex> lock(poolMutex);
    freeBuffers.push_back(buffer);
}

// ============================================================================
// AudioClipLoader Implementation
// ============================================================================

std::unique_ptr<AudioClip> AudioClipLoader::loadWAV(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return nullptr;
    
    auto clip = std::make_unique<AudioClip>();
    clip->path = path;
    clip->name = path.stem().string();
    clip->format = AudioFormat::WAV;
    
    // Read RIFF header
    char riff[4];
    file.read(riff, 4);
    if (std::strncmp(riff, "RIFF", 4) != 0) return nullptr;
    
    std::uint32_t fileSize;
    file.read(reinterpret_cast<char*>(&fileSize), 4);
    
    char wave[4];
    file.read(wave, 4);
    if (std::strncmp(wave, "WAVE", 4) != 0) return nullptr;
    
    // Find fmt chunk
    bool foundFmt = false;
    bool foundData = false;
    
    while (file.good() && (!foundFmt || !foundData)) {
        char chunkId[4];
        std::uint32_t chunkSize;
        file.read(chunkId, 4);
        file.read(reinterpret_cast<char*>(&chunkSize), 4);
        
        if (std::strncmp(chunkId, "fmt ", 4) == 0) {
            std::uint16_t audioFormat;
            file.read(reinterpret_cast<char*>(&audioFormat), 2);
            
            std::uint16_t numChannels;
            file.read(reinterpret_cast<char*>(&numChannels), 2);
            clip->channels = numChannels;
            
            std::uint32_t sampleRate;
            file.read(reinterpret_cast<char*>(&sampleRate), 4);
            clip->sampleRate = sampleRate;
            
            std::uint32_t byteRate;
            file.read(reinterpret_cast<char*>(&byteRate), 4);
            
            std::uint16_t blockAlign;
            file.read(reinterpret_cast<char*>(&blockAlign), 2);
            
            std::uint16_t bitsPerSample;
            file.read(reinterpret_cast<char*>(&bitsPerSample), 2);
            clip->bitsPerSample = bitsPerSample;
            
            // Skip any extra format bytes
            if (chunkSize > 16) {
                file.seekg(chunkSize - 16, std::ios::cur);
            }
            foundFmt = true;
        } else if (std::strncmp(chunkId, "data", 4) == 0) {
            clip->data.resize(chunkSize);
            file.read(reinterpret_cast<char*>(clip->data.data()), chunkSize);
            foundData = true;
        } else {
            file.seekg(chunkSize, std::ios::cur);
        }
    }
    
    if (!foundFmt || !foundData) {
        return nullptr;
    }
    
    // Calculate duration
    std::size_t totalSamples = clip->data.size() / (clip->channels * clip->bitsPerSample / 8);
    clip->duration = static_cast<float>(totalSamples) / static_cast<float>(clip->sampleRate);
    
    return clip;
}

std::unique_ptr<AudioClip> AudioClipLoader::loadOGG(const std::filesystem::path& path) {
    auto clip = std::make_unique<AudioClip>();
    clip->path = path;
    clip->name = path.stem().string();
    clip->format = AudioFormat::OGG;
    
    // TODO: Implement OGG loading (would use stb_vorbis or similar)
    // For now, return a valid but empty clip
    clip->sampleRate = 44100;
    clip->channels = 2;
    clip->bitsPerSample = 16;
    clip->duration = 0.0f;
    
    return clip;
}

std::unique_ptr<AudioClip> AudioClipLoader::load(const std::filesystem::path& path) {
    AudioFormat format = detectFormat(path);
    
    switch (format) {
        case AudioFormat::WAV:
            return loadWAV(path);
        case AudioFormat::OGG:
            return loadOGG(path);
        default:
            return nullptr;
    }
}

AudioFormat AudioClipLoader::detectFormat(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".wav") return AudioFormat::WAV;
    if (ext == ".ogg") return AudioFormat::OGG;
    if (ext == ".mp3") return AudioFormat::MP3;
    if (ext == ".flac") return AudioFormat::FLAC;
    
    return AudioFormat::Unknown;
}

// ============================================================================
// AudioMixer Implementation
// ============================================================================

AudioMixer::AudioMixer(std::uint32_t sampleRate, std::uint32_t channels)
    : outputSampleRate(sampleRate)
    , outputChannels(channels) {
}

AudioMixer::~AudioMixer() = default;

void AudioMixer::setMasterVolume(float volume) {
    masterVolumeValue = std::clamp(volume, 0.0f, 1.0f);
}

void AudioMixer::setGroupVolume(const std::string& group, float volume) {
    std::lock_guard<std::mutex> lock(mixerMutex);
    groupVolumes[group] = std::clamp(volume, 0.0f, 1.0f);
}

float AudioMixer::groupVolume(const std::string& group) const {
    std::lock_guard<std::mutex> lock(mixerMutex);
    auto it = groupVolumes.find(group);
    return it != groupVolumes.end() ? it->second : 1.0f;
}

void AudioMixer::mix(float* outputBuffer, std::size_t frameCount) {
    std::lock_guard<std::mutex> lock(mixerMutex);
    
    // Clear output buffer
    std::memset(outputBuffer, 0, frameCount * outputChannels * sizeof(float));
    
    float master = masterVolumeValue;
    
    for (auto& [handle, channel] : channels) {
        if (channel.state != AudioState::Playing || !channel.clip) continue;
        
        const AudioClip* clip = channel.clip;
        float volume = channel.volume * master;
        
        for (std::size_t frame = 0; frame < frameCount; ++frame) {
            std::size_t sampleIndex = static_cast<std::size_t>(channel.currentSample);
            
            if (sampleIndex >= clip->data.size() / (clip->channels * clip->bitsPerSample / 8)) {
                if (channel.settings.loop) {
                    channel.currentSample = 0.0f;
                    sampleIndex = 0;
                } else {
                    channel.state = AudioState::Finished;
                    break;
                }
            }
            
            // Simple sample reading (16-bit PCM assumed)
            for (std::uint32_t ch = 0; ch < outputChannels; ++ch) {
                std::uint32_t srcCh = ch % clip->channels;
                std::size_t byteOffset = (sampleIndex * clip->channels + srcCh) * 2;
                
                if (byteOffset + 1 < clip->data.size()) {
                    std::int16_t sample = static_cast<std::int16_t>(
                        clip->data[byteOffset] | (clip->data[byteOffset + 1] << 8));
                    float normalized = static_cast<float>(sample) / 32768.0f;
                    outputBuffer[frame * outputChannels + ch] += normalized * volume;
                }
            }
            
            channel.currentSample += channel.settings.pitch;
        }
    }
    
    // Clamp output
    for (std::size_t i = 0; i < frameCount * outputChannels; ++i) {
        outputBuffer[i] = std::clamp(outputBuffer[i], -1.0f, 1.0f);
    }
}

AudioHandle AudioMixer::addChannel(const AudioClip* clip, const AudioSourceSettings& settings) {
    std::lock_guard<std::mutex> lock(mixerMutex);
    
    AudioHandle handle = nextHandle++;
    AudioChannel channel;
    channel.sourceHandle = handle;
    channel.clip = clip;
    channel.settings = settings;
    channel.state = AudioState::Playing;
    channel.currentSample = 0.0f;
    channel.volume = settings.volume;
    
    channels[handle] = channel;
    return handle;
}

void AudioMixer::removeChannel(AudioHandle handle) {
    std::lock_guard<std::mutex> lock(mixerMutex);
    channels.erase(handle);
}

AudioChannel* AudioMixer::getChannel(AudioHandle handle) {
    std::lock_guard<std::mutex> lock(mixerMutex);
    auto it = channels.find(handle);
    return it != channels.end() ? &it->second : nullptr;
}

void AudioMixer::setListenerPosition(const glm::vec3& position) {
    listenerPosition = position;
}

void AudioMixer::setListenerOrientation(const glm::vec3& forward, const glm::vec3& up) {
    listenerForward = forward;
    listenerUp = up;
}

void AudioMixer::setListenerVelocity(const glm::vec3& velocity) {
    listenerVelocity = velocity;
}

float AudioMixer::calculateSpatialVolume(const AudioChannel& channel) const {
    if (!channel.settings.spatial) return channel.settings.volume;
    
    float distance = glm::length(channel.position - listenerPosition);
    
    if (distance <= channel.settings.minDistance) {
        return channel.settings.volume;
    }
    
    if (distance >= channel.settings.maxDistance) {
        return 0.0f;
    }
    
    float attenuation = channel.settings.minDistance / 
        (channel.settings.minDistance + channel.settings.rolloffFactor * 
         (distance - channel.settings.minDistance));
    
    return channel.settings.volume * attenuation;
}

float AudioMixer::calculatePan(const AudioChannel& channel) const {
    if (!channel.settings.spatial) return 0.0f;
    
    glm::vec3 toSource = glm::normalize(channel.position - listenerPosition);
    glm::vec3 right = glm::cross(listenerForward, listenerUp);
    
    return glm::dot(toSource, right);
}

float AudioMixer::calculateDoppler(const AudioChannel& channel) const {
    if (channel.settings.dopplerFactor <= 0.0f) return 1.0f;
    
    constexpr float speedOfSound = 343.0f;
    
    glm::vec3 direction = channel.position - listenerPosition;
    float distance = glm::length(direction);
    if (distance < 0.001f) return 1.0f;
    
    direction /= distance;
    
    float listenerApproach = glm::dot(listenerVelocity, direction);
    float sourceApproach = glm::dot(channel.velocity, direction);
    
    float doppler = (speedOfSound - listenerApproach * channel.settings.dopplerFactor) /
                    (speedOfSound + sourceApproach * channel.settings.dopplerFactor);
    
    return std::clamp(doppler, 0.5f, 2.0f);
}

// ============================================================================
// AudioEngine Implementation
// ============================================================================

AudioEngine::AudioEngine()
    : audioMixer(44100, 2)
    , bufferPool(32, 4096) {
}

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::initialize(std::uint32_t rate, std::uint32_t buffer) {
    if (initialized) return true;
    
    sampleRate = rate;
    bufferSize = buffer;
    
    // TODO: Initialize platform-specific audio device
    // This would use WASAPI on Windows, CoreAudio on macOS, etc.
    
    initialized = true;
    return true;
}

void AudioEngine::shutdown() {
    if (!initialized) return;
    
    // TODO: Cleanup platform-specific audio device
    
    clips.clear();
    clipNameMap.clear();
    initialized = false;
}

AudioHandle AudioEngine::loadClip(const std::filesystem::path& path, bool streaming) {
    std::lock_guard<std::mutex> lock(engineMutex);
    
    auto clip = AudioClipLoader::load(path);
    if (!clip) return InvalidAudioHandle;
    
    clip->streaming = streaming;
    AudioHandle handle = nextClipHandle++;
    
    clips[handle] = std::move(clip);
    return handle;
}

AudioHandle AudioEngine::loadClip(const std::string& name, const std::filesystem::path& path, bool streaming) {
    AudioHandle handle = loadClip(path, streaming);
    if (handle != InvalidAudioHandle) {
        std::lock_guard<std::mutex> lock(engineMutex);
        clips[handle]->name = name;
        clipNameMap[name] = handle;
    }
    return handle;
}

void AudioEngine::unloadClip(AudioHandle handle) {
    std::lock_guard<std::mutex> lock(engineMutex);
    
    auto it = clips.find(handle);
    if (it != clips.end()) {
        // Remove from name map if present
        for (auto nameIt = clipNameMap.begin(); nameIt != clipNameMap.end();) {
            if (nameIt->second == handle) {
                nameIt = clipNameMap.erase(nameIt);
            } else {
                ++nameIt;
            }
        }
        clips.erase(it);
    }
}

void AudioEngine::unloadClip(const std::string& name) {
    std::lock_guard<std::mutex> lock(engineMutex);
    
    auto it = clipNameMap.find(name);
    if (it != clipNameMap.end()) {
        clips.erase(it->second);
        clipNameMap.erase(it);
    }
}

const AudioClip* AudioEngine::getClip(AudioHandle handle) const {
    std::lock_guard<std::mutex> lock(engineMutex);
    auto it = clips.find(handle);
    return it != clips.end() ? it->second.get() : nullptr;
}

const AudioClip* AudioEngine::getClip(const std::string& name) const {
    std::lock_guard<std::mutex> lock(engineMutex);
    auto it = clipNameMap.find(name);
    if (it != clipNameMap.end()) {
        auto clipIt = clips.find(it->second);
        return clipIt != clips.end() ? clipIt->second.get() : nullptr;
    }
    return nullptr;
}

AudioHandle AudioEngine::play(AudioHandle clipHandle, const AudioSourceSettings& settings) {
    const AudioClip* clip = getClip(clipHandle);
    if (!clip) return InvalidAudioHandle;
    
    return audioMixer.addChannel(clip, settings);
}

AudioHandle AudioEngine::play(const std::string& clipName, const AudioSourceSettings& settings) {
    const AudioClip* clip = getClip(clipName);
    if (!clip) return InvalidAudioHandle;
    
    return audioMixer.addChannel(clip, settings);
}

void AudioEngine::stop(AudioHandle playbackHandle) {
    AudioChannel* channel = audioMixer.getChannel(playbackHandle);
    if (channel) {
        channel->state = AudioState::Stopped;
    }
}

void AudioEngine::pause(AudioHandle playbackHandle) {
    AudioChannel* channel = audioMixer.getChannel(playbackHandle);
    if (channel && channel->state == AudioState::Playing) {
        channel->state = AudioState::Paused;
    }
}

void AudioEngine::resume(AudioHandle playbackHandle) {
    AudioChannel* channel = audioMixer.getChannel(playbackHandle);
    if (channel && channel->state == AudioState::Paused) {
        channel->state = AudioState::Playing;
    }
}

void AudioEngine::setVolume(AudioHandle playbackHandle, float volume) {
    AudioChannel* channel = audioMixer.getChannel(playbackHandle);
    if (channel) {
        channel->settings.volume = std::clamp(volume, 0.0f, 1.0f);
    }
}

void AudioEngine::setPitch(AudioHandle playbackHandle, float pitch) {
    AudioChannel* channel = audioMixer.getChannel(playbackHandle);
    if (channel) {
        channel->settings.pitch = std::clamp(pitch, 0.1f, 4.0f);
    }
}

AudioState AudioEngine::getState(AudioHandle playbackHandle) const {
    AudioChannel* channel = const_cast<AudioMixer&>(audioMixer).getChannel(playbackHandle);
    return channel ? channel->state : AudioState::Stopped;
}

float AudioEngine::getCurrentTime(AudioHandle playbackHandle) const {
    AudioChannel* channel = const_cast<AudioMixer&>(audioMixer).getChannel(playbackHandle);
    if (channel && channel->clip) {
        return channel->currentSample / static_cast<float>(channel->clip->sampleRate);
    }
    return 0.0f;
}

void AudioEngine::seek(AudioHandle playbackHandle, float timeSeconds) {
    AudioChannel* channel = audioMixer.getChannel(playbackHandle);
    if (channel && channel->clip) {
        channel->currentSample = timeSeconds * static_cast<float>(channel->clip->sampleRate);
    }
}

AudioHandle AudioEngine::play3D(AudioHandle clipHandle, const glm::vec3& position, const AudioSourceSettings& settings) {
    AudioSourceSettings spatialSettings = settings;
    spatialSettings.spatial = true;
    
    AudioHandle handle = play(clipHandle, spatialSettings);
    if (handle != InvalidAudioHandle) {
        AudioChannel* channel = audioMixer.getChannel(handle);
        if (channel) {
            channel->position = position;
        }
    }
    return handle;
}

AudioHandle AudioEngine::play3D(const std::string& clipName, const glm::vec3& position, const AudioSourceSettings& settings) {
    AudioSourceSettings spatialSettings = settings;
    spatialSettings.spatial = true;
    
    AudioHandle handle = play(clipName, spatialSettings);
    if (handle != InvalidAudioHandle) {
        AudioChannel* channel = audioMixer.getChannel(handle);
        if (channel) {
            channel->position = position;
        }
    }
    return handle;
}

void AudioEngine::setPosition(AudioHandle playbackHandle, const glm::vec3& position) {
    AudioChannel* channel = audioMixer.getChannel(playbackHandle);
    if (channel) {
        channel->position = position;
    }
}

void AudioEngine::setVelocity(AudioHandle playbackHandle, const glm::vec3& velocity) {
    AudioChannel* channel = audioMixer.getChannel(playbackHandle);
    if (channel) {
        channel->velocity = velocity;
    }
}

void AudioEngine::setDirection(AudioHandle playbackHandle, const glm::vec3& /*direction*/) {
    AudioChannel* channel = audioMixer.getChannel(playbackHandle);
    if (channel) {
        // TODO: Implement directional audio
    }
}

void AudioEngine::setListenerPosition(const glm::vec3& position) {
    audioMixer.setListenerPosition(position);
}

void AudioEngine::setListenerOrientation(const glm::vec3& forward, const glm::vec3& up) {
    audioMixer.setListenerOrientation(forward, up);
}

void AudioEngine::setListenerVelocity(const glm::vec3& velocity) {
    audioMixer.setListenerVelocity(velocity);
}

void AudioEngine::setMasterVolume(float volume) {
    audioMixer.setMasterVolume(volume);
}

float AudioEngine::masterVolume() const {
    return audioMixer.masterVolume();
}

void AudioEngine::setGroupVolume(const std::string& group, float volume) {
    audioMixer.setGroupVolume(group, volume);
}

float AudioEngine::groupVolume(const std::string& group) const {
    return audioMixer.groupVolume(group);
}

void AudioEngine::pauseAll() {
    // TODO: Pause all playing channels
}

void AudioEngine::resumeAll() {
    // TODO: Resume all paused channels
}

void AudioEngine::stopAll() {
    // TODO: Stop all channels
}

void AudioEngine::update(float /*deltaSeconds*/) {
    // TODO: Update spatial audio calculations
}

void AudioEngine::updateFromScene(Scene& /*scene*/, float /*deltaSeconds*/) {
    // TODO: Update audio sources from scene components
}

void AudioEngine::audioCallback(float* outputBuffer, std::size_t frameCount) {
    audioMixer.mix(outputBuffer, frameCount);
}

void AudioEngine::staticAudioCallback(void* userData, float* outputBuffer, std::size_t frameCount) {
    auto* engine = static_cast<AudioEngine*>(userData);
    engine->audioCallback(outputBuffer, frameCount);
}

// ============================================================================
// AudioSystem Implementation
// ============================================================================

AudioSystem::AudioSystem(AudioEngine& engine) : audioEngine(&engine) {}

void AudioSystem::update(Scene& scene, float deltaSeconds) {
    if (audioEngine) {
        audioEngine->updateFromScene(scene, deltaSeconds);
    }
}

// ============================================================================
// Global Instance
// ============================================================================

static std::unique_ptr<AudioEngine> g_audioEngine;

AudioEngine& getGlobalAudioEngine() {
    if (!g_audioEngine) {
        g_audioEngine = std::make_unique<AudioEngine>();
    }
    return *g_audioEngine;
}

} // namespace vkengine
