#pragma once

#include "../units/spawn_type.h"
#include "entity.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Engine::Core {

class Event {
public:
  virtual ~Event() = default;
  [[nodiscard]] virtual auto get_type_name() const -> const char * {
    return "Event";
  }
};

template <typename T> using EventHandler = std::function<void(const T &)>;

using SubscriptionHandle = std::size_t;

struct EventStats {
  size_t publish_count = 0;
  size_t subscriber_count = 0;
};

class EventManager {
public:
  static auto instance() -> EventManager & {
    static EventManager inst;
    return inst;
  }

  template <typename T>
  auto subscribe(EventHandler<T> handler) -> SubscriptionHandle {
    static_assert(std::is_base_of_v<Event, T>, "T must inherit from Event");
    std::lock_guard<std::mutex> const lock(m_mutex);

    SubscriptionHandle const handle = m_nextHandle++;
    auto wrapper = [handler, handle](const void *event) {
      handler(*static_cast<const T *>(event));
    };
    HandlerEntry const entry{handle, wrapper};
    m_handlers[std::type_index(typeid(T))].push_back(entry);

    m_stats[std::type_index(typeid(T))].subscriber_count++;

    return handle;
  }

  template <typename T> void unsubscribe(SubscriptionHandle handle) {
    static_assert(std::is_base_of_v<Event, T>, "T must inherit from Event");
    std::lock_guard<std::mutex> const lock(m_mutex);

    auto it = m_handlers.find(std::type_index(typeid(T)));
    if (it != m_handlers.end()) {
      auto &handlers = it->second;
      auto sizeBefore = handlers.size();
      handlers.erase(std::remove_if(handlers.begin(), handlers.end(),
                                    [handle](const HandlerEntry &e) {
                                      return e.handle == handle;
                                    }),
                     handlers.end());

      if (handlers.size() < sizeBefore) {
        m_stats[std::type_index(typeid(T))].subscriber_count--;
      }
    }
  }

  template <typename T> void publish(const T &event) {
    static_assert(std::is_base_of_v<Event, T>, "T must inherit from Event");
    std::vector<HandlerEntry> handlersCopy;

    {
      std::lock_guard<std::mutex> const lock(m_mutex);
      auto it = m_handlers.find(std::type_index(typeid(T)));
      if (it != m_handlers.end()) {
        handlersCopy = it->second;
        m_stats[std::type_index(typeid(T))].publish_count++;
      }
    }

    for (const auto &entry : handlersCopy) {
      entry.handler(&event);
    }
  }

  auto get_stats(const std::type_index &event_type) const -> EventStats {
    std::lock_guard<std::mutex> const lock(m_mutex);
    auto it = m_stats.find(event_type);
    if (it != m_stats.end()) {
      return it->second;
    }
    return EventStats{};
  }

  auto get_subscriber_count(const std::type_index &event_type) const -> size_t {
    std::lock_guard<std::mutex> const lock(m_mutex);
    auto it = m_handlers.find(event_type);
    if (it != m_handlers.end()) {
      return it->second.size();
    }
    return 0;
  }

  void clear_all_subscriptions() {
    std::lock_guard<std::mutex> const lock(m_mutex);
    m_handlers.clear();
    m_stats.clear();
  }

private:
  struct HandlerEntry {
    SubscriptionHandle handle;
    std::function<void(const void *)> handler;
  };

  mutable std::mutex m_mutex;
  std::unordered_map<std::type_index, std::vector<HandlerEntry>> m_handlers;
  std::unordered_map<std::type_index, EventStats> m_stats;
  SubscriptionHandle m_nextHandle = 1;
};

template <typename T> class ScopedEventSubscription {
public:
  ScopedEventSubscription() : m_handle(0) {}

  ScopedEventSubscription(EventHandler<T> handler)
      : m_handle(EventManager::instance().subscribe<T>(handler)) {}

  ~ScopedEventSubscription() { unsubscribe(); }

  ScopedEventSubscription(const ScopedEventSubscription &) = delete;
  auto operator=(const ScopedEventSubscription &) -> ScopedEventSubscription & =
                                                         delete;

  ScopedEventSubscription(ScopedEventSubscription &&other) noexcept
      : m_handle(other.m_handle) {
    other.m_handle = 0;
  }

  auto operator=(ScopedEventSubscription &&other) noexcept
      -> ScopedEventSubscription & {
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
  UnitSelectedEvent(EntityID unit_id) : unit_id(unit_id) {}
  EntityID unit_id;
  [[nodiscard]] auto get_type_name() const -> const char * override {
    return "UNIT_SELECTED";
  }
};

class UnitMovedEvent : public Event {
public:
  UnitMovedEvent(EntityID unit_id, float x, float y)
      : unit_id(unit_id), x(x), y(y) {}
  EntityID unit_id;
  float x, y;
};

class UnitDiedEvent : public Event {
public:
  UnitDiedEvent(EntityID unit_id, int owner_id,
                Game::Units::SpawnType spawn_type, EntityID killer_id = 0,
                int killer_owner_id = 0)
      : unit_id(unit_id), owner_id(owner_id), spawn_type(spawn_type),
        killer_id(killer_id), killer_owner_id(killer_owner_id) {}
  EntityID unit_id;
  int owner_id;
  Game::Units::SpawnType spawn_type;
  EntityID killer_id;
  int killer_owner_id;
};

class UnitSpawnedEvent : public Event {
public:
  UnitSpawnedEvent(EntityID unit_id, int owner_id,
                   Game::Units::SpawnType spawn_type,
                   bool is_initial_spawn = true)
      : unit_id(unit_id), owner_id(owner_id), spawn_type(spawn_type),
        is_initial_spawn(is_initial_spawn) {}
  EntityID unit_id;
  int owner_id;
  Game::Units::SpawnType spawn_type;
  bool is_initial_spawn;
};

class BuildingAttackedEvent : public Event {
public:
  BuildingAttackedEvent(EntityID building_id, int owner_id,
                        Game::Units::SpawnType building_type,
                        EntityID attacker_id = 0, int attacker_owner_id = 0,
                        int damage = 0)
      : building_id(building_id), owner_id(owner_id),
        building_type(building_type), attacker_id(attacker_id),
        attacker_owner_id(attacker_owner_id), damage(damage) {}
  EntityID building_id;
  int owner_id;
  Game::Units::SpawnType building_type;
  EntityID attacker_id;
  int attacker_owner_id;
  int damage;
};

class BarrackCapturedEvent : public Event {
public:
  BarrackCapturedEvent(EntityID barrack_id, int previous_owner_id,
                       int new_owner_id)
      : barrack_id(barrack_id), previous_owner_id(previous_owner_id),
        new_owner_id(new_owner_id) {}
  EntityID barrack_id;
  int previous_owner_id;
  int new_owner_id;
};

enum class AmbientState { PEACEFUL, TENSE, COMBAT, VICTORY, DEFEAT };

class AmbientStateChangedEvent : public Event {
public:
  AmbientStateChangedEvent(AmbientState new_state, AmbientState previous_state)
      : new_state(new_state), previous_state(previous_state) {}
  AmbientState new_state;
  AmbientState previous_state;
  [[nodiscard]] auto get_type_name() const -> const char * override {
    return "AMBIENT_STATE_CHANGED";
  }
};

class AudioTriggerEvent : public Event {
public:
  AudioTriggerEvent(std::string sound_id, float volume = 1.0F,
                    bool loop = false, int priority = 0)
      : sound_id(std::move(sound_id)), volume(volume), loop(loop),
        priority(priority) {}
  std::string sound_id;
  float volume;
  bool loop;
  int priority;
};

class MusicTriggerEvent : public Event {
public:
  MusicTriggerEvent(std::string music_id, float volume = 1.0F,
                    bool crossfade = true)
      : music_id(std::move(music_id)), volume(volume), crossfade(crossfade) {}
  std::string music_id;
  float volume;
  bool crossfade;
};

class CombatHitEvent : public Event {
public:
  CombatHitEvent(EntityID attacker_id, EntityID target_id, int damage,
                 Game::Units::SpawnType attacker_type, bool is_killing_blow)
      : attacker_id(attacker_id), target_id(target_id), damage(damage),
        attacker_type(attacker_type), is_killing_blow(is_killing_blow) {}
  EntityID attacker_id;
  EntityID target_id;
  int damage;
  Game::Units::SpawnType attacker_type;
  bool is_killing_blow;
  [[nodiscard]] auto get_type_name() const -> const char * override {
    return "COMBAT_HIT";
  }
};

} // namespace Engine::Core
