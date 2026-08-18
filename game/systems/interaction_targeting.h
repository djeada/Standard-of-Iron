#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "../core/entity.h"
#include "../map/map_definition.h"
#include "../map/visibility_service.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

enum class InteractionAction : std::uint8_t {
  None,
  Gather,
  Deliver,
  Repair,
};

struct InteractionTargetMarker {
  Engine::Core::EntityID entity_id{0};
  std::uint64_t world_prop_id{0};
  InteractionAction action{InteractionAction::None};
  Game::Map::WorldProp::Type prop_type{Game::Map::WorldProp::Type::PineTree};
  float world_x{0.0F};
  float world_y{0.0F};
  float world_z{0.0F};
  float radius{0.6F};
  bool hovered{false};
};

struct InteractionTargetingRequest {
  Engine::Core::World* world{nullptr};
  int local_owner_id{0};

  bool has_builders{false};
  bool has_civilians{false};

  Engine::Core::EntityID hovered_entity_id{0};

  bool has_hovered_ground{false};
  float hovered_ground_x{0.0F};
  float hovered_ground_z{0.0F};

  float anchor_x{0.0F};
  float anchor_z{0.0F};
  float max_distance{0.0F};
  std::size_t max_markers{0};
  const Game::Map::VisibilityService::Snapshot* visibility{nullptr};
};

struct InteractionTargetingHighlights {
  std::vector<InteractionTargetMarker> markers;
  Engine::Core::EntityID hovered_entity_id{0};
  InteractionAction hovered_action{InteractionAction::None};
};

inline constexpr float k_interaction_highlight_max_distance = 42.0F;
inline constexpr std::size_t k_interaction_highlight_max_markers = 48;

[[nodiscard]] auto collect_interaction_target_highlights(
    const InteractionTargetingRequest& request) -> InteractionTargetingHighlights;

[[nodiscard]] auto interaction_action_key(InteractionAction action) -> std::string_view;

} // namespace Game::Systems
