#include "engine/CameraSystem.hpp"
#include "engine/GameEngine.hpp"

#include <algorithm>
#include <cmath>

namespace vkengine {

// ============================================================================
// CameraShake Implementation
// ============================================================================

void CameraShake::start(const CameraShakeParams& p) {
    params = p;
    active = true;
    elapsedTime = 0.0f;
    currentMagnitude = params.magnitude;
}

void CameraShake::stop() {
    active = false;
    currentOffset = glm::vec3(0.0f);
    rotationOffset = glm::vec3(0.0f);
}

void CameraShake::update(float deltaSeconds) {
    if (!active) return;
    
    elapsedTime += deltaSeconds;
    
    if (elapsedTime >= params.duration) {
        stop();
        return;
    }
    
    // Decay magnitude over time
    float progress = elapsedTime / params.duration;
    currentMagnitude = params.magnitude * (1.0f - progress) * params.damping;
    
    // Generate shake using noise
    float t = elapsedTime * params.frequency;
    currentOffset.x = noise(t) * currentMagnitude;
    currentOffset.y = noise(t + 100.0f) * currentMagnitude;
    currentOffset.z = noise(t + 200.0f) * currentMagnitude;
    
    if (params.rotational) {
        rotationOffset.x = noise(t + 300.0f) * params.rotationalMagnitude;
        rotationOffset.y = noise(t + 400.0f) * params.rotationalMagnitude;
        rotationOffset.z = noise(t + 500.0f) * params.rotationalMagnitude;
    }
}

float CameraShake::noise(float t) const {
    // Simple pseudo-random noise
    return std::sin(t * 1.0f) * 0.5f + 
           std::sin(t * 2.3f) * 0.3f + 
           std::sin(t * 4.7f) * 0.2f;
}

// ============================================================================
// CameraEx Implementation
// ============================================================================

CameraEx::CameraEx() {
    // Initialize with default values from header
}

void CameraEx::setMode(CameraMode mode) {
    currentMode = mode;
    viewDirty = true;
}

void CameraEx::setPosition(const glm::vec3& pos) {
    position = pos;
    targetPosition = pos;
    viewDirty = true;
}

void CameraEx::setRotation(const glm::quat& rot) {
    rotation = rot;
    targetRotation = rot;
    
    // Extract euler angles
    glm::vec3 euler = glm::eulerAngles(rot);
    pitch = euler.x;
    yaw = euler.y;
    roll = euler.z;
    
    viewDirty = true;
}

void CameraEx::setRotation(float y, float p, float r) {
    yaw = y;
    pitch = p;
    roll = r;
    rotation = glm::quat(glm::vec3(pitch, yaw, roll));
    targetRotation = rotation;
    viewDirty = true;
}

void CameraEx::lookAt(const glm::vec3& target, const glm::vec3& upDir) {
    glm::vec3 dir = glm::normalize(target - position);
    
    // Calculate yaw and pitch from direction
    yaw = std::atan2(dir.z, dir.x);
    pitch = std::asin(dir.y);
    
    rotation = glm::quatLookAt(dir, upDir);
    targetRotation = rotation;
    viewDirty = true;
}

glm::vec3 CameraEx::forward() const {
    return glm::normalize(glm::vec3(
        std::cos(yaw) * std::cos(pitch),
        std::sin(pitch),
        std::sin(yaw) * std::cos(pitch)
    ));
}

glm::vec3 CameraEx::right() const {
    return glm::normalize(glm::cross(forward(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::vec3 CameraEx::up() const {
    return glm::normalize(glm::cross(right(), forward()));
}

void CameraEx::setPerspective(float fovDegrees, float aspect, float near, float far) {
    projectionType = ProjectionType::Perspective;
    fov = fovDegrees;
    aspectRatio = aspect;
    nearPlane = near;
    farPlane = far;
    projDirty = true;
}

void CameraEx::setOrthographic(float size, float aspect, float near, float far) {
    projectionType = ProjectionType::Orthographic;
    ortho.size = size;
    aspectRatio = aspect;
    ortho.nearPlane = near;
    ortho.farPlane = far;
    projDirty = true;
}

void CameraEx::setProjectionType(ProjectionType type) {
    projectionType = type;
    projDirty = true;
}

void CameraEx::setFOV(float degrees) {
    fov = degrees;
    projDirty = true;
}

void CameraEx::setNearPlane(float near) {
    nearPlane = near;
    projDirty = true;
}

void CameraEx::setFarPlane(float far) {
    farPlane = far;
    projDirty = true;
}

void CameraEx::setAspectRatio(float aspect) {
    aspectRatio = aspect;
    projDirty = true;
}

glm::mat4 CameraEx::viewMatrix() const {
    if (viewDirty) {
        glm::vec3 pos = position + shakeEffect.getOffset();
        cachedView = glm::lookAt(pos, pos + forward(), up());
        viewDirty = false;
    }
    return cachedView;
}

glm::mat4 CameraEx::projectionMatrix() const {
    if (projDirty) {
        if (projectionType == ProjectionType::Perspective) {
            cachedProj = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
        } else {
            float halfHeight = ortho.size;
            float halfWidth = halfHeight * aspectRatio;
            cachedProj = glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, 
                                    ortho.nearPlane, ortho.farPlane);
        }
        // Flip Y for Vulkan
        cachedProj[1][1] *= -1.0f;
        projDirty = false;
    }
    return cachedProj;
}

glm::mat4 CameraEx::viewProjectionMatrix() const {
    return projectionMatrix() * viewMatrix();
}

void CameraEx::shake(const CameraShakeParams& params) {
    shakeEffect.start(params);
}

void CameraEx::stopShake() {
    shakeEffect.stop();
}

void CameraEx::processKeyboard(const glm::vec3& direction, float deltaSeconds, bool sprint) {
    float speed = moveSpeed * (sprint ? sprintMultiplier : 1.0f) * deltaSeconds;
    
    glm::vec3 movement(0.0f);
    if (direction.z > 0.0f) movement += forward() * speed;
    if (direction.z < 0.0f) movement -= forward() * speed;
    if (direction.x > 0.0f) movement += right() * speed;
    if (direction.x < 0.0f) movement -= right() * speed;
    if (direction.y > 0.0f) movement += glm::vec3(0.0f, 1.0f, 0.0f) * speed;
    if (direction.y < 0.0f) movement -= glm::vec3(0.0f, 1.0f, 0.0f) * speed;
    
    targetPosition += movement;
    viewDirty = true;
}

void CameraEx::processMouse(float deltaX, float deltaY, bool constrainPitch) {
    yaw += deltaX * rotationSpeed;
    pitch -= deltaY * rotationSpeed;
    
    if (constrainPitch) {
        pitch = std::clamp(pitch, cameraConstraints.minPitch, cameraConstraints.maxPitch);
    }
    
    rotation = glm::quat(glm::vec3(pitch, yaw, roll));
    targetRotation = rotation;
    viewDirty = true;
}

void CameraEx::processScroll(float delta) {
    if (currentMode == CameraMode::Orbit) {
        orbit.distance = std::clamp(orbit.distance - delta, 
                                   cameraConstraints.minDistance, 
                                   cameraConstraints.maxDistance);
    } else {
        fov = std::clamp(fov - delta, 1.0f, 120.0f);
        projDirty = true;
    }
}

void CameraEx::update(float deltaSeconds) {
    // Update shake
    shakeEffect.update(deltaSeconds);
    
    // Apply smoothing
    applySmoothing(deltaSeconds);
    
    // Update based on current mode
    switch (currentMode) {
        case CameraMode::FreeFly:
            updateFreeFly(deltaSeconds);
            break;
        case CameraMode::Orbit:
            updateOrbit(deltaSeconds);
            break;
        case CameraMode::Follow:
            updateFollow(deltaSeconds, nullptr);
            break;
        case CameraMode::FirstPerson:
            updateFirstPerson(deltaSeconds);
            break;
        case CameraMode::ThirdPerson:
            updateThirdPerson(deltaSeconds, nullptr);
            break;
        case CameraMode::Cinematic:
            updateCinematic(deltaSeconds);
            break;
        case CameraMode::Fixed:
        case CameraMode::Orthographic:
            // No update needed
            break;
    }
    
    // Apply constraints
    applyConstraints();
}

void CameraEx::update(Scene& scene, float deltaSeconds) {
    shakeEffect.update(deltaSeconds);
    applySmoothing(deltaSeconds);
    
    switch (currentMode) {
        case CameraMode::Follow:
            updateFollow(deltaSeconds, &scene);
            break;
        case CameraMode::ThirdPerson:
            updateThirdPerson(deltaSeconds, &scene);
            break;
        default:
            update(deltaSeconds);
            break;
    }
    
    applyConstraints();
}

void CameraEx::playCinematic(CinematicPath* path, float speed) {
    cinematic.path = path;
    cinematic.playbackSpeed = speed;
    cinematic.playing = true;
    cinematic.currentTime = 0.0f;
    currentMode = CameraMode::Cinematic;
}

void CameraEx::pauseCinematic() {
    cinematic.playing = false;
}

void CameraEx::resumeCinematic() {
    cinematic.playing = true;
}

void CameraEx::stopCinematic() {
    cinematic.playing = false;
    cinematic.currentTime = 0.0f;
}

CameraEx::Frustum CameraEx::calculateFrustum() const {
    Frustum f;
    glm::mat4 vp = viewProjectionMatrix();
    
    // Extract frustum planes from view-projection matrix
    // Left
    f.planes[0] = glm::vec4(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]);
    // Right
    f.planes[1] = glm::vec4(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]);
    // Bottom
    f.planes[2] = glm::vec4(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]);
    // Top
    f.planes[3] = glm::vec4(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]);
    // Near
    f.planes[4] = glm::vec4(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]);
    // Far
    f.planes[5] = glm::vec4(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]);
    
    // Normalize planes
    for (int i = 0; i < 6; i++) {
        float len = glm::length(glm::vec3(f.planes[i]));
        f.planes[i] /= len;
    }
    
    return f;
}

bool CameraEx::isPointInFrustum(const glm::vec3& point) const {
    Frustum f = calculateFrustum();
    for (int i = 0; i < 6; i++) {
        if (glm::dot(glm::vec3(f.planes[i]), point) + f.planes[i].w < 0.0f) {
            return false;
        }
    }
    return true;
}

bool CameraEx::isSphereInFrustum(const glm::vec3& center, float radius) const {
    Frustum f = calculateFrustum();
    for (int i = 0; i < 6; i++) {
        if (glm::dot(glm::vec3(f.planes[i]), center) + f.planes[i].w < -radius) {
            return false;
        }
    }
    return true;
}

bool CameraEx::isBoxInFrustum(const glm::vec3& min, const glm::vec3& max) const {
    Frustum f = calculateFrustum();
    for (int i = 0; i < 6; i++) {
        glm::vec3 p(
            f.planes[i].x > 0 ? max.x : min.x,
            f.planes[i].y > 0 ? max.y : min.y,
            f.planes[i].z > 0 ? max.z : min.z
        );
        if (glm::dot(glm::vec3(f.planes[i]), p) + f.planes[i].w < 0.0f) {
            return false;
        }
    }
    return true;
}

glm::vec3 CameraEx::screenToWorld(const glm::vec2& screenPos, float depth, const glm::vec2& screenSize) const {
    glm::vec4 ndc(
        (screenPos.x / screenSize.x) * 2.0f - 1.0f,
        1.0f - (screenPos.y / screenSize.y) * 2.0f,
        depth,
        1.0f
    );
    
    glm::mat4 invVP = glm::inverse(viewProjectionMatrix());
    glm::vec4 world = invVP * ndc;
    return glm::vec3(world) / world.w;
}

glm::vec2 CameraEx::worldToScreen(const glm::vec3& worldPos, const glm::vec2& screenSize) const {
    glm::vec4 clip = viewProjectionMatrix() * glm::vec4(worldPos, 1.0f);
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    
    return glm::vec2(
        (ndc.x + 1.0f) * 0.5f * screenSize.x,
        (1.0f - ndc.y) * 0.5f * screenSize.y
    );
}

glm::vec3 CameraEx::screenToRay(const glm::vec2& screenPos, const glm::vec2& screenSize) const {
    glm::vec3 nearPoint = screenToWorld(screenPos, 0.0f, screenSize);
    glm::vec3 farPoint = screenToWorld(screenPos, 1.0f, screenSize);
    return glm::normalize(farPoint - nearPoint);
}

// Private methods

void CameraEx::updateFreeFly(float deltaSeconds) {
    (void)deltaSeconds;
    // Movement handled by processKeyboard
}

void CameraEx::updateOrbit(float deltaSeconds) {
    (void)deltaSeconds;
    
    // Calculate position on orbit sphere
    float x = orbit.distance * std::cos(pitch) * std::cos(yaw);
    float y = orbit.distance * std::sin(pitch);
    float z = orbit.distance * std::cos(pitch) * std::sin(yaw);
    
    position = orbit.target + glm::vec3(x, y, z);
    lookAt(orbit.target);
}

void CameraEx::updateFollow(float deltaSeconds, Scene* scene) {
    (void)scene;
    
    if (!follow.target) return;
    
    // Get target position from GameObject
    glm::vec3 targetPos = follow.target->transform().position;
    glm::vec3 desiredPos = targetPos + follow.offset;
    
    // Smooth follow using smoothingParams
    float smoothing = smoothingParams.positionSmoothing;
    float t = 1.0f - std::exp(-smoothing * deltaSeconds);
    position = glm::mix(position, desiredPos, t);
    
    // Always look at target for follow camera
    lookAt(targetPos);
}

void CameraEx::updateFirstPerson(float deltaSeconds) {
    (void)deltaSeconds;
    // Position is controlled externally (attached to player head)
    // Rotation is controlled by mouse input
}

void CameraEx::updateThirdPerson(float deltaSeconds, Scene* scene) {
    (void)scene;
    
    if (!thirdPerson.target) return;
    
    // Get target position from GameObject
    glm::vec3 targetPos = thirdPerson.target->transform().position;
    
    // Calculate desired camera position behind player
    glm::vec3 desiredPos = targetPos - forward() * thirdPerson.armLength + 
                           glm::vec3(thirdPerson.shoulderOffset.x, thirdPerson.shoulderOffset.y, 0.0f);
    
    // Smooth follow
    float t = 1.0f - std::pow(0.5f, deltaSeconds * 10.0f);
    position = glm::mix(position, desiredPos, t);
    
    lookAt(targetPos + glm::vec3(0.0f, thirdPerson.shoulderOffset.y * 0.5f, 0.0f));
}

void CameraEx::updateCinematic(float deltaSeconds) {
    if (!cinematic.playing || !cinematic.path) return;
    
    cinematic.currentTime += deltaSeconds * cinematic.playbackSpeed;
    
    // Evaluate path
    position = evaluateCinematicPosition(cinematic.currentTime);
    rotation = evaluateCinematicRotation(cinematic.currentTime);
    fov = evaluateCinematicFOV(cinematic.currentTime);
    
    // Check for completion
    float dur = cinematic.path->duration;
    if (cinematic.currentTime >= dur) {
        if (cinematic.path->loop) {
            cinematic.currentTime = std::fmod(cinematic.currentTime, dur);
        } else {
            cinematic.playing = false;
            if (cinematic.onComplete) {
                cinematic.onComplete();
            }
        }
    }
    
    viewDirty = true;
    projDirty = true;
}

void CameraEx::applyConstraints() {
    if (cameraConstraints.limitPosition) {
        position = glm::clamp(position, cameraConstraints.minPosition, cameraConstraints.maxPosition);
        targetPosition = glm::clamp(targetPosition, cameraConstraints.minPosition, cameraConstraints.maxPosition);
    }
    
    if (cameraConstraints.limitPitch) {
        pitch = std::clamp(pitch, cameraConstraints.minPitch, cameraConstraints.maxPitch);
    }
    
    if (cameraConstraints.limitYaw) {
        yaw = std::clamp(yaw, cameraConstraints.minYaw, cameraConstraints.maxYaw);
    }
    
    viewDirty = true;
}

void CameraEx::applySmoothing(float deltaSeconds) {
    if (!smoothingParams.enabled) {
        position = targetPosition;
        rotation = targetRotation;
        return;
    }
    
    if (smoothingParams.useSpring) {
        // Spring-based smoothing
        glm::vec3 diff = targetPosition - position;
        glm::vec3 accel = diff * smoothingParams.springStiffness - velocity * smoothingParams.springDamping;
        velocity += accel * deltaSeconds;
        position += velocity * deltaSeconds;
    } else {
        // Exponential smoothing
        float t = 1.0f - std::exp(-smoothingParams.positionSmoothing * deltaSeconds);
        position = glm::mix(position, targetPosition, t);
        
        t = 1.0f - std::exp(-smoothingParams.rotationSmoothing * deltaSeconds);
        rotation = glm::slerp(rotation, targetRotation, t);
    }
    
    viewDirty = true;
}

glm::vec3 CameraEx::evaluateCinematicPosition(float time) const {
    if (!cinematic.path) return position;
    
    const auto& kfs = cinematic.path->keyframes;
    if (kfs.empty()) return position;
    if (kfs.size() == 1) return kfs[0].position;
    
    // Find keyframes
    size_t i = 0;
    for (; i < kfs.size() - 1; i++) {
        if (time < kfs[i + 1].time) break;
    }
    
    if (i >= kfs.size() - 1) return kfs.back().position;
    
    float t = (time - kfs[i].time) / (kfs[i + 1].time - kfs[i].time);
    return glm::mix(kfs[i].position, kfs[i + 1].position, t);
}

glm::quat CameraEx::evaluateCinematicRotation(float time) const {
    if (!cinematic.path) return rotation;
    
    const auto& kfs = cinematic.path->keyframes;
    if (kfs.empty()) return rotation;
    if (kfs.size() == 1) return kfs[0].rotation;
    
    size_t i = 0;
    for (; i < kfs.size() - 1; i++) {
        if (time < kfs[i + 1].time) break;
    }
    
    if (i >= kfs.size() - 1) return kfs.back().rotation;
    
    float t = (time - kfs[i].time) / (kfs[i + 1].time - kfs[i].time);
    return glm::slerp(kfs[i].rotation, kfs[i + 1].rotation, t);
}

float CameraEx::evaluateCinematicFOV(float time) const {
    if (!cinematic.path) return fov;
    
    const auto& kfs = cinematic.path->keyframes;
    if (kfs.empty()) return fov;
    if (kfs.size() == 1) return kfs[0].fov;
    
    size_t i = 0;
    for (; i < kfs.size() - 1; i++) {
        if (time < kfs[i + 1].time) break;
    }
    
    if (i >= kfs.size() - 1) return kfs.back().fov;
    
    float t = (time - kfs[i].time) / (kfs[i + 1].time - kfs[i].time);
    return glm::mix(kfs[i].fov, kfs[i + 1].fov, t);
}

// ============================================================================
// CameraManager Implementation
// ============================================================================

CameraEx& CameraManager::createCamera(const std::string& name) {
    auto camera = std::make_unique<CameraEx>();
    CameraEx& ref = *camera;
    cameras[name] = std::move(camera);
    
    if (activeName.empty()) {
        activeName = name;
    }
    
    return ref;
}

void CameraManager::removeCamera(const std::string& name) {
    if (activeName == name) {
        activeName.clear();
    }
    cameras.erase(name);
}

CameraEx* CameraManager::getCamera(const std::string& name) {
    auto it = cameras.find(name);
    return it != cameras.end() ? it->second.get() : nullptr;
}

const CameraEx* CameraManager::getCamera(const std::string& name) const {
    auto it = cameras.find(name);
    return it != cameras.end() ? it->second.get() : nullptr;
}

void CameraManager::setActiveCamera(const std::string& name) {
    if (cameras.find(name) != cameras.end()) {
        activeName = name;
    }
}

CameraEx* CameraManager::activeCamera() {
    return getCamera(activeName);
}

const CameraEx* CameraManager::activeCamera() const {
    return getCamera(activeName);
}

std::vector<std::string> CameraManager::cameraNames() const {
    std::vector<std::string> names;
    names.reserve(cameras.size());
    for (const auto& [name, _] : cameras) {
        names.push_back(name);
    }
    return names;
}

void CameraManager::update(float deltaSeconds) {
    // Update transition
    if (transitioning) {
        transitionTime += deltaSeconds;
        if (transitionTime >= transitionDuration) {
            transitioning = false;
            activeName = transitionTarget;
        }
    }
    
    // Update active camera
    if (CameraEx* cam = activeCamera()) {
        cam->update(deltaSeconds);
    }
}

void CameraManager::update(Scene& scene, float deltaSeconds) {
    if (transitioning) {
        transitionTime += deltaSeconds;
        if (transitionTime >= transitionDuration) {
            transitioning = false;
            activeName = transitionTarget;
        }
    }
    
    if (CameraEx* cam = activeCamera()) {
        cam->update(scene, deltaSeconds);
    }
}

void CameraManager::transitionTo(const std::string& name, float duration) {
    if (cameras.find(name) == cameras.end()) return;
    
    transitioning = true;
    transitionTarget = name;
    transitionDuration = duration;
    transitionTime = 0.0f;
    
    if (CameraEx* current = activeCamera()) {
        transitionStartPos = current->getPosition();
        transitionStartRot = current->getRotation();
    }
}

} // namespace vkengine
