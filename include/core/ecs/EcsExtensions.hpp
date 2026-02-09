#pragma once

#include "core/ecs/Entity.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace core::ecs {

// ============================================================================
// Event System
// ============================================================================

using EventId = std::uint32_t;

struct Event {
    EventId typeId{0};
    Entity entity{};
    virtual ~Event() = default;
};

template<typename T>
struct TypedEvent : Event {
    T data;
    TypedEvent(const T& d) : data(d) {
        typeId = getEventTypeId<T>();
    }
};

template<typename T>
inline EventId getEventTypeId() {
    static EventId id = []() {
        static EventId counter = 0;
        return ++counter;
    }();
    return id;
}

using EventCallback = std::function<void(const Event&)>;

class EventDispatcher {
public:
    template<typename T>
    void subscribe(std::function<void(const T&)> callback) {
        EventId id = getEventTypeId<T>();
        callbacks[id].push_back([callback](const Event& e) {
            callback(static_cast<const TypedEvent<T>&>(e).data);
        });
    }

    template<typename T>
    void emit(const T& eventData, Entity entity = {}) {
        TypedEvent<T> event(eventData);
        event.entity = entity;
        EventId id = getEventTypeId<T>();
        auto it = callbacks.find(id);
        if (it != callbacks.end()) {
            for (auto& cb : it->second) {
                cb(event);
            }
        }
    }

    void clear() { callbacks.clear(); }

private:
    std::unordered_map<EventId, std::vector<EventCallback>> callbacks;
};

// ============================================================================
// Component Events
// ============================================================================

template<typename T>
struct ComponentAddedEvent {
    Entity entity;
    T* component;
};

template<typename T>
struct ComponentRemovedEvent {
    Entity entity;
};

struct EntityCreatedEvent {
    Entity entity;
};

struct EntityDestroyedEvent {
    Entity entity;
};

// ============================================================================
// Component Query / View
// ============================================================================

template<typename... Components>
class ComponentView {
public:
    class Iterator {
    public:
        Iterator(std::vector<Entity>::iterator it, std::vector<Entity>::iterator end, class Registry* reg)
            : current(it), endIt(end), registry(reg) {
            skipInvalid();
        }

        std::tuple<Entity, Components&...> operator*() {
            return std::tuple<Entity, Components&...>(*current, registry->get<Components>(*current)...);
        }

        Iterator& operator++() {
            ++current;
            skipInvalid();
            return *this;
        }

        bool operator!=(const Iterator& other) const {
            return current != other.current;
        }

    private:
        void skipInvalid() {
            while (current != endIt && !hasAllComponents()) {
                ++current;
            }
        }

        bool hasAllComponents() const {
            return (registry->has<Components>(Entity{*current}) && ...);
        }

        std::vector<Entity>::iterator current;
        std::vector<Entity>::iterator endIt;
        class Registry* registry;
    };

    ComponentView(std::vector<Entity>& entities, class Registry* reg)
        : entityList(entities), registry(reg) {}

    Iterator begin() { return Iterator(entityList.begin(), entityList.end(), registry); }
    Iterator end() { return Iterator(entityList.end(), entityList.end(), registry); }

    std::size_t count() {
        std::size_t c = 0;
        for (auto it = begin(); it != end(); ++it) ++c;
        return c;
    }

    void each(std::function<void(Entity, Components&...)> func) {
        for (auto [entity, components...] : *this) {
            std::apply([&func, entity](auto&... comps) {
                func(entity, comps...);
            }, std::tie(components...));
        }
    }

private:
    std::vector<Entity>& entityList;
    class Registry* registry;
};

// ============================================================================
// Entity Hierarchy
// ============================================================================

struct HierarchyComponent {
    Entity parent{};
    Entity firstChild{};
    Entity nextSibling{};
    Entity prevSibling{};
    std::size_t depth{0};
};

class EntityHierarchy {
public:
    void setParent(class Registry& registry, Entity child, Entity parent);
    void removeParent(class Registry& registry, Entity child);
    void destroyWithChildren(class Registry& registry, Entity entity);

    [[nodiscard]] Entity getParent(const class Registry& registry, Entity entity) const;
    [[nodiscard]] std::vector<Entity> getChildren(const class Registry& registry, Entity entity) const;
    [[nodiscard]] std::vector<Entity> getDescendants(const class Registry& registry, Entity entity) const;
    [[nodiscard]] std::vector<Entity> getAncestors(const class Registry& registry, Entity entity) const;
    [[nodiscard]] Entity getRoot(const class Registry& registry, Entity entity) const;
    [[nodiscard]] bool isAncestorOf(const class Registry& registry, Entity ancestor, Entity descendant) const;
    [[nodiscard]] std::size_t getDepth(const class Registry& registry, Entity entity) const;

    // Traverse hierarchy
    void traverseDepthFirst(const class Registry& registry, Entity root,
                            std::function<void(Entity, std::size_t depth)> visitor) const;
    void traverseBreadthFirst(const class Registry& registry, Entity root,
                              std::function<void(Entity, std::size_t depth)> visitor) const;

private:
    void addChild(class Registry& registry, Entity parent, Entity child);
    void removeChild(class Registry& registry, Entity parent, Entity child);
};

// ============================================================================
// Deferred Operations
// ============================================================================

class DeferredOperations {
public:
    void createEntity(std::function<void(Entity)> setup);
    void destroyEntity(Entity entity);
    
    template<typename T, typename... Args>
    void addComponent(Entity entity, Args&&... args) {
        componentAdds.push_back([entity, args = std::make_tuple(std::forward<Args>(args)...)](class Registry& reg) mutable {
            std::apply([&reg, entity](auto&&... a) {
                reg.emplace<T>(entity, std::forward<decltype(a)>(a)...);
            }, std::move(args));
        });
    }

    template<typename T>
    void removeComponent(Entity entity) {
        componentRemoves.push_back([entity](class Registry& reg) {
            reg.remove<T>(entity);
        });
    }

    void flush(class Registry& registry);
    void clear();
    [[nodiscard]] bool hasPending() const;

private:
    std::vector<std::function<void(Entity)>> entityCreates;
    std::vector<Entity> entityDestroys;
    std::vector<std::function<void(class Registry&)>> componentAdds;
    std::vector<std::function<void(class Registry&)>> componentRemoves;
};

// ============================================================================
// System Base Class
// ============================================================================

class System {
public:
    virtual ~System() = default;

    virtual void initialize(class Registry& registry) {}
    virtual void update(class Registry& registry, float deltaSeconds) = 0;
    virtual void shutdown(class Registry& registry) {}

    void setEnabled(bool enabled) { isEnabled = enabled; }
    [[nodiscard]] bool enabled() const { return isEnabled; }
    void setPriority(int priority) { updatePriority = priority; }
    [[nodiscard]] int priority() const { return updatePriority; }

protected:
    bool isEnabled{true};
    int updatePriority{0};
};

// ============================================================================
// System Manager
// ============================================================================

class SystemManager {
public:
    template<typename T, typename... Args>
    T* addSystem(Args&&... args) {
        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = system.get();
        systems.push_back(std::move(system));
        sortSystems();
        return ptr;
    }

    template<typename T>
    T* getSystem() {
        for (auto& sys : systems) {
            if (auto* ptr = dynamic_cast<T*>(sys.get())) {
                return ptr;
            }
        }
        return nullptr;
    }

    template<typename T>
    void removeSystem() {
        systems.erase(
            std::remove_if(systems.begin(), systems.end(),
                [](const std::unique_ptr<System>& sys) {
                    return dynamic_cast<T*>(sys.get()) != nullptr;
                }),
            systems.end()
        );
    }

    void initialize(class Registry& registry) {
        for (auto& sys : systems) {
            sys->initialize(registry);
        }
    }

    void update(class Registry& registry, float deltaSeconds) {
        for (auto& sys : systems) {
            if (sys->enabled()) {
                sys->update(registry, deltaSeconds);
            }
        }
    }

    void shutdown(class Registry& registry) {
        for (auto& sys : systems) {
            sys->shutdown(registry);
        }
    }

private:
    void sortSystems() {
        std::sort(systems.begin(), systems.end(),
            [](const std::unique_ptr<System>& a, const std::unique_ptr<System>& b) {
                return a->priority() < b->priority();
            });
    }

    std::vector<std::unique_ptr<System>> systems;
};

// ============================================================================
// Component Group (for archetype-like queries)
// ============================================================================

template<typename... Components>
class ComponentGroup {
public:
    struct Archetype {
        std::vector<Entity> entities;
        std::tuple<std::vector<Components>...> components;
    };

    void add(Entity entity, Components... comps) {
        archetype.entities.push_back(entity);
        (std::get<std::vector<Components>>(archetype.components).push_back(std::move(comps)), ...);
        entityIndex[entity.id] = archetype.entities.size() - 1;
    }

    void remove(Entity entity) {
        auto it = entityIndex.find(entity.id);
        if (it == entityIndex.end()) return;

        std::size_t index = it->second;
        std::size_t lastIndex = archetype.entities.size() - 1;

        if (index != lastIndex) {
            // Swap with last
            archetype.entities[index] = archetype.entities[lastIndex];
            ((std::get<std::vector<Components>>(archetype.components)[index] = 
              std::move(std::get<std::vector<Components>>(archetype.components)[lastIndex])), ...);
            entityIndex[archetype.entities[index].id] = index;
        }

        archetype.entities.pop_back();
        (std::get<std::vector<Components>>(archetype.components).pop_back(), ...);
        entityIndex.erase(entity.id);
    }

    bool contains(Entity entity) const {
        return entityIndex.find(entity.id) != entityIndex.end();
    }

    template<typename C>
    C& get(Entity entity) {
        return std::get<std::vector<C>>(archetype.components)[entityIndex.at(entity.id)];
    }

    template<typename Func>
    void each(Func&& func) {
        for (std::size_t i = 0; i < archetype.entities.size(); ++i) {
            func(archetype.entities[i], 
                 std::get<std::vector<Components>>(archetype.components)[i]...);
        }
    }

    std::size_t size() const { return archetype.entities.size(); }

private:
    Archetype archetype;
    std::unordered_map<EntityId, std::size_t> entityIndex;
};

// ============================================================================
// Tag Components (empty components for filtering)
// ============================================================================

struct TagStatic {};
struct TagDynamic {};
struct TagPlayer {};
struct TagEnemy {};
struct TagTrigger {};
struct TagDisabled {};

// ============================================================================
// Singleton Components
// ============================================================================

template<typename T>
class SingletonComponent {
public:
    static T& get(class Registry& registry);
    static bool exists(const class Registry& registry);
    static void set(class Registry& registry, T value);
    static void remove(class Registry& registry);
};

// ============================================================================
// Component Serialization Interface
// ============================================================================

class IComponentSerializer {
public:
    virtual ~IComponentSerializer() = default;
    virtual void serialize(const void* component, class JsonValue& output) const = 0;
    virtual void deserialize(void* component, const class JsonValue& input) const = 0;
    virtual std::type_index typeIndex() const = 0;
};

template<typename T>
class ComponentSerializer : public IComponentSerializer {
public:
    using SerializeFunc = std::function<void(const T&, class JsonValue&)>;
    using DeserializeFunc = std::function<void(T&, const class JsonValue&)>;

    ComponentSerializer(SerializeFunc s, DeserializeFunc d)
        : serializeFunc(std::move(s)), deserializeFunc(std::move(d)) {}

    void serialize(const void* component, class JsonValue& output) const override {
        serializeFunc(*static_cast<const T*>(component), output);
    }

    void deserialize(void* component, const class JsonValue& input) const override {
        deserializeFunc(*static_cast<T*>(component), input);
    }

    std::type_index typeIndex() const override {
        return std::type_index(typeid(T));
    }

private:
    SerializeFunc serializeFunc;
    DeserializeFunc deserializeFunc;
};

class ComponentSerializerRegistry {
public:
    static ComponentSerializerRegistry& instance();

    template<typename T>
    void registerSerializer(
        std::function<void(const T&, class JsonValue&)> serialize,
        std::function<void(T&, const class JsonValue&)> deserialize) {
        serializers[std::type_index(typeid(T))] = 
            std::make_unique<ComponentSerializer<T>>(std::move(serialize), std::move(deserialize));
    }

    template<typename T>
    const IComponentSerializer* getSerializer() const {
        auto it = serializers.find(std::type_index(typeid(T)));
        return it != serializers.end() ? it->second.get() : nullptr;
    }

private:
    std::unordered_map<std::type_index, std::unique_ptr<IComponentSerializer>> serializers;
};

} // namespace core::ecs
