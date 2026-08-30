#pragma once

#include <QString>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../systems/resource_types.h"
#include "../units/spawn_type.h"
#include "entity.h"
#include "local_audience.h"

namespace Engine::Core {

class Event {
public:
  virtual ~Event() = default;
  [[nodiscard]] virtual auto get_type_name() const -> const char* { return "Event"; }
};

template <typename T>
using EventHandler = std::function<void(const T&)>;

using SubscriptionHandle = std::size_t;

struct EventStats {
  size_t publish_count = 0;
  size_t subscriber_count = 0;
};

class EventManager {
public:
  static auto instance() -> EventManager& {

    static auto* inst = new EventManager();
    return *inst;
  }

  template <typename T>
  auto subscribe(EventHandler<T> handler) -> SubscriptionHandle {
    static_assert(std::is_base_of_v<Event, T>, "T must inherit from Event");
    std::lock_guard<std::mutex> const lock(m_mutex);

    SubscriptionHandle const handle = m_next_handle++;
    auto wrapper = [handler](const void* event) {
      handler(*static_cast<const T*>(event));
    };
    HandlerEntry const entry{handle, wrapper};
    m_handlers[std::type_index(typeid(T))].push_back(entry);

    m_stats[std::type_index(typeid(T))].subscriber_count++;

    return handle;
  }

  template <typename T>
  void unsubscribe(SubscriptionHandle handle) {
    static_assert(std::is_base_of_v<Event, T>, "T must inherit from Event");
    std::lock_guard<std::mutex> const lock(m_mutex);

    auto it = m_handlers.find(std::type_index(typeid(T)));
    if (it != m_handlers.end()) {
      auto& handlers = it->second;
      auto size_before = handlers.size();
      handlers.erase(std::remove_if(handlers.begin(),
                                    handlers.end(),
                                    [handle](const HandlerEntry& e) {
                                      return e.handle == handle;
                                    }),
                     handlers.end());

      if (handlers.size() < size_before) {
        m_stats[std::type_index(typeid(T))].subscriber_count--;
      }
    }
  }

  template <typename T>
  void publish(const T& event) {
    static_assert(std::is_base_of_v<Event, T>, "T must inherit from Event");
    std::vector<HandlerEntry> handlers_copy;

    {
      std::lock_guard<std::mutex> const lock(m_mutex);
      auto it = m_handlers.find(std::type_index(typeid(T)));
      if (it != m_handlers.end()) {
        handlers_copy = it->second;
        m_stats[std::type_index(typeid(T))].publish_count++;
      }
    }

    for (const auto& entry : handlers_copy) {
      entry.handler(&event);
    }
  }

  auto get_stats(const std::type_index& event_type) const -> EventStats {
    std::lock_guard<std::mutex> const lock(m_mutex);
    auto it = m_stats.find(event_type);
    if (it != m_stats.end()) {
      return it->second;
    }
    return EventStats{};
  }

  auto get_subscriber_count(const std::type_index& event_type) const -> size_t {
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
    std::function<void(const void*)> handler;
  };

  mutable std::mutex m_mutex;
  std::unordered_map<std::type_index, std::vector<HandlerEntry>> m_handlers;
  std::unordered_map<std::type_index, EventStats> m_stats;
  SubscriptionHandle m_next_handle = 1;
};

template <typename T>
class ScopedEventSubscription {
public:
  ScopedEventSubscription()
      : m_handle(0) {}

  ScopedEventSubscription(EventHandler<T> handler)
      : m_handle(EventManager::instance().subscribe<T>(handler)) {}

  ~ScopedEventSubscription() { unsubscribe(); }

  ScopedEventSubscription(const ScopedEventSubscription&) = delete;
  auto operator=(const ScopedEventSubscription&) -> ScopedEventSubscription& = delete;

  ScopedEventSubscription(ScopedEventSubscription&& other) noexcept
      : m_handle(other.m_handle) {
    other.m_handle = 0;
  }

  auto operator=(ScopedEventSubscription&& other) noexcept -> ScopedEventSubscription& {
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
  UnitSelectedEvent(EntityID unit_id)
      : unit_id(unit_id) {}
  EntityID unit_id;
  [[nodiscard]] auto get_type_name() const -> const char* override {
    return "UNIT_SELECTED";
  }
};

class UnitDiedEvent : public Event {
public:
  UnitDiedEvent(EntityID unit_id,
                int owner_id,
                Game::Units::SpawnType spawn_type,
                EntityID killer_id = 0,
                int killer_owner_id = 0)
      : unit_id(unit_id)
      , owner_id(owner_id)
      , spawn_type(spawn_type)
      , killer_id(killer_id)
      , killer_owner_id(killer_owner_id) {}
  EntityID unit_id;
  int owner_id;
  Game::Units::SpawnType spawn_type;
  EntityID killer_id;
  int killer_owner_id;
};

class UnitSpawnedEvent : public Event {
public:
  UnitSpawnedEvent(EntityID unit_id,
                   int owner_id,
                   Game::Units::SpawnType spawn_type,
                   bool is_initial_spawn = true)
      : unit_id(unit_id)
      , owner_id(owner_id)
      , spawn_type(spawn_type)
      , is_initial_spawn(is_initial_spawn) {}
  EntityID unit_id;
  int owner_id;
  Game::Units::SpawnType spawn_type;
  bool is_initial_spawn;
};

class BuildingAttackedEvent : public Event {
public:
  BuildingAttackedEvent(EntityID building_id,
                        int owner_id,
                        Game::Units::SpawnType building_type,
                        EntityID attacker_id = 0,
                        int attacker_owner_id = 0,
                        int damage = 0)
      : building_id(building_id)
      , owner_id(owner_id)
      , building_type(building_type)
      , attacker_id(attacker_id)
      , attacker_owner_id(attacker_owner_id)
      , damage(damage) {}
  EntityID building_id;
  int owner_id;
  Game::Units::SpawnType building_type;
  EntityID attacker_id;
  int attacker_owner_id;
  int damage;
};

class BarrackCapturedEvent : public Event {
public:
  BarrackCapturedEvent(EntityID barrack_id, int previous_owner_id, int new_owner_id)
      : barrack_id(barrack_id)
      , previous_owner_id(previous_owner_id)
      , new_owner_id(new_owner_id) {}
  EntityID barrack_id;
  int previous_owner_id;
  int new_owner_id;
};

enum class AmbientState {
  PEACEFUL,
  TENSE,
  COMBAT,
  VICTORY,
  DEFEAT
};

class AmbientStateChangedEvent : public Event {
public:
  AmbientStateChangedEvent(AmbientState new_state, AmbientState previous_state)
      : new_state(new_state)
      , previous_state(previous_state) {}
  AmbientState new_state;
  AmbientState previous_state;
  [[nodiscard]] auto get_type_name() const -> const char* override {
    return "AMBIENT_STATE_CHANGED";
  }
};

class AudioTriggerEvent : public Event {
public:
  AudioTriggerEvent(std::string sound_id,
                    float volume = 1.0F,
                    bool loop = false,
                    int priority = 0,
                    int owner_id = k_owner_everyone)
      : sound_id(std::move(sound_id))
      , volume(volume)
      , loop(loop)
      , priority(priority)
      , owner_id(owner_id) {}
  std::string sound_id;
  float volume;
  bool loop;
  int priority;

  int owner_id;
};

class AudioCueEvent : public Event {
public:
  explicit AudioCueEvent(std::string cue_id,
                         float volume_scale = 1.0F,
                         int owner_id = k_owner_everyone)
      : cue_id(std::move(cue_id))
      , volume_scale(volume_scale)
      , owner_id(owner_id) {}

  static auto for_owner(int owner_id,
                        std::string cue_id,
                        float volume_scale = 1.0F) -> AudioCueEvent {
    return AudioCueEvent(std::move(cue_id), volume_scale, owner_id);
  }

  std::string cue_id;
  float volume_scale;

  int owner_id;
  [[nodiscard]] auto get_type_name() const -> const char* override {
    return "AUDIO_CUE";
  }
};

class MissionAnnouncementEvent : public Event {
public:
  explicit MissionAnnouncementEvent(QString text, int owner_id = k_owner_everyone)
      : text(std::move(text))
      , owner_id(owner_id) {}

  static auto for_owner(int owner_id, QString text) -> MissionAnnouncementEvent {
    return MissionAnnouncementEvent(std::move(text), owner_id);
  }

  QString text;

  int owner_id;
  [[nodiscard]] auto get_type_name() const -> const char* override {
    return "MISSION_ANNOUNCEMENT";
  }
};

class MusicTriggerEvent : public Event {
public:
  MusicTriggerEvent(std::string music_id, float volume = 1.0F, bool crossfade = true)
      : music_id(std::move(music_id))
      , volume(volume)
      , crossfade(crossfade) {}
  std::string music_id;
  float volume;
  bool crossfade;
};

class MusicStopEvent : public Event {
public:
  MusicStopEvent() = default;
};

class CombatHitEvent : public Event {
public:
  CombatHitEvent(EntityID attacker_id,
                 EntityID target_id,
                 int damage,
                 Game::Units::SpawnType attacker_type,
                 bool is_killing_blow,
                 int attacker_owner_id = 0,
                 int target_owner_id = 0)
      : attacker_id(attacker_id)
      , target_id(target_id)
      , damage(damage)
      , attacker_type(attacker_type)
      , is_killing_blow(is_killing_blow)
      , attacker_owner_id(attacker_owner_id)
      , target_owner_id(target_owner_id) {}
  EntityID attacker_id;
  EntityID target_id;
  int damage;
  Game::Units::SpawnType attacker_type;
  bool is_killing_blow;
  int attacker_owner_id;
  int target_owner_id;
  [[nodiscard]] auto get_type_name() const -> const char* override {
    return "COMBAT_HIT";
  }
};

enum class EconomyFeedbackKind : std::uint8_t {
  Resource = 0,
  Reserve,
};

class EconomyFeedbackEvent : public Event {
public:
  static auto make_resource(int owner_id,
                            EntityID anchor_id,
                            Game::Systems::ResourceType type,
                            int amount) -> EconomyFeedbackEvent {
    EconomyFeedbackEvent event;
    event.owner_id = owner_id;
    event.anchor_id = anchor_id;
    event.kind = EconomyFeedbackKind::Resource;
    event.resource = static_cast<int>(Game::Systems::resource_type_index(type));
    event.amount = amount;
    return event;
  }

  static auto make_trade(int owner_id,
                         EntityID anchor_id,
                         Game::Systems::ResourceType spent_type,
                         int spent_amount,
                         Game::Systems::ResourceType gained_type,
                         int gained_amount) -> EconomyFeedbackEvent {
    EconomyFeedbackEvent event =
        make_resource(owner_id, anchor_id, spent_type, -std::abs(spent_amount));
    event.paired_resource =
        static_cast<int>(Game::Systems::resource_type_index(gained_type));
    event.paired_amount = std::abs(gained_amount);
    return event;
  }

  static auto
  make_reserve(int owner_id, EntityID anchor_id, int amount) -> EconomyFeedbackEvent {
    EconomyFeedbackEvent event;
    event.owner_id = owner_id;
    event.anchor_id = anchor_id;
    event.kind = EconomyFeedbackKind::Reserve;
    event.amount = amount;
    return event;
  }

  auto at(float world_x, float world_y, float world_z) -> EconomyFeedbackEvent& {
    x = world_x;
    y = world_y;
    z = world_z;
    has_position = true;
    return *this;
  }

  int owner_id = 0;
  EntityID anchor_id = 0;
  EconomyFeedbackKind kind = EconomyFeedbackKind::Resource;
  int resource = -1;
  int amount = 0;
  int paired_resource = -1;
  int paired_amount = 0;
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
  bool has_position = false;

  [[nodiscard]] auto get_type_name() const -> const char* override {
    return "ECONOMY_FEEDBACK";
  }
};

} // namespace Engine::Core
