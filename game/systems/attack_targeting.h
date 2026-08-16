#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "../core/entity.h"
#include "../map/visibility_service.h"

namespace Engine::Core {
class World;
class Entity;
class UnitComponent;
} // namespace Engine::Core

namespace Game::Systems {

enum class AttackTargetVerdict : std::uint8_t {
  NoTarget,
  Valid,
  Ally,
  Neutral,
  NoAttackers,
};

struct AttackTargetMarker {
  Engine::Core::EntityID entity_id{0};
  float world_x{0.0F};
  float world_y{0.0F};
  float world_z{0.0F};
  float radius{0.5F};
  bool is_building{false};
  bool hovered{false};
  bool attackable{true};
};

struct AttackTargetingRequest {
  Engine::Core::World* world{nullptr};
  int local_owner_id{0};

  bool has_attackers{false};
  Engine::Core::EntityID hovered_entity_id{0};

  float anchor_x{0.0F};
  float anchor_z{0.0F};
  float max_distance{0.0F};
  std::size_t max_markers{0};
  const Game::Map::VisibilityService::Snapshot* visibility{nullptr};
};

struct AttackTargetingHighlights {
  std::vector<AttackTargetMarker> markers;
  Engine::Core::EntityID hovered_entity_id{0};
  AttackTargetVerdict hovered_verdict{AttackTargetVerdict::NoTarget};
  bool hovered_marker_included{false};
};

inline constexpr float k_attack_highlight_max_distance = 60.0F;
inline constexpr std::size_t k_attack_highlight_max_markers = 64;

[[nodiscard]] auto attack_marker_radius(Engine::Core::Entity& entity,
                                        const Engine::Core::UnitComponent& unit,
                                        bool is_building) -> float;

[[nodiscard]] auto
classify_attack_target(Engine::Core::World* world,
                       int local_owner_id,
                       bool has_attackers,
                       Engine::Core::EntityID target_id) -> AttackTargetVerdict;

[[nodiscard]] auto collect_attack_target_highlights(
    const AttackTargetingRequest& request) -> AttackTargetingHighlights;

[[nodiscard]] auto
attack_target_verdict_key(AttackTargetVerdict verdict) -> std::string_view;

} // namespace Game::Systems
