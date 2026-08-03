#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace Engine::Core {
class Entity;
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace Game::Systems {

// What a unit is visibly busy with right now. The HUD, the overhead markers and
// the arena overlay all read this one classification so a soldier can never be
// described as mining in the panel and marching above its head.
enum class ActivityKind : std::uint8_t {
  Idle,
  Move,
  Attack,
  Patrol,
  Guard,
  Hold,
  Construct,
  Repair,
  Dismantle,
  ChopWood,
  MineStone,
  MineIron,
  Deliver,
  Heal,
  Train,
  Blocked,
};

// How far along that work is. `Queued` means the order is accepted but the unit
// is still walking to it, `Interrupted` means work started and stopped, and
// `Unavailable` means the target went away and the order cannot resume.
enum class ActivityState : std::uint8_t {
  Active,
  Queued,
  Unavailable,
  Interrupted,
};

struct UnitActivity {
  ActivityKind kind{ActivityKind::Idle};
  ActivityState state{ActivityState::Active};
  // Orders still waiting behind the current one, e.g. queued wall segments.
  int queued_orders{0};

  [[nodiscard]] auto operator==(const UnitActivity& other) const -> bool {
    return kind == other.kind && state == other.state;
  }
};

// The builder product types the production system understands, mapped onto the
// activity a player sees. Unknown products read as construction.
[[nodiscard]] auto
activity_for_builder_product(std::string_view product_type) -> ActivityKind;

[[nodiscard]] auto
classify_unit_activity(const Engine::Core::Entity& entity) -> UnitActivity;

[[nodiscard]] auto
classify_unit_activity(Engine::Core::World& world,
                       Engine::Core::EntityID entity_id) -> UnitActivity;

// Stable identifiers shared by C++, QML and the arena overlay.
[[nodiscard]] auto activity_kind_id(ActivityKind kind) -> std::string_view;
[[nodiscard]] auto activity_state_id(ActivityState state) -> std::string_view;
[[nodiscard]] auto activity_kind_from_id(std::string_view id) -> ActivityKind;
[[nodiscard]] auto activity_state_from_id(std::string_view id) -> ActivityState;

// True when the activity is worth drawing above a unit. Idle units would just
// litter the battlefield with markers.
[[nodiscard]] auto activity_is_noteworthy(const UnitActivity& activity) -> bool;

} // namespace Game::Systems
