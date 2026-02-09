#include "engine/Animation.hpp"
#include "core/ecs/Components.hpp"

#include <algorithm>
#include <cmath>

namespace vkengine {

// ============================================================================
// AnimationChannel Implementation (template)
// ============================================================================

template<typename T>
void AnimationChannel<T>::addKeyframe(float time, const T& value) {
    Keyframe<T> kf;
    kf.time = time;
    kf.value = value;
    
    // Insert sorted by time
    auto it = std::lower_bound(keyframes.begin(), keyframes.end(), kf,
        [](const Keyframe<T>& a, const Keyframe<T>& b) { return a.time < b.time; });
    keyframes.insert(it, kf);
}

template<typename T>
void AnimationChannel<T>::addKeyframe(float time, const T& value, const T& inTangent, const T& outTangent) {
    Keyframe<T> kf;
    kf.time = time;
    kf.value = value;
    kf.inTangent = inTangent;
    kf.outTangent = outTangent;
    
    auto it = std::lower_bound(keyframes.begin(), keyframes.end(), kf,
        [](const Keyframe<T>& a, const Keyframe<T>& b) { return a.time < b.time; });
    keyframes.insert(it, kf);
}

template<typename T>
T AnimationChannel<T>::sample(float time, InterpolationType interp) const {
    if (keyframes.empty()) return T{};
    if (keyframes.size() == 1) return keyframes[0].value;
    
    // Clamp to range
    if (time <= keyframes.front().time) return keyframes.front().value;
    if (time >= keyframes.back().time) return keyframes.back().value;
    
    // Find surrounding keyframes
    auto it = std::lower_bound(keyframes.begin(), keyframes.end(), time,
        [](const Keyframe<T>& kf, float t) { return kf.time < t; });
    
    if (it == keyframes.begin()) return keyframes.front().value;
    
    const Keyframe<T>& next = *it;
    const Keyframe<T>& prev = *(it - 1);
    
    float t = (time - prev.time) / (next.time - prev.time);
    return interpolate(prev, next, t, interp);
}

template<typename T>
float AnimationChannel<T>::duration() const {
    if (keyframes.empty()) return 0.0f;
    return keyframes.back().time - keyframes.front().time;
}

// Specialization for vec3
template<>
glm::vec3 AnimationChannel<glm::vec3>::interpolate(const Keyframe<glm::vec3>& a, const Keyframe<glm::vec3>& b, float t, InterpolationType interp) const {
    switch (interp) {
        case InterpolationType::Step:
            return a.value;
        case InterpolationType::Linear:
            return glm::mix(a.value, b.value, t);
        case InterpolationType::CubicSpline: {
            float t2 = t * t;
            float t3 = t2 * t;
            float dt = b.time - a.time;
            return (2.0f * t3 - 3.0f * t2 + 1.0f) * a.value +
                   (t3 - 2.0f * t2 + t) * dt * a.outTangent +
                   (-2.0f * t3 + 3.0f * t2) * b.value +
                   (t3 - t2) * dt * b.inTangent;
        }
    }
    return glm::mix(a.value, b.value, t);
}

// Specialization for quat
template<>
glm::quat AnimationChannel<glm::quat>::interpolate(const Keyframe<glm::quat>& a, const Keyframe<glm::quat>& b, float t, InterpolationType interp) const {
    switch (interp) {
        case InterpolationType::Step:
            return a.value;
        case InterpolationType::Linear:
        case InterpolationType::CubicSpline: // Fallback to slerp for quats
            return glm::slerp(a.value, b.value, t);
    }
    return glm::slerp(a.value, b.value, t);
}

// Specialization for float
template<>
float AnimationChannel<float>::interpolate(const Keyframe<float>& a, const Keyframe<float>& b, float t, InterpolationType interp) const {
    switch (interp) {
        case InterpolationType::Step:
            return a.value;
        case InterpolationType::Linear:
            return glm::mix(a.value, b.value, t);
        case InterpolationType::CubicSpline: {
            float t2 = t * t;
            float t3 = t2 * t;
            float dt = b.time - a.time;
            return (2.0f * t3 - 3.0f * t2 + 1.0f) * a.value +
                   (t3 - 2.0f * t2 + t) * dt * a.outTangent +
                   (-2.0f * t3 + 3.0f * t2) * b.value +
                   (t3 - t2) * dt * b.inTangent;
        }
    }
    return glm::mix(a.value, b.value, t);
}

// Explicit template instantiations
template class AnimationChannel<glm::vec3>;
template class AnimationChannel<glm::quat>;
template class AnimationChannel<float>;

// ============================================================================
// Skeleton Implementation
// ============================================================================

BoneIndex Skeleton::addBone(const std::string& name, BoneIndex parent) {
    BoneIndex index = static_cast<BoneIndex>(bones.size());
    
    Bone bone;
    bone.name = name;
    bone.index = index;
    bone.parentIndex = parent;
    
    bones.push_back(bone);
    boneNameMap[name] = index;
    
    globalTransforms.resize(bones.size());
    finalMatrices.resize(bones.size());
    
    return index;
}

BoneIndex Skeleton::findBone(const std::string& name) const {
    auto it = boneNameMap.find(name);
    return it != boneNameMap.end() ? it->second : InvalidBone;
}

Bone* Skeleton::getBone(BoneIndex index) {
    if (index < 0 || index >= static_cast<BoneIndex>(bones.size())) return nullptr;
    return &bones[index];
}

const Bone* Skeleton::getBone(BoneIndex index) const {
    if (index < 0 || index >= static_cast<BoneIndex>(bones.size())) return nullptr;
    return &bones[index];
}

Bone* Skeleton::getBone(const std::string& name) {
    BoneIndex index = findBone(name);
    return getBone(index);
}

const Bone* Skeleton::getBone(const std::string& name) const {
    BoneIndex index = findBone(name);
    return getBone(index);
}

void Skeleton::setBindPose() {
    for (auto& bone : bones) {
        bone.offsetMatrix = glm::inverse(globalTransforms[bone.index]);
    }
}

void Skeleton::calculateGlobalTransforms() {
    globalTransforms.resize(bones.size());
    
    for (std::size_t i = 0; i < bones.size(); ++i) {
        auto& bone = bones[i];
        
        // Compose local transform
        bone.localTransform = glm::translate(glm::mat4(1.0f), bone.localPosition) *
                              glm::mat4_cast(bone.localRotation) *
                              glm::scale(glm::mat4(1.0f), bone.localScale);
        
        // Calculate global transform
        if (bone.parentIndex >= 0 && bone.parentIndex < static_cast<BoneIndex>(bones.size())) {
            globalTransforms[i] = globalTransforms[bone.parentIndex] * bone.localTransform;
        } else {
            globalTransforms[i] = bone.localTransform;
        }
        
        // Calculate final matrix for skinning
        finalMatrices[i] = globalTransforms[i] * bone.offsetMatrix;
    }
}

const glm::mat4* Skeleton::boneMatrixData() const {
    return finalMatrices.empty() ? nullptr : finalMatrices.data();
}

// ============================================================================
// AnimationClip Implementation
// ============================================================================

AnimationClip::AnimationClip(const std::string& clipName)
    : clipName(clipName) {}

BoneAnimationTrack& AnimationClip::addTrack(const std::string& boneName) {
    auto it = trackNameMap.find(boneName);
    if (it != trackNameMap.end()) {
        return tracks[it->second];
    }
    
    BoneAnimationTrack track;
    track.boneName = boneName;
    tracks.push_back(track);
    trackNameMap[boneName] = tracks.size() - 1;
    return tracks.back();
}

BoneAnimationTrack* AnimationClip::getTrack(const std::string& boneName) {
    auto it = trackNameMap.find(boneName);
    return it != trackNameMap.end() ? &tracks[it->second] : nullptr;
}

const BoneAnimationTrack* AnimationClip::getTrack(const std::string& boneName) const {
    auto it = trackNameMap.find(boneName);
    return it != trackNameMap.end() ? &tracks[it->second] : nullptr;
}

void AnimationClip::sample(float time, Skeleton& skeleton, InterpolationType interp) const {
    for (const auto& track : tracks) {
        BoneIndex boneIndex = skeleton.findBone(track.boneName);
        if (boneIndex == InvalidBone) continue;
        
        Bone* bone = skeleton.getBone(boneIndex);
        if (!bone) continue;
        
        // Sample channels
        if (track.positionChannel.keyframeCount() > 0) {
            bone->localPosition = track.positionChannel.sample(time, interp);
        }
        if (track.rotationChannel.keyframeCount() > 0) {
            bone->localRotation = track.rotationChannel.sample(time, interp);
        }
        if (track.scaleChannel.keyframeCount() > 0) {
            bone->localScale = track.scaleChannel.sample(time, interp);
        }
    }
}

// ============================================================================
// AnimationState Implementation
// ============================================================================

void AnimationState::update(float deltaSeconds) {
    if (!playing || !clip) return;
    
    currentTime += deltaSeconds * speed;
    
    float dur = clip->duration();
    if (dur <= 0.0f) return;
    
    switch (wrapMode) {
        case AnimationWrapMode::Once:
            if (currentTime >= dur) {
                currentTime = dur;
                playing = false;
            }
            break;
        case AnimationWrapMode::Loop:
            currentTime = std::fmod(currentTime, dur);
            if (currentTime < 0.0f) currentTime += dur;
            break;
        case AnimationWrapMode::PingPong: {
            float cycles = currentTime / dur;
            int cycleInt = static_cast<int>(cycles);
            float frac = cycles - cycleInt;
            currentTime = (cycleInt % 2 == 0) ? frac * dur : (1.0f - frac) * dur;
            break;
        }
        case AnimationWrapMode::ClampForever:
            if (currentTime >= dur) currentTime = dur;
            if (currentTime < 0.0f) currentTime = 0.0f;
            break;
    }
}

float AnimationState::normalizedTime() const {
    if (!clip || clip->duration() <= 0.0f) return 0.0f;
    return currentTime / clip->duration();
}

// ============================================================================
// AnimationBlendTree Implementation
// ============================================================================

void AnimationBlendTree::setRoot(std::unique_ptr<BlendNode> root) {
    rootNode = std::move(root);
}

void AnimationBlendTree::setParameter(const std::string& name, float value) {
    parameters[name] = value;
}

float AnimationBlendTree::getParameter(const std::string& name) const {
    auto it = parameters.find(name);
    return it != parameters.end() ? it->second : 0.0f;
}

void AnimationBlendTree::evaluate(float time, Skeleton& skeleton) const {
    if (rootNode) {
        evaluateNode(rootNode.get(), time, skeleton, 1.0f);
    }
}

void AnimationBlendTree::evaluateNode(const BlendNode* node, float time, Skeleton& skeleton, float parentWeight) const {
    if (!node) return;
    
    float weight = node->weight * parentWeight;
    
    switch (node->type) {
        case BlendNodeType::Clip:
            if (node->clip && weight > 0.0f) {
                node->clip->sample(time, skeleton);
            }
            break;
            
        case BlendNodeType::Blend1D:
        case BlendNodeType::Blend2D:
        case BlendNodeType::Additive:
            for (const auto& child : node->children) {
                evaluateNode(child.get(), time, skeleton, weight);
            }
            break;
    }
}

// ============================================================================
// AnimationStateMachine Implementation
// ============================================================================

void AnimationStateMachine::addState(const std::string& name, const AnimationClip* clip) {
    State state;
    state.name = name;
    state.clip = clip;
    state.animState.clip = clip;
    states[name] = std::move(state);
    
    if (defaultState.empty()) {
        defaultState = name;
        currentState = name;
    }
}

void AnimationStateMachine::addState(const std::string& name, std::unique_ptr<AnimationBlendTree> blendTree) {
    State state;
    state.name = name;
    state.blendTree = std::move(blendTree);
    states[name] = std::move(state);
    
    if (defaultState.empty()) {
        defaultState = name;
        currentState = name;
    }
}

void AnimationStateMachine::removeState(const std::string& name) {
    states.erase(name);
}

void AnimationStateMachine::addTransition(const AnimationTransition& transition) {
    transitions.push_back(transition);
}

void AnimationStateMachine::setDefaultState(const std::string& name) {
    defaultState = name;
    if (currentState.empty()) {
        currentState = name;
    }
}

void AnimationStateMachine::trigger(const std::string& triggerName) {
    triggers.insert(triggerName);
}

void AnimationStateMachine::setBool(const std::string& name, bool value) {
    boolParams[name] = value;
}

void AnimationStateMachine::setFloat(const std::string& name, float value) {
    floatParams[name] = value;
}

void AnimationStateMachine::setInt(const std::string& name, int value) {
    intParams[name] = value;
}

bool AnimationStateMachine::getBool(const std::string& name) const {
    auto it = boolParams.find(name);
    return it != boolParams.end() ? it->second : false;
}

float AnimationStateMachine::getFloat(const std::string& name) const {
    auto it = floatParams.find(name);
    return it != floatParams.end() ? it->second : 0.0f;
}

int AnimationStateMachine::getInt(const std::string& name) const {
    auto it = intParams.find(name);
    return it != intParams.end() ? it->second : 0;
}

void AnimationStateMachine::update(float deltaSeconds, Skeleton& skeleton) {
    if (currentState.empty()) return;
    
    auto stateIt = states.find(currentState);
    if (stateIt == states.end()) return;
    
    State& state = stateIt->second;
    
    if (inTransition) {
        updateTransition(deltaSeconds, skeleton);
    } else {
        // Update animation state
        state.animState.update(deltaSeconds);
        
        // Sample animation
        if (state.clip) {
            state.clip->sample(state.animState.currentTime, skeleton);
        } else if (state.blendTree) {
            state.blendTree->evaluate(state.animState.currentTime, skeleton);
        }
        
        checkTransitions();
    }
    
    // Clear triggers at end of frame
    triggers.clear();
}

void AnimationStateMachine::checkTransitions() {
    for (const auto& transition : transitions) {
        if (transition.fromState != currentState) continue;
        
        // Check condition callback
        if (transition.condition && !transition.condition()) continue;
        
        // Check exit time
        if (transition.hasExitTime) {
            auto stateIt = states.find(currentState);
            if (stateIt != states.end()) {
                float normalized = stateIt->second.animState.normalizedTime();
                if (normalized < transition.exitTime) continue;
            }
        }
        
        startTransition(transition);
        break;
    }
}

void AnimationStateMachine::startTransition(const AnimationTransition& transition) {
    targetState = transition.toState;
    transitionTime = 0.0f;
    transitionDuration = transition.duration;
    inTransition = true;
}

void AnimationStateMachine::updateTransition(float deltaSeconds, Skeleton& skeleton) {
    transitionTime += deltaSeconds;
    
    if (transitionTime >= transitionDuration) {
        currentState = targetState;
        targetState.clear();
        inTransition = false;
        transitionTime = 0.0f;
        return;
    }
    
    // Blend between states (simplified - just switch)
    auto targetIt = states.find(targetState);
    if (targetIt != states.end()) {
        targetIt->second.animState.update(deltaSeconds);
        
        if (targetIt->second.clip) {
            targetIt->second.clip->sample(targetIt->second.animState.currentTime, skeleton);
        }
    }
}

// ============================================================================
// AnimationSystem Implementation
// ============================================================================

void AnimationSystem::update(Scene& scene, float deltaSeconds) {
    // In a full implementation, would iterate over entities with AnimatorComponent
    // and update their animations
    (void)scene;
    (void)deltaSeconds;
}

void AnimationSystem::addClip(const std::string& name, std::shared_ptr<AnimationClip> clip) {
    clipLibrary[name] = std::move(clip);
}

void AnimationSystem::removeClip(const std::string& name) {
    clipLibrary.erase(name);
}

std::shared_ptr<AnimationClip> AnimationSystem::getClip(const std::string& name) const {
    auto it = clipLibrary.find(name);
    return it != clipLibrary.end() ? it->second : nullptr;
}

bool AnimationSystem::hasClip(const std::string& name) const {
    return clipLibrary.find(name) != clipLibrary.end();
}

// ============================================================================
// AnimationLoader Implementation
// ============================================================================

AnimationLoader::LoadResult AnimationLoader::loadFromGLTF(const std::filesystem::path& path) {
    LoadResult result;
    result.success = false;
    
    // Placeholder - actual implementation would parse glTF files
    if (!std::filesystem::exists(path)) {
        result.errorMessage = "File not found: " + path.string();
        return result;
    }
    
    result.skeleton = std::make_shared<Skeleton>();
    result.success = true;
    return result;
}

AnimationLoader::LoadResult AnimationLoader::loadFromFile(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".gltf" || ext == ".glb") {
        return loadFromGLTF(path);
    }
    
    LoadResult result;
    result.errorMessage = "Unsupported format: " + ext;
    return result;
}

// ============================================================================
// TransformAnimationComponent Implementation
// ============================================================================

void TransformAnimationComponent::play(const TransformAnimation* anim, bool restart) {
    state.animation = anim;
    if (restart) {
        state.currentTime = 0.0f;
    }
    state.playing = true;
}

void TransformAnimationComponent::stop() {
    state.playing = false;
    state.currentTime = 0.0f;
}

void TransformAnimationComponent::pause() {
    state.playing = false;
}

void TransformAnimationComponent::resume() {
    state.playing = true;
}

void TransformAnimationComponent::update(float deltaSeconds, Transform& transform) {
    if (!state.playing || !state.animation) return;
    
    state.currentTime += deltaSeconds * state.speed;
    
    const TransformAnimation* anim = state.animation;
    
    // Handle wrap mode
    if (state.currentTime >= anim->duration) {
        switch (anim->wrapMode) {
            case AnimationWrapMode::Once:
                state.currentTime = anim->duration;
                state.playing = false;
                if (state.onComplete) state.onComplete();
                break;
            case AnimationWrapMode::Loop:
                state.currentTime = std::fmod(state.currentTime, anim->duration);
                break;
            case AnimationWrapMode::PingPong:
            case AnimationWrapMode::ClampForever:
                state.currentTime = anim->duration;
                break;
        }
    }
    
    float t = state.currentTime;
    
    // Sample channels and apply to transform
    if (anim->positionChannel.keyframeCount() > 0) {
        transform.position = anim->positionChannel.sample(t);
    }
    if (anim->rotationChannel.keyframeCount() > 0) {
        glm::quat q = anim->rotationChannel.sample(t);
        transform.rotation = glm::eulerAngles(q);
    }
    if (anim->scaleChannel.keyframeCount() > 0) {
        transform.scale = anim->scaleChannel.sample(t);
    }
}

} // namespace vkengine
