#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "../core/entity.h"
#include "wildlife_census.h"
#include "wildlife_config.h"
#include "wildlife_group.h"
#include "wildlife_persistence.h"

namespace Engine::Core {
class World;
}

namespace Game::Units {
class UnitFactoryRegistry;
}

namespace Game::Wildlife {

[[nodiscard]] auto pick_open_point(std::uint32_t& rng,
                                   float origin_x,
                                   float origin_z,
                                   float min_radius,
                                   float max_radius,
                                   float& out_x,
                                   float& out_z) -> bool;

class WildlifeSpawner {
public:
  ~WildlifeSpawner();

  void configure(const WildlifeSettings& settings, std::uint32_t seed, bool enabled);

  void spawn_initial_population(Engine::Core::World& world,
                                const WildlifeSettings& settings);
  void release_due_packs(Engine::Core::World& world,
                         const WildlifeSettings& settings,
                         float delta_time);
  void update_respawns(Engine::Core::World& world,
                       const WildlifeSettings& settings,
                       const WildlifeCensus& census,
                       WildlifeStats& stats,
                       float delta_time);

  [[nodiscard]] auto groups() const noexcept -> const std::vector<GroupState>& {
    return m_groups;
  }
  [[nodiscard]] auto seed() const noexcept -> std::uint32_t { return m_seed; }

  void store_into(WildlifeSaveData& data) const;
  void restore_from(const WildlifeSaveData& data);

private:
  void ensure_factory_registry();
  void plan_groups(const WildlifeSettings& settings);
  auto spawn_member(Engine::Core::World& world,
                    GroupState& group,
                    const SpeciesConfig& config) -> Engine::Core::EntityID;
  void release_pack(Engine::Core::World& world,
                    const SpeciesConfig& config,
                    std::size_t wave_index);

  std::vector<GroupState> m_groups;
  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory_registry;
  std::uint32_t m_seed{1U};
  std::uint16_t m_next_group_id{0U};
  double m_elapsed{0.0};
  std::vector<bool> m_released_waves;
};

} // namespace Game::Wildlife
