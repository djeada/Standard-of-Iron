#pragma once

#include "entity.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Engine::Core {

class Event {
public:
  virtual ~Event() = default;
};

template <typename T> using EventHandler = std::function<void(const T &)>;

using SubscriptionHandle = std::size_t;

class EventManager {
public:
  static EventManager &instance() {
    static EventManager inst;
    return inst;
  }

  template <typename T> SubscriptionHandle subscribe(EventHandler<T> handler) {
    static_assert(std::is_base_of_v<Event, T>, "T must inherit from Event");
    SubscriptionHandle handle = m_nextHandle++;
    auto wrapper = [handler, handle](const void *event) {
      handler(*static_cast<const T *>(event));
    };
    HandlerEntry entry{handle, wrapper};
    m_handlers[std::type_index(typeid(T))].push_back(entry);
    return handle;
  }

  template <typename T> void unsubscribe(SubscriptionHandle handle) {
    static_assert(std::is_base_of_v<Event, T>, "T must inherit from Event");
    auto it = m_handlers.find(std::type_index(typeid(T)));
    if (it != m_handlers.end()) {
      auto &handlers = it->second;
      handlers.erase(std::remove_if(handlers.begin(), handlers.end(),
                                    [handle](const HandlerEntry &e) {
                                      return e.handle == handle;
                                    }),
                     handlers.end());
    }
  }

  template <typename T> void publish(const T &event) {
    static_assert(std::is_base_of_v<Event, T>, "T must inherit from Event");
    auto it = m_handlers.find(std::type_index(typeid(T)));
    if (it != m_handlers.end()) {
      for (const auto &entry : it->second) {
        entry.handler(&event);
      }
    }
  }

private:
  struct HandlerEntry {
    SubscriptionHandle handle;
    std::function<void(const void *)> handler;
  };

  std::unordered_map<std::type_index, std::vector<HandlerEntry>> m_handlers;
  SubscriptionHandle m_nextHandle = 1;
};

template <typename T> class ScopedEventSubscription {
public:
  ScopedEventSubscription() : m_handle(0) {}

  ScopedEventSubscription(EventHandler<T> handler)
      : m_handle(EventManager::instance().subscribe<T>(handler)) {}

  ~ScopedEventSubscription() { unsubscribe(); }

  ScopedEventSubscription(const ScopedEventSubscription &) = delete;
  ScopedEventSubscription &operator=(const ScopedEventSubscription &) = delete;

  ScopedEventSubscription(ScopedEventSubscription &&other) noexcept
      : m_handle(other.m_handle) {
    other.m_handle = 0;
  }

  ScopedEventSubscription &operator=(ScopedEventSubscription &&other) noexcept {
    if (this != &other) {
      unsubscribe();
      m_handle = other.m_handle;
      other.m_handle = 0;
    }
    return *this;
  }

  void unsubscribe() {
    if (m_handle != 0) {
      EventManager::instance().unsubscribe<T>(m_handle);
      m_handle = 0;
    }
  }

private:
  SubscriptionHandle m_handle;
};

class UnitSelectedEvent : public Event {
public:
  UnitSelectedEvent(EntityID unitId) : unitId(unitId) {}
  EntityID unitId;
};

class UnitMovedEvent : public Event {
public:
  UnitMovedEvent(EntityID unitId, float x, float y)
      : unitId(unitId), x(x), y(y) {}
  EntityID unitId;
  float x, y;
};

class UnitDiedEvent : public Event {
public:
  UnitDiedEvent(EntityID unitId, int ownerId, const std::string &unitType,
                EntityID killerId = 0, int killerOwnerId = 0)
      : unitId(unitId), ownerId(ownerId), unitType(unitType),
        killerId(killerId), killerOwnerId(killerOwnerId) {}
  EntityID unitId;
  int ownerId;
  std::string unitType;
  EntityID killerId;
  int killerOwnerId;
};

class UnitSpawnedEvent : public Event {
public:
  UnitSpawnedEvent(EntityID unitId, int ownerId, const std::string &unitType)
      : unitId(unitId), ownerId(ownerId), unitType(unitType) {}
  EntityID unitId;
  int ownerId;
  std::string unitType;
};

class BuildingAttackedEvent : public Event {
public:
  BuildingAttackedEvent(EntityID buildingId, int ownerId,
                        const std::string &buildingType,
                        EntityID attackerId = 0, int attackerOwnerId = 0,
                        int damage = 0)
      : buildingId(buildingId), ownerId(ownerId), buildingType(buildingType),
        attackerId(attackerId), attackerOwnerId(attackerOwnerId),
        damage(damage) {}
  EntityID buildingId;
  int ownerId;
  std::string buildingType;
  EntityID attackerId;
  int attackerOwnerId;
  int damage;
};

class BarrackCapturedEvent : public Event {
public:
  BarrackCapturedEvent(EntityID barrackId, int previousOwnerId, int newOwnerId)
      : barrackId(barrackId), previousOwnerId(previousOwnerId),
        newOwnerId(newOwnerId) {}
  EntityID barrackId;
  int previousOwnerId;
  int newOwnerId;
};

class BattleStartedEvent : public Event {
public:
  BattleStartedEvent(EntityID attackerId, EntityID defenderId,
                     float posX = 0.0f, float posY = 0.0f)
      : attackerId(attackerId), defenderId(defenderId), posX(posX), posY(posY) {
  }
  EntityID attackerId;
  EntityID defenderId;
  float posX;
  float posY;
};

class BattleEndedEvent : public Event {
public:
  BattleEndedEvent(EntityID winnerId, EntityID loserId,
                   bool defenderDied = false)
      : winnerId(winnerId), loserId(loserId), defenderDied(defenderDied) {}
  EntityID winnerId;
  EntityID loserId;
  bool defenderDied;
};

enum class AmbientState { PEACEFUL, TENSE, COMBAT, VICTORY, DEFEAT };

class AmbientStateChangedEvent : public Event {
public:
  AmbientStateChangedEvent(AmbientState newState, AmbientState previousState)
      : newState(newState), previousState(previousState) {}
  AmbientState newState;
  AmbientState previousState;
};

class AudioTriggerEvent : public Event {
public:
  AudioTriggerEvent(const std::string &soundId, float volume = 1.0f,
                    bool loop = false, int priority = 0)
      : soundId(soundId), volume(volume), loop(loop), priority(priority) {}
  std::string soundId;
  float volume;
  bool loop;
  int priority;
};

class MusicTriggerEvent : public Event {
public:
  MusicTriggerEvent(const std::string &musicId, float volume = 1.0f,
                    bool crossfade = true)
      : musicId(musicId), volume(volume), crossfade(crossfade) {}
  std::string musicId;
  float volume;
  bool crossfade;
};

} // namespace Engine::Core
