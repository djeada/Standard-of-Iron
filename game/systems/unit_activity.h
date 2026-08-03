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

enum class ActivityState : std::uint8_t {
  Active,
  Queued,
  Unavailable,
  Interrupted,
};

struct UnitActivity {
  ActivityKind kind{ActivityKind::Idle};
  ActivityState state{ActivityState::Active};

  int queued_orders{0};

  [[nodiscard]] auto operator==(const UnitActivity& other) const -> bool {
    return kind == other.kind && state == other.state;
  }
};

[[nodiscard]] auto
activity_for_builder_product(std::string_view product_type) -> ActivityKind;

[[nodiscard]] auto
classify_unit_activity(const Engine::Core::Entity& entity) -> UnitActivity;

[[nodiscard]] auto
classify_unit_activity(Engine::Core::World& world,
                       Engine::Core::EntityID entity_id) -> UnitActivity;

[[nodiscard]] auto activity_kind_id(ActivityKind kind) -> std::string_view;
[[nodiscard]] auto activity_state_id(ActivityState state) -> std::string_view;
[[nodiscard]] auto activity_kind_from_id(std::string_view id) -> ActivityKind;
[[nodiscard]] auto activity_state_from_id(std::string_view id) -> ActivityState;

[[nodiscard]] auto activity_is_noteworthy(const UnitActivity& activity) -> bool;

} // namespace Game::Systems
