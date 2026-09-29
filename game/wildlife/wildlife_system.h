#pragma once

#include <QJsonObject>

#include <cstdint>
#include <vector>

#include "../core/system.h"
#include "nature_ai.h"
#include "wildlife_census.h"
#include "wildlife_config.h"
#include "wildlife_group.h"
#include "wildlife_predation.h"
#include "wildlife_spawner.h"
#include "wildlife_species.h"
#include "wildlife_threats.h"

namespace Engine::Core {
class Entity;
class World;
class WildlifeComponent;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game::Map {
struct MapDefinition;
}

namespace Game::Wildlife {

class WildlifeSystem : public Engine::Core::System {
public:
  static constexpr float k_think_interval = 0.5F;
  static constexpr float k_far_think_multiplier = 4.0F;
  static constexpr float k_threat_refresh_interval = 0.35F;
  static constexpr float k_stall_step_epsilon = 0.004F;

  WildlifeSystem();
  ~WildlifeSystem() override;
  WildlifeSystem(const WildlifeSystem&) = delete;
  WildlifeSystem(WildlifeSystem&&) = delete;
  auto operator=(const WildlifeSystem&) -> WildlifeSystem& = delete;
  auto operator=(WildlifeSystem&&) -> WildlifeSystem& = delete;

  void configure(const Game::Map::MapDefinition& map_definition);
  void configure(const WildlifeSettings& settings, std::uint32_t map_seed);

  void set_cosmetic_focus(float world_x, float world_z) noexcept;
  void clear_cosmetic_focus() noexcept;

  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto serialize_state() const -> QJsonObject;
  void restore_state(const QJsonObject& state);

  [[nodiscard]] auto settings() const noexcept -> const WildlifeSettings& {
    return m_settings;
  }
  [[nodiscard]] auto is_enabled() const noexcept -> bool { return m_enabled; }
  [[nodiscard]] auto groups() const noexcept -> const std::vector<GroupState>& {
    return m_spawner.groups();
  }
  [[nodiscard]] auto stats() const noexcept -> const WildlifeStats& { return m_stats; }
  [[nodiscard]] auto threats() const noexcept -> const ThreatField& {
    return m_census.threats();
  }

private:
  void refresh_census(Engine::Core::World& world, float delta_time);
  void
  update_animal(Engine::Core::World& world, const AnimalRef& animal, float delta_time);
  void think_if_due(Engine::Core::World& world,
                    const AnimalRef& animal,
                    Engine::Core::WildlifeComponent& wildlife,
                    Tier tier,
                    float delta_time);
  void think(Engine::Core::World& world,
             Engine::Core::Entity& entity,
             const GroupRuntime& runtime,
             const SpeciesConfig& config,
             Species species);
  void release_if_stalled(const AnimalRef& animal,
                          Engine::Core::WildlifeComponent& wildlife,
                          float delta_time);

  WildlifeSettings m_settings{};
  WildlifeStats m_stats{};
  WildlifeCensus m_census;
  WildlifeSpawner m_spawner;
  WildlifePredation m_predation{m_stats};
  NatureBrain m_sheep_brain{make_sheep_brain()};
  NatureBrain m_wolf_brain{make_wolf_brain()};
  float m_threat_refresh{0.0F};
  bool m_enabled{false};
  bool m_spawn_pending{false};
  bool m_restored{false};
};

} // namespace Game::Wildlife
