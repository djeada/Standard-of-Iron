#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <type_traits>

namespace Engine::Core {

using EntityID = std::uint32_t;
constexpr EntityID NULL_ENTITY = 0;

class Component {
public:
    virtual ~Component() = default;
};

class Entity {
public:
    Entity(EntityID id);
    
    EntityID getId() const;
    
    template<typename T, typename... Args>
    T* addComponent(Args&&... args) {
        static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        auto ptr = component.get();
        m_components[std::type_index(typeid(T))] = std::move(component);
        return ptr;
    }
    
    template<typename T>
    T* getComponent() {
        auto it = m_components.find(std::type_index(typeid(T)));
        if (it != m_components.end()) {
            return static_cast<T*>(it->second.get());
        }
        return nullptr;
    }
    
    template<typename T>
    const T* getComponent() const {
        auto it = m_components.find(std::type_index(typeid(T)));
        if (it != m_components.end()) {
            return static_cast<const T*>(it->second.get());
        }
        return nullptr;
    }
    
    template<typename T>
    void removeComponent() {
        m_components.erase(std::type_index(typeid(T)));
    }
    
    template<typename T>
    bool hasComponent() const {
        return m_components.find(std::type_index(typeid(T))) != m_components.end();
    }

private:
    EntityID m_id;
    std::unordered_map<std::type_index, std::unique_ptr<Component>> m_components;
};

} // namespace Engine::Core
