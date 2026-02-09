#pragma once

#include "core/ecs/Components.hpp"
#include "core/ecs/Registry.hpp"
#include "engine/GameEngine.hpp"
#include "engine/Material.hpp"

#include <glm/glm.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace vkengine {

// Forward declarations
class Scene;
class GameObject;
class Light;

// ============================================================================
// JSON Value Types (lightweight JSON implementation)
// ============================================================================

class JsonValue;

using JsonObject = std::unordered_map<std::string, std::shared_ptr<JsonValue>>;
using JsonArray = std::vector<std::shared_ptr<JsonValue>>;
using JsonNull = std::nullptr_t;

class JsonValue {
public:
    using Value = std::variant<JsonNull, bool, double, std::string, JsonArray, JsonObject>;

    JsonValue() : value(nullptr) {}
    JsonValue(std::nullptr_t) : value(nullptr) {}
    JsonValue(bool b) : value(b) {}
    JsonValue(int i) : value(static_cast<double>(i)) {}
    JsonValue(float f) : value(static_cast<double>(f)) {}
    JsonValue(double d) : value(d) {}
    JsonValue(const char* s) : value(std::string(s)) {}
    JsonValue(std::string s) : value(std::move(s)) {}
    JsonValue(JsonArray arr) : value(std::move(arr)) {}
    JsonValue(JsonObject obj) : value(std::move(obj)) {}

    [[nodiscard]] bool isNull() const { return std::holds_alternative<JsonNull>(value); }
    [[nodiscard]] bool isBool() const { return std::holds_alternative<bool>(value); }
    [[nodiscard]] bool isNumber() const { return std::holds_alternative<double>(value); }
    [[nodiscard]] bool isString() const { return std::holds_alternative<std::string>(value); }
    [[nodiscard]] bool isArray() const { return std::holds_alternative<JsonArray>(value); }
    [[nodiscard]] bool isObject() const { return std::holds_alternative<JsonObject>(value); }

    [[nodiscard]] bool asBool() const { return std::get<bool>(value); }
    [[nodiscard]] double asNumber() const { return std::get<double>(value); }
    [[nodiscard]] float asFloat() const { return static_cast<float>(std::get<double>(value)); }
    [[nodiscard]] int asInt() const { return static_cast<int>(std::get<double>(value)); }
    [[nodiscard]] const std::string& asString() const { return std::get<std::string>(value); }
    [[nodiscard]] const JsonArray& asArray() const { return std::get<JsonArray>(value); }
    [[nodiscard]] const JsonObject& asObject() const { return std::get<JsonObject>(value); }
    [[nodiscard]] JsonArray& asArray() { return std::get<JsonArray>(value); }
    [[nodiscard]] JsonObject& asObject() { return std::get<JsonObject>(value); }

    // Object access
    [[nodiscard]] bool has(const std::string& key) const;
    [[nodiscard]] const JsonValue& operator[](const std::string& key) const;
    [[nodiscard]] JsonValue& operator[](const std::string& key);

    // Array access
    [[nodiscard]] const JsonValue& operator[](std::size_t index) const;
    [[nodiscard]] JsonValue& operator[](std::size_t index);
    [[nodiscard]] std::size_t size() const;

    // Serialization
    [[nodiscard]] std::string stringify(int indent = 2) const;
    static std::shared_ptr<JsonValue> parse(const std::string& json);

private:
    Value value;
    std::string stringifyImpl(int currentIndent, int indentStep) const;
};

// Helper to create shared JsonValue
inline std::shared_ptr<JsonValue> json(JsonValue v) {
    return std::make_shared<JsonValue>(std::move(v));
}

// ============================================================================
// Serialization helpers for GLM types
// ============================================================================

namespace serialization {

std::shared_ptr<JsonValue> serializeVec2(const glm::vec2& v);
std::shared_ptr<JsonValue> serializeVec3(const glm::vec3& v);
std::shared_ptr<JsonValue> serializeVec4(const glm::vec4& v);
std::shared_ptr<JsonValue> serializeMat4(const glm::mat4& m);

glm::vec2 deserializeVec2(const JsonValue& v);
glm::vec3 deserializeVec3(const JsonValue& v);
glm::vec4 deserializeVec4(const JsonValue& v);
glm::mat4 deserializeMat4(const JsonValue& v);

// ============================================================================
// Component Serializers
// ============================================================================

std::shared_ptr<JsonValue> serializeTransform(const Transform& t);
Transform deserializeTransform(const JsonValue& v);

std::shared_ptr<JsonValue> serializePhysicsProperties(const PhysicsProperties& p);
PhysicsProperties deserializePhysicsProperties(const JsonValue& v);

std::shared_ptr<JsonValue> serializeCollider(const Collider& c);
Collider deserializeCollider(const JsonValue& v);

std::shared_ptr<JsonValue> serializeRenderComponent(const RenderComponent& r);
RenderComponent deserializeRenderComponent(const JsonValue& v);

std::shared_ptr<JsonValue> serializeLightComponent(const LightComponent& l);
LightComponent deserializeLightComponent(const JsonValue& v);

std::shared_ptr<JsonValue> serializeMaterial(const Material& m);
Material deserializeMaterial(const JsonValue& v);

} // namespace serialization

// ============================================================================
// Prefab System
// ============================================================================

struct Prefab {
    std::string name;
    std::string description;
    std::shared_ptr<JsonValue> data;

    // Components stored in the prefab
    std::optional<Transform> transform;
    std::optional<PhysicsProperties> physics;
    std::optional<Collider> collider;
    std::optional<RenderComponent> render;
    std::optional<std::string> meshResource;
    std::optional<std::string> albedoTexture;
};

class PrefabLibrary {
public:
    PrefabLibrary() = default;

    // Create prefab from existing GameObject
    Prefab createFromGameObject(const GameObject& object, const std::string& prefabName);

    // Instantiate prefab into scene
    GameObject& instantiate(Scene& scene, const Prefab& prefab, const std::string& instanceName);
    GameObject& instantiate(Scene& scene, const std::string& prefabName, const std::string& instanceName);

    // Library management
    void add(Prefab prefab);
    bool remove(const std::string& name);
    [[nodiscard]] bool has(const std::string& name) const;
    [[nodiscard]] const Prefab* find(const std::string& name) const;
    [[nodiscard]] std::vector<std::string> names() const;

    // Persistence
    bool saveToFile(const std::filesystem::path& path) const;
    bool loadFromFile(const std::filesystem::path& path);

private:
    std::unordered_map<std::string, Prefab> prefabs;
};

// ============================================================================
// Scene Serializer
// ============================================================================

struct SceneData {
    std::string name;
    std::string version{"1.0"};
    glm::vec3 ambientColor{0.1f};
    glm::vec3 gravity{0.0f, -9.81f, 0.0f};

    struct CameraData {
        glm::vec3 position{0.0f, 0.0f, 5.0f};
        float yaw{-90.0f};
        float pitch{0.0f};
        float fov{60.0f};
        float nearClip{0.1f};
        float farClip{100.0f};
    } camera;

    std::vector<std::shared_ptr<JsonValue>> gameObjects;
    std::vector<std::shared_ptr<JsonValue>> lights;
    std::vector<Material> materials;
};

class SceneSerializer {
public:
    SceneSerializer() = default;

    // Serialize entire scene to JSON
    [[nodiscard]] std::shared_ptr<JsonValue> serialize(const Scene& scene) const;

    // Deserialize JSON into scene (clears existing content)
    void deserialize(Scene& scene, const JsonValue& data) const;

    // File operations
    bool saveToFile(const Scene& scene, const std::filesystem::path& path) const;
    bool loadFromFile(Scene& scene, const std::filesystem::path& path) const;

    // Serialize individual game object
    [[nodiscard]] std::shared_ptr<JsonValue> serializeGameObject(const GameObject& object) const;
    void deserializeGameObject(Scene& scene, const JsonValue& data) const;

    // Serialize individual light
    [[nodiscard]] std::shared_ptr<JsonValue> serializeLight(const Light& light) const;
    void deserializeLight(Scene& scene, const JsonValue& data) const;

    // Custom component serializers (for extensibility)
    using ComponentSerializer = std::function<std::shared_ptr<JsonValue>(const GameObject&)>;
    using ComponentDeserializer = std::function<void(GameObject&, const JsonValue&)>;

    void registerComponentSerializer(const std::string& typeName, ComponentSerializer serializer);
    void registerComponentDeserializer(const std::string& typeName, ComponentDeserializer deserializer);

private:
    std::unordered_map<std::string, ComponentSerializer> customSerializers;
    std::unordered_map<std::string, ComponentDeserializer> customDeserializers;
};

// ============================================================================
// Undo/Redo System
// ============================================================================

class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    [[nodiscard]] virtual std::string description() const = 0;
};

class UndoRedoManager {
public:
    UndoRedoManager(std::size_t maxHistory = 100);

    void execute(std::unique_ptr<Command> command);
    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();
    void clear();

    [[nodiscard]] std::string undoDescription() const;
    [[nodiscard]] std::string redoDescription() const;
    [[nodiscard]] std::size_t undoStackSize() const { return undoStack.size(); }
    [[nodiscard]] std::size_t redoStackSize() const { return redoStack.size(); }

private:
    std::vector<std::unique_ptr<Command>> undoStack;
    std::vector<std::unique_ptr<Command>> redoStack;
    std::size_t maxHistorySize;
};

// Common commands
class TransformCommand : public Command {
public:
    TransformCommand(GameObject& obj, const Transform& newTransform);
    void execute() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    GameObject* object;
    Transform oldTransform;
    Transform newTransform;
};

class CreateObjectCommand : public Command {
public:
    CreateObjectCommand(Scene& scene, const std::string& name, MeshType mesh);
    void execute() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scene* scene;
    std::string objectName;
    MeshType meshType;
    core::ecs::Entity createdEntity{};
};

class DeleteObjectCommand : public Command {
public:
    DeleteObjectCommand(Scene& scene, const std::string& objectName);
    void execute() override;
    void undo() override;
    [[nodiscard]] std::string description() const override;

private:
    Scene* scene;
    std::string objectName;
    std::shared_ptr<JsonValue> savedData;
};

} // namespace vkengine
