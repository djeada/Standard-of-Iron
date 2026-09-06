#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "../core/component.h"
#include "../core/entity.h"
#include "../formation/unit_layout_resolver.h"

namespace Game::Systems {
struct TroopProfile;
}

namespace Game::Systems::FormationCombat {

inline constexpr float k_body_core_radius_floor = 0.5F;

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

enum class SoldierAnchorSource : std::uint8_t {
  BaseLayout = 0,
  TraversalLayout,
  PresentationFacts,
};

struct SoldierSpatialAnchor {
  std::uint16_t slot_index{0U};
  std::uint16_t row{0U};
  std::uint16_t col{0U};
  float local_x{0.0F};
  float local_z{0.0F};
  float local_yaw{0.0F};
  float world_x{0.0F};
  float world_z{0.0F};
  SoldierAnchorSource source{SoldierAnchorSource::BaseLayout};
};

struct FormationDefinition {
  int total_count{1};
  int max_per_row{1};
  float spacing{0.75F};
  Game::Formation::UnitLayoutId layout{Game::Formation::k_invalid_layout};
  Game::Formation::UnitLayoutState layout_state{
      Game::Formation::UnitLayoutState::Normal};
  Game::Formation::FormationDoctrineId doctrine{"rome"};
};

struct ContactGeometry {
  float center_distance{0.0F};
  float surface_gap{0.0F};
  float contact_center_distance{0.0F};

  float engagement_center_distance{0.0F};

  float body_contact_center_distance{0.0F};
  float contact_tolerance{0.0F};
  bool uses_formation_slots{false};
  bool formation_overlap_required{false};
};

struct FormationContactContext {
  FormationLayout attacker_layout;
  FormationLayout target_layout;
  ContactGeometry geometry;
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

[[nodiscard]] auto soldier_spatial_anchors(const Engine::Core::Entity& entity)
    -> std::vector<SoldierSpatialAnchor>;

[[nodiscard]] auto soldier_spatial_anchors(const Engine::Core::Entity& entity,
                                           const FormationLayout& base_layout)
    -> std::vector<SoldierSpatialAnchor>;

void soldier_spatial_anchors_into(const Engine::Core::Entity& entity,
                                  const FormationLayout& base_layout,
                                  std::vector<SoldierSpatialAnchor>& result);

[[nodiscard]] auto living_slot_indices(const Engine::Core::Entity& entity,
                                       int total_count) -> std::vector<std::uint16_t>;

[[nodiscard]] auto living_slot_count(const Engine::Core::Entity& entity,
                                     int total_count) -> int;

[[nodiscard]] auto formation_turn_radius(const Engine::Core::Entity& entity) -> float;

[[nodiscard]] auto
formation_navigation_clearance(const Engine::Core::Entity& entity) -> float;

[[nodiscard]] auto
formation_lateral_half_extent(const FormationLayout& layout) -> float;

[[nodiscard]] auto narrow_file_depth(const FormationLayout& layout,
                                     std::uint32_t files,
                                     float rank_spacing) -> float;

void narrow_file_slots_into(const FormationLayout& layout,
                            std::uint32_t files,
                            float file_spacing,
                            float rank_spacing,
                            std::vector<SoldierSlot>& result);

[[nodiscard]] auto
resolve_contact_context(const Engine::Core::Entity& attacker,
                        const Engine::Core::Entity& target) -> FormationContactContext;

[[nodiscard]] auto
contact_geometry(const Engine::Core::Entity& attacker,
                 const Engine::Core::Entity& target) -> ContactGeometry;

[[nodiscard]] auto
single_combat_strike_distance(const Engine::Core::Entity& attacker,
                              const Engine::Core::Entity& target,
                              const ContactGeometry& geometry) -> float;

[[nodiscard]] auto contact_is_active(const Engine::Core::Entity& attacker,
                                     const Engine::Core::Entity& target,
                                     const ContactGeometry& geometry) -> bool;

[[nodiscard]] auto
engaged_soldiers(const Engine::Core::Entity& attacker,
                 const Engine::Core::Entity& target) -> std::vector<std::uint16_t>;

[[nodiscard]] auto engagement_pairs(const Engine::Core::Entity& attacker,
                                    const Engine::Core::Entity& target)
    -> std::vector<Engine::Core::FormationEngagementPair>;

[[nodiscard]] auto engagement_pairs(const Engine::Core::Entity& attacker,
                                    const Engine::Core::Entity& target,
                                    const FormationLayout& attacker_layout,
                                    const FormationLayout& target_layout)
    -> std::vector<Engine::Core::FormationEngagementPair>;

[[nodiscard]] auto select_damage_engagement_pair(
    const Engine::Core::Entity& attacker,
    Engine::Core::EntityID opponent_id,
    const std::vector<Engine::Core::FormationEngagementPair>& pairs)
    -> std::optional<Engine::Core::FormationEngagementPair>;

[[nodiscard]] auto has_formation_slots(const Engine::Core::Entity& entity) -> bool;

[[nodiscard]] auto max_contact_extent() -> float;

void invalidate_layout_cache();

[[nodiscard]] auto formation_cache_generation() -> std::uint64_t;

struct ContactStats {
  std::uint64_t resolutions{0};
  std::uint64_t cache_hits{0};
  std::uint64_t cache_misses{0};
  std::uint64_t slot_pairs_possible{0};
  std::uint64_t slot_pairs_examined{0};
};

[[nodiscard]] auto contact_stats() -> ContactStats;

void reset_contact_stats();

} // namespace Game::Systems::FormationCombat
