#pragma once

#include <QJsonObject>

#include <cstdint>
#include <memory>
#include <vector>

#include "../core/system.h"
#include "nature_ai.h"
#include "wildlife_config.h"
#include "wildlife_species.h"
#include "wildlife_threats.h"

namespace Engine::Core {
class Entity;
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game::Map {
struct MapDefinition;
}

namespace Game::Units {
class UnitFactoryRegistry;
}

namespace Game::Wildlife {

struct GroupState {
  std::uint16_t id{0U};
  Species species{Species::Sheep};
  float home_x{0.0F};
  float home_z{0.0F};
  float roam_radius{14.0F};
  int desired_size{0};
  float respawn_timer{0.0F};
  std::uint32_t rng_state{1U};
};

struct WildlifeStats {
  std::uint64_t near_thinks{0U};
  std::uint64_t far_thinks{0U};
  std::uint64_t dormant_skips{0U};
  std::uint64_t flee_events{0U};
  std::uint64_t hunt_events{0U};
  std::uint64_t bites{0U};
  std::uint64_t respawns{0U};

  void reset() noexcept { *this = WildlifeStats{}; }
};

class WildlifeSystem : public Engine::Core::System {
public:
  static constexpr float k_think_interval = 0.5F;
  static constexpr float k_far_think_multiplier = 4.0F;
  static constexpr float k_threat_refresh_interval = 0.35F;

  WildlifeSystem();
  ~WildlifeSystem() override;
  WildlifeSystem(const WildlifeSystem&) = delete;
  WildlifeSystem(WildlifeSystem&&) = delete;
  auto operator=(const WildlifeSystem&) -> WildlifeSystem& = delete;
  auto operator=(WildlifeSystem&&) -> WildlifeSystem& = delete;

  void configure(const Game::Map::MapDefinition& map_definition);
  void configure(const WildlifeSettings& settings, std::uint32_t map_seed);

  void set_focus(float world_x, float world_z) noexcept;
  void clear_focus() noexcept;

  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto serialize_state() const -> QJsonObject;
  void restore_state(const QJsonObject& state);

  [[nodiscard]] auto settings() const noexcept -> const WildlifeSettings& {
    return m_settings;
  }
  [[nodiscard]] auto is_enabled() const noexcept -> bool { return m_enabled; }
  [[nodiscard]] auto groups() const noexcept -> const std::vector<GroupState>& {
    return m_groups;
  }
  [[nodiscard]] auto stats() const noexcept -> const WildlifeStats& { return m_stats; }
  [[nodiscard]] auto threats() const noexcept -> const ThreatField& {
    return m_threats;
  }

private:
  enum class Tier : std::uint8_t {
    Near = 0,
    Far = 1,
    Dormant = 2,
  };

  struct GroupRuntime {
    float center_x{0.0F};
    float center_z{0.0F};
    float alarm{0.0F};
    int alive{0};
  };

  class NatureActionAdapter;
  friend class NatureActionAdapter;

  struct AnimalRef {
    Engine::Core::Entity* entity{nullptr};
    Engine::Core::EntityID id{0};
    float x{0.0F};
    float z{0.0F};
    std::uint16_t group{0U};
    Species species{Species::Sheep};
  };

  struct QuarryRef {
    Engine::Core::EntityID id{0};
    float x{0.0F};
    float z{0.0F};
    bool civilian{false};
  };

  void ensure_factory_registry();
  void spawn_initial_population(Engine::Core::World& world);
  void release_due_packs(Engine::Core::World& world, float delta_time);
  void plan_groups();
  auto spawn_member(Engine::Core::World& world,
                    GroupState& group,
                    const SpeciesConfig& config) -> Engine::Core::EntityID;

  void rebuild_threats(Engine::Core::World& world);
  void collect_animals(Engine::Core::World& world);
  void update_respawns(Engine::Core::World& world, float delta_time);

  void think(Engine::Core::World& world,
             Engine::Core::Entity& entity,
             const GroupRuntime& runtime,
             const SpeciesConfig& config,
             Species species);

  auto begin_bite(Engine::Core::Entity& entity,
                  Engine::Core::WildlifeComponent& wildlife,
                  const PreyRef& prey) -> bool;
  void try_contact_bite(Engine::Core::World& world,
                        const AnimalRef& animal,
                        Engine::Core::WildlifeComponent& wildlife);

  void alert_group(std::uint16_t group_id, float duration);
  void
  rally_pack(std::uint16_t group_id, Engine::Core::EntityID foe_id, float duration);
  void issue_move(Engine::Core::World& world,
                  Engine::Core::EntityID entity_id,
                  float world_x,
                  float world_z);
  void set_travel_speed(Engine::Core::Entity& entity,
                        const SpeciesConfig& config,
                        bool urgent);

  [[nodiscard]] auto tier_for(float world_x, float world_z) const noexcept -> Tier;
  [[nodiscard]] auto find_group(std::uint16_t group_id) -> GroupState*;
  [[nodiscard]] auto pick_open_point(std::uint32_t& rng,
                                     float origin_x,
                                     float origin_z,
                                     float min_radius,
                                     float max_radius,
                                     float& out_x,
                                     float& out_z) const -> bool;
  [[nodiscard]] auto
  nearest_prey(float world_x,
               float world_z,
               float radius,
               Engine::Core::EntityID hunter_id) const -> const AnimalRef*;
  [[nodiscard]] auto
  nearest_quarry(float world_x,
                 float world_z,
                 float radius,
                 Engine::Core::EntityID hunter_id) const -> const QuarryRef*;
  [[nodiscard]] auto attackers_on(Engine::Core::EntityID prey_id,
                                  Engine::Core::EntityID exclude_id) const -> int;
  [[nodiscard]] auto pack_slot_for(Engine::Core::EntityID prey_id,
                                   Engine::Core::EntityID hunter_id) const -> PackSlot;

  WildlifeSettings m_settings{};
  std::vector<GroupState> m_groups;
  std::vector<GroupRuntime> m_group_runtime;
  std::vector<AnimalRef> m_animals;
  std::vector<QuarryRef> m_quarry;
  ThreatField m_threats;
  WildlifeStats m_stats{};
  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory_registry;
  NatureBrain m_sheep_brain{make_sheep_brain()};
  NatureBrain m_wolf_brain{make_wolf_brain()};
  std::uint32_t m_seed{1U};
  std::uint16_t m_next_group_id{0U};
  float m_threat_refresh{0.0F};
  float m_focus_x{0.0F};
  float m_focus_z{0.0F};
  bool m_has_focus{false};
  bool m_enabled{false};
  bool m_spawn_pending{false};
  bool m_restored{false};
  float m_elapsed{0.0F};
  std::vector<bool> m_released_waves;
};

} // namespace Game::Wildlife
