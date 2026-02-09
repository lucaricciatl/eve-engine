#include "engine/Serialization.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace vkengine {

// ============================================================================
// JsonValue Implementation (methods not defined inline in header)
// ============================================================================

static JsonValue nullJsonValue;

bool JsonValue::has(const std::string& key) const {
    if (!isObject()) return false;
    const auto& obj = asObject();
    return obj.find(key) != obj.end();
}

const JsonValue& JsonValue::operator[](const std::string& key) const {
    if (!isObject()) return nullJsonValue;
    const auto& obj = asObject();
    auto it = obj.find(key);
    if (it == obj.end()) return nullJsonValue;
    return *it->second;
}

JsonValue& JsonValue::operator[](const std::string& key) {
    if (!isObject()) {
        value = JsonObject{};
    }
    auto& obj = asObject();
    if (obj.find(key) == obj.end()) {
        obj[key] = std::make_shared<JsonValue>();
    }
    return *obj[key];
}

const JsonValue& JsonValue::operator[](std::size_t index) const {
    if (!isArray()) return nullJsonValue;
    const auto& arr = asArray();
    if (index >= arr.size()) return nullJsonValue;
    return *arr[index];
}

JsonValue& JsonValue::operator[](std::size_t index) {
    if (!isArray()) {
        value = JsonArray{};
    }
    auto& arr = asArray();
    while (arr.size() <= index) {
        arr.push_back(std::make_shared<JsonValue>());
    }
    return *arr[index];
}

std::size_t JsonValue::size() const {
    if (isArray()) return asArray().size();
    if (isObject()) return asObject().size();
    return 0;
}

std::string JsonValue::stringifyImpl(int currentIndent, int indentStep) const {
    std::ostringstream oss;
    std::string indent(currentIndent, ' ');
    std::string childIndent(currentIndent + indentStep, ' ');
    
    if (isNull()) {
        oss << "null";
    } else if (isBool()) {
        oss << (asBool() ? "true" : "false");
    } else if (isNumber()) {
        oss << std::setprecision(10) << asNumber();
    } else if (isString()) {
        oss << '"';
        for (char c : asString()) {
            switch (c) {
                case '"': oss << "\\\""; break;
                case '\\': oss << "\\\\"; break;
                case '\n': oss << "\\n"; break;
                case '\r': oss << "\\r"; break;
                case '\t': oss << "\\t"; break;
                default: oss << c; break;
            }
        }
        oss << '"';
    } else if (isArray()) {
        const auto& arr = asArray();
        if (arr.empty()) {
            oss << "[]";
        } else {
            oss << "[\n";
            for (size_t i = 0; i < arr.size(); ++i) {
                oss << childIndent << arr[i]->stringifyImpl(currentIndent + indentStep, indentStep);
                if (i < arr.size() - 1) oss << ",";
                oss << "\n";
            }
            oss << indent << "]";
        }
    } else if (isObject()) {
        const auto& obj = asObject();
        if (obj.empty()) {
            oss << "{}";
        } else {
            oss << "{\n";
            size_t count = 0;
            for (const auto& [key, val] : obj) {
                oss << childIndent << '"' << key << "\": " << val->stringifyImpl(currentIndent + indentStep, indentStep);
                if (++count < obj.size()) oss << ",";
                oss << "\n";
            }
            oss << indent << "}";
        }
    }
    
    return oss.str();
}

std::string JsonValue::stringify(int indent) const {
    return stringifyImpl(0, indent);
}

std::shared_ptr<JsonValue> JsonValue::parse(const std::string& /*json*/) {
    // Simplified stub - full parser would be more complex
    return std::make_shared<JsonValue>();
}

// ============================================================================
// Serialization helpers
// ============================================================================

namespace serialization {

std::shared_ptr<JsonValue> serializeVec2(const glm::vec2& v) {
    auto arr = std::make_shared<JsonValue>(JsonArray{});
    arr->asArray().push_back(json(v.x));
    arr->asArray().push_back(json(v.y));
    return arr;
}

std::shared_ptr<JsonValue> serializeVec3(const glm::vec3& v) {
    auto arr = std::make_shared<JsonValue>(JsonArray{});
    arr->asArray().push_back(json(v.x));
    arr->asArray().push_back(json(v.y));
    arr->asArray().push_back(json(v.z));
    return arr;
}

std::shared_ptr<JsonValue> serializeVec4(const glm::vec4& v) {
    auto arr = std::make_shared<JsonValue>(JsonArray{});
    arr->asArray().push_back(json(v.x));
    arr->asArray().push_back(json(v.y));
    arr->asArray().push_back(json(v.z));
    arr->asArray().push_back(json(v.w));
    return arr;
}

std::shared_ptr<JsonValue> serializeMat4(const glm::mat4& m) {
    auto arr = std::make_shared<JsonValue>(JsonArray{});
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            arr->asArray().push_back(json(m[i][j]));
        }
    }
    return arr;
}

glm::vec2 deserializeVec2(const JsonValue& v) {
    if (!v.isArray() || v.size() < 2) return glm::vec2(0.0f);
    return glm::vec2(v[0].asFloat(), v[1].asFloat());
}

glm::vec3 deserializeVec3(const JsonValue& v) {
    if (!v.isArray() || v.size() < 3) return glm::vec3(0.0f);
    return glm::vec3(v[0].asFloat(), v[1].asFloat(), v[2].asFloat());
}

glm::vec4 deserializeVec4(const JsonValue& v) {
    if (!v.isArray() || v.size() < 4) return glm::vec4(0.0f);
    return glm::vec4(v[0].asFloat(), v[1].asFloat(), v[2].asFloat(), v[3].asFloat());
}

glm::mat4 deserializeMat4(const JsonValue& v) {
    glm::mat4 m(1.0f);
    if (!v.isArray() || v.size() < 16) return m;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            m[i][j] = v[i * 4 + j].asFloat();
        }
    }
    return m;
}

std::shared_ptr<JsonValue> serializeTransform(const Transform& t) {
    auto obj = std::make_shared<JsonValue>(JsonObject{});
    obj->asObject()["position"] = serializeVec3(t.position);
    obj->asObject()["rotation"] = serializeVec3(t.rotation);
    obj->asObject()["scale"] = serializeVec3(t.scale);
    return obj;
}

Transform deserializeTransform(const JsonValue& v) {
    Transform t;
    if (v.has("position")) t.position = deserializeVec3(v["position"]);
    if (v.has("rotation")) t.rotation = deserializeVec3(v["rotation"]);
    if (v.has("scale")) t.scale = deserializeVec3(v["scale"]);
    return t;
}

std::shared_ptr<JsonValue> serializePhysicsProperties(const PhysicsProperties& p) {
    auto obj = std::make_shared<JsonValue>(JsonObject{});
    obj->asObject()["velocity"] = serializeVec3(p.velocity);
    obj->asObject()["mass"] = json(p.mass);
    obj->asObject()["restitution"] = json(p.restitution);
    obj->asObject()["simulate"] = json(p.simulate);
    obj->asObject()["linearDamping"] = json(p.linearDamping);
    obj->asObject()["angularDamping"] = json(p.angularDamping);
    return obj;
}

PhysicsProperties deserializePhysicsProperties(const JsonValue& v) {
    PhysicsProperties p;
    if (v.has("velocity")) p.velocity = deserializeVec3(v["velocity"]);
    if (v.has("mass")) p.mass = v["mass"].asFloat();
    if (v.has("restitution")) p.restitution = v["restitution"].asFloat();
    if (v.has("simulate")) p.simulate = v["simulate"].asBool();
    if (v.has("linearDamping")) p.linearDamping = v["linearDamping"].asFloat();
    if (v.has("angularDamping")) p.angularDamping = v["angularDamping"].asFloat();
    return p;
}

std::shared_ptr<JsonValue> serializeCollider(const Collider& /*c*/) {
    return std::make_shared<JsonValue>(JsonObject{});
}

Collider deserializeCollider(const JsonValue& /*v*/) {
    return Collider{};
}

std::shared_ptr<JsonValue> serializeRenderComponent(const RenderComponent& /*r*/) {
    return std::make_shared<JsonValue>(JsonObject{});
}

RenderComponent deserializeRenderComponent(const JsonValue& /*v*/) {
    return RenderComponent{};
}

std::shared_ptr<JsonValue> serializeLightComponent(const LightComponent& /*l*/) {
    return std::make_shared<JsonValue>(JsonObject{});
}

LightComponent deserializeLightComponent(const JsonValue& /*v*/) {
    return LightComponent{};
}

std::shared_ptr<JsonValue> serializeMaterial(const Material& /*m*/) {
    return std::make_shared<JsonValue>(JsonObject{});
}

Material deserializeMaterial(const JsonValue& /*v*/) {
    return Material{};
}

} // namespace serialization

// ============================================================================
// PrefabLibrary Implementation
// ============================================================================

Prefab PrefabLibrary::createFromGameObject(const GameObject& /*object*/, const std::string& prefabName) {
    Prefab p;
    p.name = prefabName;
    return p;
}

GameObject& PrefabLibrary::instantiate(Scene& scene, const Prefab& /*prefab*/, const std::string& instanceName) {
    return scene.createObject(instanceName, MeshType::Cube);
}

GameObject& PrefabLibrary::instantiate(Scene& scene, const std::string& prefabName, const std::string& instanceName) {
    const Prefab* p = find(prefabName);
    if (p) {
        return instantiate(scene, *p, instanceName);
    }
    return scene.createObject(instanceName, MeshType::Cube);
}

void PrefabLibrary::add(Prefab prefab) {
    prefabs[prefab.name] = std::move(prefab);
}

bool PrefabLibrary::remove(const std::string& name) {
    return prefabs.erase(name) > 0;
}

bool PrefabLibrary::has(const std::string& name) const {
    return prefabs.find(name) != prefabs.end();
}

const Prefab* PrefabLibrary::find(const std::string& name) const {
    auto it = prefabs.find(name);
    return it != prefabs.end() ? &it->second : nullptr;
}

std::vector<std::string> PrefabLibrary::names() const {
    std::vector<std::string> result;
    result.reserve(prefabs.size());
    for (const auto& [name, _] : prefabs) {
        result.push_back(name);
    }
    return result;
}

bool PrefabLibrary::saveToFile(const std::filesystem::path& /*path*/) const {
    return true; // Stub
}

bool PrefabLibrary::loadFromFile(const std::filesystem::path& /*path*/) {
    return true; // Stub
}

// ============================================================================
// SceneSerializer Implementation
// ============================================================================

std::shared_ptr<JsonValue> SceneSerializer::serialize(const Scene& /*scene*/) const {
    return std::make_shared<JsonValue>(JsonObject{});
}

void SceneSerializer::deserialize(Scene& /*scene*/, const JsonValue& /*data*/) const {
    // Stub
}

bool SceneSerializer::saveToFile(const Scene& scene, const std::filesystem::path& path) const {
    auto data = serialize(scene);
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << data->stringify(2);
    return file.good();
}

bool SceneSerializer::loadFromFile(Scene& scene, const std::filesystem::path& path) const {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    auto data = JsonValue::parse(content);
    if (data) deserialize(scene, *data);
    return true;
}

std::shared_ptr<JsonValue> SceneSerializer::serializeGameObject(const GameObject& /*object*/) const {
    return std::make_shared<JsonValue>(JsonObject{});
}

void SceneSerializer::deserializeGameObject(Scene& /*scene*/, const JsonValue& /*data*/) const {
    // Stub
}

std::shared_ptr<JsonValue> SceneSerializer::serializeLight(const Light& /*light*/) const {
    return std::make_shared<JsonValue>(JsonObject{});
}

void SceneSerializer::deserializeLight(Scene& /*scene*/, const JsonValue& /*data*/) const {
    // Stub
}

void SceneSerializer::registerComponentSerializer(const std::string& typeName, ComponentSerializer serializer) {
    customSerializers[typeName] = std::move(serializer);
}

void SceneSerializer::registerComponentDeserializer(const std::string& typeName, ComponentDeserializer deserializer) {
    customDeserializers[typeName] = std::move(deserializer);
}

// ============================================================================
// UndoRedoManager Implementation
// ============================================================================

UndoRedoManager::UndoRedoManager(std::size_t maxHistory)
    : maxHistorySize(maxHistory)
{
}

void UndoRedoManager::execute(std::unique_ptr<Command> command) {
    command->execute();
    undoStack.push_back(std::move(command));
    redoStack.clear();
    
    while (undoStack.size() > maxHistorySize) {
        undoStack.erase(undoStack.begin());
    }
}

bool UndoRedoManager::canUndo() const {
    return !undoStack.empty();
}

bool UndoRedoManager::canRedo() const {
    return !redoStack.empty();
}

void UndoRedoManager::undo() {
    if (undoStack.empty()) return;
    auto cmd = std::move(undoStack.back());
    undoStack.pop_back();
    cmd->undo();
    redoStack.push_back(std::move(cmd));
}

void UndoRedoManager::redo() {
    if (redoStack.empty()) return;
    auto cmd = std::move(redoStack.back());
    redoStack.pop_back();
    cmd->execute();
    undoStack.push_back(std::move(cmd));
}

void UndoRedoManager::clear() {
    undoStack.clear();
    redoStack.clear();
}

std::string UndoRedoManager::undoDescription() const {
    return undoStack.empty() ? "" : undoStack.back()->description();
}

std::string UndoRedoManager::redoDescription() const {
    return redoStack.empty() ? "" : redoStack.back()->description();
}

// ============================================================================
// Commands Implementation
// ============================================================================

TransformCommand::TransformCommand(GameObject& obj, const Transform& newTrans)
    : object(&obj)
    , oldTransform(obj.transform())
    , newTransform(newTrans)
{
}

void TransformCommand::execute() {
    object->transform() = newTransform;
}

void TransformCommand::undo() {
    object->transform() = oldTransform;
}

std::string TransformCommand::description() const {
    return "Transform Object";
}

CreateObjectCommand::CreateObjectCommand(Scene& s, const std::string& name, MeshType mesh)
    : scene(&s)
    , objectName(name)
    , meshType(mesh)
{
}

void CreateObjectCommand::execute() {
    auto& obj = scene->createObject(objectName, meshType);
    createdEntity = obj.entity();
}

void CreateObjectCommand::undo() {
    // Note: Scene doesn't have removeGameObject - this is a stub
    // In a real implementation, we'd need to add object removal to Scene
    (void)createdEntity;
}

std::string CreateObjectCommand::description() const {
    return "Create Object: " + objectName;
}

DeleteObjectCommand::DeleteObjectCommand(Scene& s, const std::string& objName)
    : scene(&s)
    , objectName(objName)
{
}

void DeleteObjectCommand::execute() {
    // Note: Scene doesn't have removeGameObject - this is a stub
}

void DeleteObjectCommand::undo() {
    scene->createObject(objectName, MeshType::Cube);
}

std::string DeleteObjectCommand::description() const {
    return "Delete Object: " + objectName;
}

} // namespace vkengine
