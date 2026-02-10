#pragma once

#include "core/ecs/Entity.hpp"

#include <algorithm>
#include <map>
#include <mutex>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

#include <stdexcept>

namespace core::ecs {

class Registry {
public:
    Registry() = default;

    Entity createEntity();
    void destroyEntity(Entity entity);

    void clear();

    [[nodiscard]] const std::vector<EntityId>& entities() const noexcept { return activeEntities; }
    [[nodiscard]] bool contains(Entity entity) const noexcept
    {
        if (!entity) {
            return false;
        }
        return std::find(activeEntities.begin(), activeEntities.end(), entity.id) != activeEntities.end();
    }

    template <typename Component, typename... Args>
    Component& emplace(Entity entity, Args&&... args)
    {
        std::scoped_lock lock(registryMutex);
        auto& pool = poolFor<Component>();
        return pool.emplace(entity.id, std::forward<Args>(args)...);
    }

    template <typename Component>
    bool has(Entity entity) const noexcept
    {
        std::scoped_lock lock(registryMutex);
        const auto* pool = findPool<Component>();
        if (!pool) {
            return false;
        }
        return pool->contains(entity.id);
    }

    template <typename Component>
    Component& get(Entity entity)
    {
        std::scoped_lock lock(registryMutex);
        auto* pool = findPool<Component>();
        if (!pool) {
            throw std::runtime_error("Component not present on entity");
        }
        return pool->get(entity.id);
    }

    template <typename Component>
    const Component& get(Entity entity) const
    {
        std::scoped_lock lock(registryMutex);
        const auto* pool = findPool<Component>();
        if (!pool) {
            throw std::runtime_error("Component not present on entity");
        }
        return pool->get(entity.id);
    }

    template <typename Component>
    Component* tryGet(Entity entity) noexcept
    {
        std::scoped_lock lock(registryMutex);
        auto* pool = findPool<Component>();
        if (!pool) {
            return nullptr;
        }
        return pool->tryGet(entity.id);
    }

    template <typename Component>
    const Component* tryGet(Entity entity) const noexcept
    {
        std::scoped_lock lock(registryMutex);
        const auto* pool = findPool<Component>();
        if (!pool) {
            return nullptr;
        }
        return pool->tryGet(entity.id);
    }

    template <typename Component>
    void remove(Entity entity)
    {
        std::scoped_lock lock(registryMutex);
        auto* pool = findPool<Component>();
        if (!pool) {
            return;
        }
        pool->remove(entity.id);
    }

    template <typename Component, typename... Args>
    Component& getOrEmplace(Entity entity, Args&&... args)
    {
        std::scoped_lock lock(registryMutex);
        if (auto* existing = tryGet<Component>(entity)) {
            return *existing;
        }
        return emplace<Component>(entity, std::forward<Args>(args)...);
    }

    template <typename... Components, typename Func>
    void view(Func&& func)
    {
        std::scoped_lock lock(registryMutex);
        for (EntityId id : activeEntities) {
            const Entity entity{id};
            if (((has<Components>(entity)) && ...)) {
                func(entity, get<Components>(entity)...);
            }
        }
    }

private:
    struct IComponentPool {
        virtual ~IComponentPool() = default;
        virtual void remove(EntityId entity) = 0;
        virtual void clear() = 0;
    };

    template <typename Component>
    struct ComponentPool final : IComponentPool {
        Component& emplace(EntityId entity, Component&& value)
        {
            auto [it, _] = components.emplace(entity, std::move(value));
            return it->second;
        }

        template <typename... Args>
        Component& emplace(EntityId entity, Args&&... args)
        {
            auto [it, inserted] = components.try_emplace(entity, std::forward<Args>(args)...);
            if (!inserted) {
                it->second = Component(std::forward<Args>(args)...);
            }
            return it->second;
        }

        bool contains(EntityId entity) const noexcept
        {
            return components.find(entity) != components.end();
        }

        Component& get(EntityId entity)
        {
            auto it = components.find(entity);
            if (it == components.end()) {
                throw std::runtime_error("Component missing on entity");
            }
            return it->second;
        }

        const Component& get(EntityId entity) const
        {
            auto it = components.find(entity);
            if (it == components.end()) {
                throw std::runtime_error("Component missing on entity");
            }
            return it->second;
        }

        Component* tryGet(EntityId entity) noexcept
        {
            auto it = components.find(entity);
            if (it == components.end()) {
                return nullptr;
            }
            return &it->second;
        }

        const Component* tryGet(EntityId entity) const noexcept
        {
            auto it = components.find(entity);
            if (it == components.end()) {
                return nullptr;
            }
            return &it->second;
        }

        void remove(EntityId entity) override
        {
            components.erase(entity);
        }

        void clear() override
        {
            components.clear();
        }

    std::unordered_map<EntityId, Component> components;
    };

    template <typename Component>
    ComponentPool<Component>* findPool() noexcept
    {
        const auto it = pools.find(std::type_index(typeid(Component)));
        if (it == pools.end()) {
            return nullptr;
        }
        return static_cast<ComponentPool<Component>*>(it->second.get());
    }

    template <typename Component>
    const ComponentPool<Component>* findPool() const noexcept
    {
        const auto it = pools.find(std::type_index(typeid(Component)));
        if (it == pools.end()) {
            return nullptr;
        }
        return static_cast<const ComponentPool<Component>*>(it->second.get());
    }

    template <typename Component>
    ComponentPool<Component>& poolFor()
    {
        auto key = std::type_index(typeid(Component));
        auto it = pools.find(key);
        if (it == pools.end()) {
            auto pool = std::make_unique<ComponentPool<Component>>();
            auto* rawPtr = pool.get();
            pools.emplace(key, std::move(pool));
            return *rawPtr;
        }
        return *static_cast<ComponentPool<Component>*>(it->second.get());
    }

private:
    EntityId nextEntity{1};
    std::vector<EntityId> activeEntities;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> pools;
    mutable std::recursive_mutex registryMutex;
};

inline Entity Registry::createEntity()
{
    std::scoped_lock lock(registryMutex);
    Entity entity{nextEntity++};
    activeEntities.push_back(entity.id);
    return entity;
}

inline void Registry::destroyEntity(Entity entity)
{
    std::scoped_lock lock(registryMutex);
    if (!entity) {
        return;
    }
    for (auto& [_, pool] : pools) {
        pool->remove(entity.id);
    }
    activeEntities.erase(std::remove(activeEntities.begin(), activeEntities.end(), entity.id), activeEntities.end());
}

inline void Registry::clear()
{
    std::scoped_lock lock(registryMutex);
    activeEntities.clear();
    for (auto& [_, pool] : pools) {
        pool->clear();
    }
    nextEntity = 1;
}

} // namespace core::ecs
