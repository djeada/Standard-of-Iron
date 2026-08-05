#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "../core/entity.h"
#include "../units/spawn_type.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

enum class RangeWeaponClass : std::uint8_t {
  None,
  Bow,
  Siege,
  Arcane,
};

enum class RangeVerdict : std::uint8_t {
  None,
  InRange,
  TooClose,
  OutOfRange,
  Blocked,
};

struct AttackRangeProfile {
  bool ranged{false};
  float max_range{0.0F};
  float current_range{0.0F};
  float min_range{0.0F};
  RangeWeaponClass weapon_class{RangeWeaponClass::None};
};

struct AttackRangeRing {
  Engine::Core::EntityID entity_id{0};
  float world_x{0.0F};
  float world_y{0.0F};
  float world_z{0.0F};
  float max_radius{0.0F};
  float min_radius{0.0F};
  RangeWeaponClass weapon_class{RangeWeaponClass::None};
  bool focused{false};
};

struct AttackRangeRingRequest {
  Engine::Core::World* world{nullptr};
  int local_owner_id{0};
  std::span<const Engine::Core::EntityID> selection;
  Engine::Core::EntityID focus_entity_id{0};
  std::size_t max_rings{0};
};

inline constexpr std::size_t k_attack_range_max_rings = 12;

inline constexpr float k_attack_range_duplicate_radius_tolerance = 0.05F;

inline constexpr float k_attack_range_duplicate_center_factor = 0.35F;

[[nodiscard]] auto
hold_mode_range_multiplier(const Engine::Core::Entity& entity,
                           Game::Units::SpawnType spawn_type) -> float;

[[nodiscard]] auto
range_weapon_class(Game::Units::SpawnType spawn_type) -> RangeWeaponClass;

[[nodiscard]] auto
range_weapon_class_key(RangeWeaponClass weapon_class) -> std::string_view;

[[nodiscard]] auto range_verdict_key(RangeVerdict verdict) -> std::string_view;

[[nodiscard]] auto
resolve_attack_range(const Engine::Core::Entity& entity) -> AttackRangeProfile;

[[nodiscard]] auto collect_attack_range_rings(const AttackRangeRingRequest& request)
    -> std::vector<AttackRangeRing>;

[[nodiscard]] auto
classify_range_to_target(Engine::Core::World* world,
                         std::span<const Engine::Core::EntityID> attackers,
                         Engine::Core::EntityID target_id) -> RangeVerdict;

} // namespace Game::Systems
