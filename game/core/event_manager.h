#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <memory>
#include "entity.h"

namespace Engine::Core {

class Event {
public:
    virtual ~Event() = default;
};

template<typename T>
using EventHandler = std::function<void(const T&)>;

class EventManager {
public:
    template<typename T>
    void subscribe(EventHandler<T> handler) {
        static_assert(std::is_base_of_v<Event, T>, "T must inherit from Event");
        auto wrapper = [handler](const void* event) {
            handler(*static_cast<const T*>(event));
        };
        m_handlers[std::type_index(typeid(T))].push_back(wrapper);
    }
    
    template<typename T>
    void publish(const T& event) {
        static_assert(std::is_base_of_v<Event, T>, "T must inherit from Event");
        auto it = m_handlers.find(std::type_index(typeid(T)));
        if (it != m_handlers.end()) {
            for (const auto& handler : it->second) {
                handler(&event);
            }
        }
    }
    
private:
    std::unordered_map<std::type_index, std::vector<std::function<void(const void*)>>> m_handlers;
};

// Common game events
class UnitSelectedEvent : public Event {
public:
    UnitSelectedEvent(EntityID unitId) : unitId(unitId) {}
    EntityID unitId;
};

class UnitMovedEvent : public Event {
public:
    UnitMovedEvent(EntityID unitId, float x, float y) : unitId(unitId), x(x), y(y) {}
    EntityID unitId;
    float x, y;
};

} // namespace Engine::Core
