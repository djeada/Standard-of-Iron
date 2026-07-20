#pragma once

#include <cstdint>
#include <vector>

#include "../core/component.h"
#include "../core/entity.h"
#include "formation_system.h"

namespace Game::Systems {
struct TroopProfile;
}

namespace Game::Systems::FormationCombat {

struct SoldierSlot {
  std::uint16_t index{0};
  std::uint16_t row{0};
  std::uint16_t col{0};
  float local_x{0.0F};
  float local_z{0.0F};
  float local_yaw{0.0F};
  float world_x{0.0F};
  float world_z{0.0F};
};

struct FormationLayout {
  int total_count{1};
  int live_count{1};
  int rows{1};
  int cols{1};
  float spacing{0.75F};
  float body_radius{0.5F};
  std::uint32_t seed{0U};

  std::vector<SoldierSlot> all_slots;
  std::vector<SoldierSlot> live_slots;

  std::vector<SoldierSlot> occupied_slots;
};

struct FormationDefinition {
  int total_count{1};
  int max_per_row{1};
  float spacing{0.75F};
  FormationType type{FormationType::Roman};
  FormationUnitCategory category{FormationUnitCategory::Infantry};
};

struct ContactGeometry {
  float center_distance{0.0F};
  float surface_gap{0.0F};
  float contact_center_distance{0.0F};

  float engagement_center_distance{0.0F};
  float contact_tolerance{0.0F};
  bool uses_formation_slots{false};
  bool formation_overlap_required{false};
};

[[nodiscard]] auto
formation_seed(const Engine::Core::Entity& entity) noexcept -> std::uint32_t;

[[nodiscard]] auto
resolve_definition(const Engine::Core::UnitComponent& unit) -> FormationDefinition;

[[nodiscard]] auto
resolve_definition(const Engine::Core::UnitComponent& unit,
                   const Game::Systems::TroopProfile& profile) -> FormationDefinition;

[[nodiscard]] auto
resolve_layout(const Engine::Core::Entity& entity) -> FormationLayout;

[[nodiscard]] auto
contact_geometry(const Engine::Core::Entity& attacker,
                 const Engine::Core::Entity& target) -> ContactGeometry;

[[nodiscard]] auto contact_is_active(const Engine::Core::Entity& attacker,
                                     const Engine::Core::Entity& target,
                                     const ContactGeometry& geometry) -> bool;

[[nodiscard]] auto
engaged_soldiers(const Engine::Core::Entity& attacker,
                 const Engine::Core::Entity& target) -> std::vector<std::uint16_t>;

[[nodiscard]] auto engagement_pairs(const Engine::Core::Entity& attacker,
                                    const Engine::Core::Entity& target)
    -> std::vector<Engine::Core::FormationEngagementPair>;

[[nodiscard]] auto has_formation_slots(const Engine::Core::Entity& entity) -> bool;

} // namespace Game::Systems::FormationCombat
