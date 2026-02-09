#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace vkengine {

// Forward declarations
class Scene;
class GameObject;

// ============================================================================
// Camera Types
// ============================================================================

enum class CameraMode {
    FreeFly,        // WASD + mouse look
    Orbit,          // Rotate around target point
    Follow,         // Follow a target object
    FirstPerson,    // Attached to player position
    ThirdPerson,    // Behind player with collision
    Cinematic,      // Spline-based path
    Fixed,          // Static position
    Orthographic    // 2D-style camera
};

enum class ProjectionType {
    Perspective,
    Orthographic
};

// ============================================================================
// Camera Shake
// ============================================================================

struct CameraShakeParams {
    float duration{0.5f};
    float magnitude{0.1f};
    float frequency{25.0f};
    float damping{0.9f};  // How quickly shake dies down
    bool rotational{false};  // Also shake rotation
    float rotationalMagnitude{0.01f};
};

class CameraShake {
public:
    CameraShake() = default;

    void start(const CameraShakeParams& params);
    void stop();
    [[nodiscard]] bool isActive() const { return active; }

    void update(float deltaSeconds);

    [[nodiscard]] glm::vec3 getOffset() const { return currentOffset; }
    [[nodiscard]] glm::vec3 getRotationOffset() const { return rotationOffset; }

private:
    float noise(float t) const;

    CameraShakeParams params;
    bool active{false};
    float elapsedTime{0.0f};
    glm::vec3 currentOffset{0.0f};
    glm::vec3 rotationOffset{0.0f};
    float currentMagnitude{0.0f};
};

// ============================================================================
// Camera Constraints
// ============================================================================

struct CameraConstraints {
    // Position limits
    bool limitPosition{false};
    glm::vec3 minPosition{-100.0f};
    glm::vec3 maxPosition{100.0f};

    // Rotation limits
    bool limitPitch{true};
    float minPitch{glm::radians(-89.0f)};
    float maxPitch{glm::radians(89.0f)};

    bool limitYaw{false};
    float minYaw{0.0f};
    float maxYaw{0.0f};

    // Distance limits (for orbit/follow)
    float minDistance{1.0f};
    float maxDistance{100.0f};

    // Speed limits
    float maxSpeed{100.0f};
    float maxAngularSpeed{glm::radians(360.0f)};
};

// ============================================================================
// Camera Smoothing
// ============================================================================

struct CameraSmoothingParams {
    bool enabled{true};
    float positionSmoothing{10.0f};   // Higher = faster response
    float rotationSmoothing{15.0f};
    float zoomSmoothing{8.0f};
    bool useSpring{false};            // Spring-based smoothing
    float springStiffness{100.0f};
    float springDamping{10.0f};
};

// ============================================================================
// Orbit Camera Settings
// ============================================================================

struct OrbitCameraSettings {
    glm::vec3 target{0.0f};
    float distance{10.0f};
    float azimuth{0.0f};       // Horizontal angle (radians)
    float elevation{0.3f};     // Vertical angle (radians)
    float orbitSpeed{1.0f};
    float zoomSpeed{2.0f};
    bool autoRotate{false};
    float autoRotateSpeed{0.5f};  // Radians per second
    bool invertY{false};
};

// ============================================================================
// Follow Camera Settings
// ============================================================================

struct FollowCameraSettings {
    GameObject* target{nullptr};
    glm::vec3 offset{0.0f, 2.0f, -5.0f};  // Offset from target
    float lookAhead{0.0f};                // Look ahead based on target velocity
    bool matchTargetRotation{false};       // Rotate with target
    float rotationLag{0.0f};              // Delay in following rotation
    bool avoidObstacles{false};           // Collision avoidance
    float obstacleAvoidanceRadius{0.5f};
};

// ============================================================================
// First Person Camera Settings
// ============================================================================

struct FirstPersonSettings {
    GameObject* attachTo{nullptr};
    glm::vec3 eyeOffset{0.0f, 1.7f, 0.0f};  // Offset from object pivot
    float bobAmplitude{0.02f};
    float bobFrequency{10.0f};
    bool headBob{true};
};

// ============================================================================
// Third Person Camera Settings
// ============================================================================

struct ThirdPersonSettings {
    GameObject* target{nullptr};
    glm::vec3 shoulderOffset{0.3f, 1.5f, 0.0f};  // Offset from target center
    float armLength{3.0f};
    float armPitch{glm::radians(-15.0f)};
    bool collisionAvoidance{true};
    float collisionRadius{0.3f};
    float collisionRecoverySpeed{5.0f};
};

// ============================================================================
// Cinematic Camera (spline path)
// ============================================================================

struct CameraKeyframe {
    float time{0.0f};
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    float fov{60.0f};
    float tension{0.0f};   // For Catmull-Rom spline
    float continuity{0.0f};
    float bias{0.0f};
};

struct CinematicPath {
    std::string name;
    std::vector<CameraKeyframe> keyframes;
    float duration{0.0f};
    bool loop{false};
    bool lookAtTarget{false};
    glm::vec3 lookTarget{0.0f};
};

struct CinematicSettings {
    CinematicPath* path{nullptr};
    float playbackSpeed{1.0f};
    bool playing{false};
    float currentTime{0.0f};
    std::function<void()> onComplete;
};

// ============================================================================
// Orthographic Camera Settings
// ============================================================================

struct OrthographicSettings {
    float size{10.0f};        // Half-height of view
    float nearPlane{-100.0f};
    float farPlane{100.0f};
    bool maintainAspect{true};
};

// ============================================================================
// Advanced Camera Class
// ============================================================================

class CameraEx {
public:
    CameraEx();

    // Mode switching
    void setMode(CameraMode mode);
    [[nodiscard]] CameraMode getMode() const { return currentMode; }

    // Transform
    void setPosition(const glm::vec3& pos);
    [[nodiscard]] const glm::vec3& getPosition() const { return position; }

    void setRotation(const glm::quat& rot);
    void setRotation(float yaw, float pitch, float roll = 0.0f);
    [[nodiscard]] const glm::quat& getRotation() const { return rotation; }
    [[nodiscard]] float getYaw() const { return yaw; }
    [[nodiscard]] float getPitch() const { return pitch; }
    [[nodiscard]] float getRoll() const { return roll; }

    void lookAt(const glm::vec3& target, const glm::vec3& up = {0.0f, 1.0f, 0.0f});

    // Direction vectors
    [[nodiscard]] glm::vec3 forward() const;
    [[nodiscard]] glm::vec3 right() const;
    [[nodiscard]] glm::vec3 up() const;

    // Projection
    void setPerspective(float fovDegrees, float aspect, float nearPlane, float farPlane);
    void setOrthographic(float size, float aspect, float nearPlane, float farPlane);
    void setProjectionType(ProjectionType type);
    [[nodiscard]] ProjectionType getProjectionType() const { return projectionType; }

    void setFOV(float degrees);
    [[nodiscard]] float getFOV() const { return fov; }
    void setNearPlane(float near);
    [[nodiscard]] float getNearPlane() const { return nearPlane; }
    void setFarPlane(float far);
    [[nodiscard]] float getFarPlane() const { return farPlane; }
    void setAspectRatio(float aspect);
    [[nodiscard]] float getAspectRatio() const { return aspectRatio; }

    // Matrices
    [[nodiscard]] glm::mat4 viewMatrix() const;
    [[nodiscard]] glm::mat4 projectionMatrix() const;
    [[nodiscard]] glm::mat4 viewProjectionMatrix() const;

    // Mode-specific settings
    OrbitCameraSettings& orbitSettings() { return orbit; }
    FollowCameraSettings& followSettings() { return follow; }
    FirstPersonSettings& firstPersonSettings() { return firstPerson; }
    ThirdPersonSettings& thirdPersonSettings() { return thirdPerson; }
    CinematicSettings& cinematicSettings() { return cinematic; }
    OrthographicSettings& orthographicSettings() { return ortho; }

    const OrbitCameraSettings& orbitSettings() const { return orbit; }
    const FollowCameraSettings& followSettings() const { return follow; }
    const FirstPersonSettings& firstPersonSettings() const { return firstPerson; }
    const ThirdPersonSettings& thirdPersonSettings() const { return thirdPerson; }
    const CinematicSettings& cinematicSettings() const { return cinematic; }
    const OrthographicSettings& orthographicSettings() const { return ortho; }

    // Constraints & smoothing
    CameraConstraints& constraints() { return cameraConstraints; }
    const CameraConstraints& constraints() const { return cameraConstraints; }
    CameraSmoothingParams& smoothing() { return smoothingParams; }
    const CameraSmoothingParams& smoothing() const { return smoothingParams; }

    // Effects
    void shake(const CameraShakeParams& params);
    void stopShake();
    [[nodiscard]] bool isShaking() const { return shakeEffect.isActive(); }

    // Movement speeds
    void setMoveSpeed(float speed) { moveSpeed = speed; }
    [[nodiscard]] float getMoveSpeed() const { return moveSpeed; }
    void setRotationSpeed(float speed) { rotationSpeed = speed; }
    [[nodiscard]] float getRotationSpeed() const { return rotationSpeed; }
    void setSprintMultiplier(float mult) { sprintMultiplier = mult; }
    [[nodiscard]] float getSprintMultiplier() const { return sprintMultiplier; }

    // Input handling
    void processKeyboard(const glm::vec3& direction, float deltaSeconds, bool sprint = false);
    void processMouse(float deltaX, float deltaY, bool constrainPitch = true);
    void processScroll(float delta);

    // Update (call each frame)
    void update(float deltaSeconds);
    void update(Scene& scene, float deltaSeconds);  // For collision avoidance

    // Cinematic playback
    void playCinematic(CinematicPath* path, float speed = 1.0f);
    void pauseCinematic();
    void resumeCinematic();
    void stopCinematic();
    [[nodiscard]] bool isCinematicPlaying() const { return cinematic.playing; }

    // Frustum for culling
    struct Frustum {
        glm::vec4 planes[6];  // Left, right, bottom, top, near, far
    };
    [[nodiscard]] Frustum calculateFrustum() const;
    [[nodiscard]] bool isPointInFrustum(const glm::vec3& point) const;
    [[nodiscard]] bool isSphereInFrustum(const glm::vec3& center, float radius) const;
    [[nodiscard]] bool isBoxInFrustum(const glm::vec3& min, const glm::vec3& max) const;

    // Screen-space utilities
    [[nodiscard]] glm::vec3 screenToWorld(const glm::vec2& screenPos, float depth, const glm::vec2& screenSize) const;
    [[nodiscard]] glm::vec2 worldToScreen(const glm::vec3& worldPos, const glm::vec2& screenSize) const;
    [[nodiscard]] glm::vec3 screenToRay(const glm::vec2& screenPos, const glm::vec2& screenSize) const;

private:
    void updateFreeFly(float deltaSeconds);
    void updateOrbit(float deltaSeconds);
    void updateFollow(float deltaSeconds, Scene* scene);
    void updateFirstPerson(float deltaSeconds);
    void updateThirdPerson(float deltaSeconds, Scene* scene);
    void updateCinematic(float deltaSeconds);
    void applyConstraints();
    void applySmoothing(float deltaSeconds);
    glm::vec3 evaluateCinematicPosition(float time) const;
    glm::quat evaluateCinematicRotation(float time) const;
    float evaluateCinematicFOV(float time) const;

    CameraMode currentMode{CameraMode::FreeFly};
    ProjectionType projectionType{ProjectionType::Perspective};

    // Transform
    glm::vec3 position{0.0f, 0.0f, 5.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    float yaw{glm::radians(-90.0f)};
    float pitch{0.0f};
    float roll{0.0f};

    // Smoothing targets
    glm::vec3 targetPosition{0.0f, 0.0f, 5.0f};
    glm::quat targetRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 velocity{0.0f};  // For spring smoothing

    // Projection
    float fov{60.0f};
    float aspectRatio{16.0f / 9.0f};
    float nearPlane{0.1f};
    float farPlane{1000.0f};

    // Movement
    float moveSpeed{5.0f};
    float rotationSpeed{0.1f};
    float sprintMultiplier{2.5f};

    // Mode-specific settings
    OrbitCameraSettings orbit;
    FollowCameraSettings follow;
    FirstPersonSettings firstPerson;
    ThirdPersonSettings thirdPerson;
    CinematicSettings cinematic;
    OrthographicSettings ortho;

    // Constraints & effects
    CameraConstraints cameraConstraints;
    CameraSmoothingParams smoothingParams;
    CameraShake shakeEffect;

    // Cached matrices
    mutable bool viewDirty{true};
    mutable bool projDirty{true};
    mutable glm::mat4 cachedView{1.0f};
    mutable glm::mat4 cachedProj{1.0f};
};

// ============================================================================
// Camera Manager (for multiple cameras)
// ============================================================================

class CameraManager {
public:
    CameraManager() = default;

    CameraEx& createCamera(const std::string& name);
    void removeCamera(const std::string& name);
    [[nodiscard]] CameraEx* getCamera(const std::string& name);
    [[nodiscard]] const CameraEx* getCamera(const std::string& name) const;

    void setActiveCamera(const std::string& name);
    [[nodiscard]] CameraEx* activeCamera();
    [[nodiscard]] const CameraEx* activeCamera() const;
    [[nodiscard]] const std::string& activeCameraName() const { return activeName; }

    [[nodiscard]] std::vector<std::string> cameraNames() const;

    void update(float deltaSeconds);
    void update(Scene& scene, float deltaSeconds);

    // Smooth transition between cameras
    void transitionTo(const std::string& name, float duration);
    [[nodiscard]] bool isTransitioning() const { return transitioning; }

private:
    std::unordered_map<std::string, std::unique_ptr<CameraEx>> cameras;
    std::string activeName;
    bool transitioning{false};
    std::string transitionTarget;
    float transitionDuration{0.0f};
    float transitionTime{0.0f};
    glm::vec3 transitionStartPos{0.0f};
    glm::quat transitionStartRot{1.0f, 0.0f, 0.0f, 0.0f};
};

// ============================================================================
// Camera Component for ECS
// ============================================================================

struct CameraComponent {
    std::shared_ptr<CameraEx> camera;
    bool active{false};
    int priority{0};  // Higher priority cameras take precedence
};

} // namespace vkengine
