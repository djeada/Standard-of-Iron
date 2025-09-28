#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <typeindex>
#include <unordered_map>

namespace Engine::Core {

using EntityID = std::uint32_t;
constexpr EntityID NULL_ENTITY = 0;

class Component {
public:
    virtual ~Component() = default;
};

class Entity {
public:
    Entity(EntityID id) : m_id(id) {}
    
    EntityID getId() const { return m_id; }
    
    template<typename T, typename... Args>
    T* addComponent(Args&&... args);
    
    template<typename T>
    T* getComponent();
    
    template<typename T>
    const T* getComponent() const;
    
    template<typename T>
    void removeComponent();
    
    template<typename T>
    bool hasComponent() const;

private:
    EntityID m_id;
    std::unordered_map<std::type_index, std::unique_ptr<Component>> m_components;
};

template<typename T, typename... Args>
T* Entity::addComponent(Args&&... args) {
    static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
    auto component = std::make_unique<T>(std::forward<Args>(args)...);
    auto ptr = component.get();
    m_components[std::type_index(typeid(T))] = std::move(component);
    return ptr;
}

template<typename T>
T* Entity::getComponent() {
    auto it = m_components.find(std::type_index(typeid(T)));
    if (it != m_components.end()) {
        return static_cast<T*>(it->second.get());
    }
    return nullptr;
}

template<typename T>
const T* Entity::getComponent() const {
    auto it = m_components.find(std::type_index(typeid(T)));
    if (it != m_components.end()) {
        return static_cast<const T*>(it->second.get());
    }
    return nullptr;
}

template<typename T>
void Entity::removeComponent() {
    m_components.erase(std::type_index(typeid(T)));
}

template<typename T>
bool Entity::hasComponent() const {
    return m_components.find(std::type_index(typeid(T))) != m_components.end();
}

} // namespace Engine::Core