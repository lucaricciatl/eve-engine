#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace vkengine {

// Forward declarations
class Scene;
class GameObject;
struct Transform;

// ============================================================================
// Animation Types
// ============================================================================

using BoneIndex = std::int32_t;
constexpr BoneIndex InvalidBone = -1;

enum class InterpolationType {
    Step,       // No interpolation
    Linear,     // Linear interpolation
    CubicSpline // Cubic spline interpolation
};

enum class AnimationWrapMode {
    Once,       // Play once and stop
    Loop,       // Loop continuously
    PingPong,   // Alternate forward/backward
    ClampForever // Play once and hold last frame
};

// ============================================================================
// Keyframe Types
// ============================================================================

template<typename T>
struct Keyframe {
    float time{0.0f};
    T value{};
    T inTangent{};   // For cubic spline
    T outTangent{};  // For cubic spline
};

using VectorKeyframe = Keyframe<glm::vec3>;
using QuaternionKeyframe = Keyframe<glm::quat>;
using FloatKeyframe = Keyframe<float>;

// ============================================================================
// Animation Channel (animates a single property)
// ============================================================================

template<typename T>
class AnimationChannel {
public:
    AnimationChannel() = default;

    void addKeyframe(float time, const T& value);
    void addKeyframe(float time, const T& value, const T& inTangent, const T& outTangent);

    [[nodiscard]] T sample(float time, InterpolationType interp = InterpolationType::Linear) const;
    [[nodiscard]] float duration() const;
    [[nodiscard]] std::size_t keyframeCount() const { return keyframes.size(); }
    [[nodiscard]] const std::vector<Keyframe<T>>& getKeyframes() const { return keyframes; }

    void clear() { keyframes.clear(); }

private:
    T interpolate(const Keyframe<T>& a, const Keyframe<T>& b, float t, InterpolationType interp) const;

    std::vector<Keyframe<T>> keyframes;
};

using PositionChannel = AnimationChannel<glm::vec3>;
using RotationChannel = AnimationChannel<glm::quat>;
using ScaleChannel = AnimationChannel<glm::vec3>;
using FloatChannel = AnimationChannel<float>;

// ============================================================================
// Bone / Joint
// ============================================================================

struct Bone {
    std::string name;
    BoneIndex index{InvalidBone};
    BoneIndex parentIndex{InvalidBone};
    glm::mat4 offsetMatrix{1.0f};      // Inverse bind pose matrix
    glm::mat4 localTransform{1.0f};    // Current local transform
    glm::vec3 localPosition{0.0f};
    glm::quat localRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 localScale{1.0f};
};

// ============================================================================
// Skeleton
// ============================================================================

class Skeleton {
public:
    Skeleton() = default;

    BoneIndex addBone(const std::string& name, BoneIndex parent = InvalidBone);
    BoneIndex findBone(const std::string& name) const;

    [[nodiscard]] Bone* getBone(BoneIndex index);
    [[nodiscard]] const Bone* getBone(BoneIndex index) const;
    [[nodiscard]] Bone* getBone(const std::string& name);
    [[nodiscard]] const Bone* getBone(const std::string& name) const;

    [[nodiscard]] std::size_t boneCount() const { return bones.size(); }
    [[nodiscard]] const std::vector<Bone>& getBones() const { return bones; }

    void setBindPose();
    void calculateGlobalTransforms();
    [[nodiscard]] const std::vector<glm::mat4>& getFinalBoneMatrices() const { return finalMatrices; }

    // For GPU skinning
    [[nodiscard]] const glm::mat4* boneMatrixData() const;
    [[nodiscard]] std::size_t boneMatrixCount() const { return finalMatrices.size(); }

private:
    std::vector<Bone> bones;
    std::unordered_map<std::string, BoneIndex> boneNameMap;
    std::vector<glm::mat4> globalTransforms;
    std::vector<glm::mat4> finalMatrices;
};

// ============================================================================
// Animation Track (animates a single bone)
// ============================================================================

struct BoneAnimationTrack {
    std::string boneName;
    BoneIndex boneIndex{InvalidBone};
    PositionChannel positionChannel;
    RotationChannel rotationChannel;
    ScaleChannel scaleChannel;
};

// ============================================================================
// Animation Clip
// ============================================================================

class AnimationClip {
public:
    AnimationClip() = default;
    explicit AnimationClip(const std::string& clipName);

    void setName(const std::string& name) { clipName = name; }
    [[nodiscard]] const std::string& name() const { return clipName; }

    void setDuration(float seconds) { clipDuration = seconds; }
    [[nodiscard]] float duration() const { return clipDuration; }

    void setTicksPerSecond(float tps) { ticksPerSecond = tps; }
    [[nodiscard]] float getTicksPerSecond() const { return ticksPerSecond; }

    BoneAnimationTrack& addTrack(const std::string& boneName);
    [[nodiscard]] BoneAnimationTrack* getTrack(const std::string& boneName);
    [[nodiscard]] const BoneAnimationTrack* getTrack(const std::string& boneName) const;
    [[nodiscard]] const std::vector<BoneAnimationTrack>& getTracks() const { return tracks; }

    // Sample all tracks at given time and apply to skeleton
    void sample(float time, Skeleton& skeleton, InterpolationType interp = InterpolationType::Linear) const;

private:
    std::string clipName;
    float clipDuration{0.0f};
    float ticksPerSecond{30.0f};
    std::vector<BoneAnimationTrack> tracks;
    std::unordered_map<std::string, std::size_t> trackNameMap;
};

// ============================================================================
// Animation State
// ============================================================================

struct AnimationState {
    const AnimationClip* clip{nullptr};
    float currentTime{0.0f};
    float speed{1.0f};
    float weight{1.0f};
    AnimationWrapMode wrapMode{AnimationWrapMode::Loop};
    bool playing{false};

    void update(float deltaSeconds);
    void play() { playing = true; }
    void pause() { playing = false; }
    void stop() { playing = false; currentTime = 0.0f; }
    void setTime(float time) { currentTime = time; }
    [[nodiscard]] float normalizedTime() const;
};

// ============================================================================
// Animation Layer (for blending multiple animations)
// ============================================================================

struct AnimationLayer {
    std::string name;
    AnimationState state;
    float blendWeight{1.0f};
    bool additive{false};  // Additive blending vs override
    std::uint32_t mask{~0u};  // Bone mask for partial body animations
};

// ============================================================================
// Animation Blend Tree
// ============================================================================

enum class BlendNodeType {
    Clip,
    Blend1D,
    Blend2D,
    Additive
};

struct BlendNode {
    BlendNodeType type{BlendNodeType::Clip};
    const AnimationClip* clip{nullptr};
    std::vector<std::unique_ptr<BlendNode>> children;
    std::vector<float> thresholds;  // For 1D blending
    glm::vec2 position{0.0f};       // For 2D blending
    float weight{1.0f};
};

class AnimationBlendTree {
public:
    AnimationBlendTree() = default;

    void setRoot(std::unique_ptr<BlendNode> root);
    [[nodiscard]] BlendNode* getRoot() { return rootNode.get(); }

    void setParameter(const std::string& name, float value);
    [[nodiscard]] float getParameter(const std::string& name) const;

    void evaluate(float time, Skeleton& skeleton) const;

private:
    void evaluateNode(const BlendNode* node, float time, Skeleton& skeleton, float parentWeight) const;

    std::unique_ptr<BlendNode> rootNode;
    std::unordered_map<std::string, float> parameters;
};

// ============================================================================
// Animation State Machine
// ============================================================================

struct AnimationTransition {
    std::string fromState;
    std::string toState;
    float duration{0.25f};
    float exitTime{0.0f};     // Normalized time when transition can occur (0 = any time)
    bool hasExitTime{false};
    std::function<bool()> condition;  // Optional condition callback
};

class AnimationStateMachine {
public:
    AnimationStateMachine() = default;

    void addState(const std::string& name, const AnimationClip* clip);
    void addState(const std::string& name, std::unique_ptr<AnimationBlendTree> blendTree);
    void removeState(const std::string& name);

    void addTransition(const AnimationTransition& transition);
    void setDefaultState(const std::string& name);

    void trigger(const std::string& triggerName);
    void setBool(const std::string& name, bool value);
    void setFloat(const std::string& name, float value);
    void setInt(const std::string& name, int value);

    [[nodiscard]] bool getBool(const std::string& name) const;
    [[nodiscard]] float getFloat(const std::string& name) const;
    [[nodiscard]] int getInt(const std::string& name) const;

    [[nodiscard]] const std::string& currentStateName() const { return currentState; }
    [[nodiscard]] bool isInTransition() const { return inTransition; }

    void update(float deltaSeconds, Skeleton& skeleton);

private:
    struct State {
        std::string name;
        const AnimationClip* clip{nullptr};
        std::unique_ptr<AnimationBlendTree> blendTree;
        AnimationState animState;
    };

    void checkTransitions();
    void startTransition(const AnimationTransition& transition);
    void updateTransition(float deltaSeconds, Skeleton& skeleton);

    std::unordered_map<std::string, State> states;
    std::vector<AnimationTransition> transitions;
    std::string currentState;
    std::string defaultState;
    std::unordered_map<std::string, bool> boolParams;
    std::unordered_map<std::string, float> floatParams;
    std::unordered_map<std::string, int> intParams;
    std::unordered_set<std::string> triggers;

    bool inTransition{false};
    std::string targetState;
    float transitionTime{0.0f};
    float transitionDuration{0.0f};
};

// ============================================================================
// Animator Component
// ============================================================================

struct AnimatorComponent {
    std::shared_ptr<Skeleton> skeleton;
    std::unique_ptr<AnimationStateMachine> stateMachine;
    std::vector<AnimationLayer> layers;
    bool enabled{true};
    float globalSpeed{1.0f};
};

// ============================================================================
// Animation System
// ============================================================================

class AnimationSystem {
public:
    AnimationSystem() = default;

    void update(Scene& scene, float deltaSeconds);

    // Animation library management
    void addClip(const std::string& name, std::shared_ptr<AnimationClip> clip);
    void removeClip(const std::string& name);
    [[nodiscard]] std::shared_ptr<AnimationClip> getClip(const std::string& name) const;
    [[nodiscard]] bool hasClip(const std::string& name) const;

private:
    std::unordered_map<std::string, std::shared_ptr<AnimationClip>> clipLibrary;
};

// ============================================================================
// Animation Loader (for glTF, FBX, etc.)
// ============================================================================

class AnimationLoader {
public:
    struct LoadResult {
        std::shared_ptr<Skeleton> skeleton;
        std::vector<std::shared_ptr<AnimationClip>> clips;
        bool success{false};
        std::string errorMessage;
    };

    static LoadResult loadFromGLTF(const std::filesystem::path& path);
    static LoadResult loadFromFile(const std::filesystem::path& path);
};

// ============================================================================
// Simple Transform Animation (non-skeletal)
// ============================================================================

struct TransformAnimation {
    std::string name;
    PositionChannel positionChannel;
    RotationChannel rotationChannel;
    ScaleChannel scaleChannel;
    FloatChannel opacityChannel;
    float duration{0.0f};
    AnimationWrapMode wrapMode{AnimationWrapMode::Once};
};

struct TransformAnimationState {
    const TransformAnimation* animation{nullptr};
    float currentTime{0.0f};
    float speed{1.0f};
    bool playing{false};
    std::function<void()> onComplete;
};

class TransformAnimationComponent {
public:
    void play(const TransformAnimation* anim, bool restart = true);
    void stop();
    void pause();
    void resume();

    void update(float deltaSeconds, Transform& transform);

    [[nodiscard]] bool isPlaying() const { return state.playing; }
    [[nodiscard]] float currentTime() const { return state.currentTime; }

    void setOnComplete(std::function<void()> callback) { state.onComplete = std::move(callback); }

private:
    TransformAnimationState state;
};

} // namespace vkengine
