#pragma once

#include <QVector3D>

#include <cstdint>
#include <optional>

#include "../../core/component.h"
#include "../../core/entity.h"
#include "rpg_targeting.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems::RpgCombat {

struct AimRay {
  QVector3D origin{0.0F, 0.0F, 0.0F};
  QVector3D direction{0.0F, 0.0F, 1.0F};
};

struct AimHit {
  Engine::Core::EntityID entity_id{0};
  std::uint16_t soldier_slot{
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot};
  QVector3D point{0.0F, 0.0F, 0.0F};
  float distance{0.0F};
};

struct BowShot {
  QVector3D start{0.0F, 0.0F, 0.0F};
  QVector3D end{0.0F, 0.0F, 0.0F};

  QVector3D impact{0.0F, 0.0F, 0.0F};
  AimHit hit;
  bool hit_body{false};
};

inline constexpr float k_aim_body_height = 1.80F;

inline constexpr float k_bow_hand_height = 1.34F;
inline constexpr float k_bow_hand_forward = 0.34F;
inline constexpr float k_bow_hand_lateral = 0.26F;

struct FpvAimInput {
  float view_yaw_degrees{0.0F};
  float view_pitch_degrees{0.0F};
  float move_speed{0.0F};
  bool running{false};
  bool primary_held{false};

  QVector3D camera_origin;
  bool camera_origin_valid{false};

  QVector3D camera_forward{0.0F, 0.0F, 1.0F};
  bool camera_forward_valid{false};

  float camera_fov_degrees{68.0F};
};

[[nodiscard]] auto default_weapon_stance(const Engine::Core::Entity& commander)
    -> Engine::Core::FpvWeaponStance;

auto sync_commander_aim(Engine::Core::Entity& commander, const FpvAimInput& input)
    -> Engine::Core::RpgCommanderAimComponent*;

auto toggle_weapon_stance(Engine::Core::Entity& commander) -> bool;

[[nodiscard]] auto
crosshair_ray(const Engine::Core::Entity& commander,
              const Engine::Core::RpgCommanderAimComponent& aim) -> AimRay;

[[nodiscard]] auto
bow_muzzle(const Engine::Core::Entity& commander,
           const Engine::Core::RpgCommanderAimComponent& aim) -> QVector3D;

[[nodiscard]] auto
commander_aim_ray(const Engine::Core::Entity& commander,
                  const Engine::Core::RpgCommanderAimComponent& aim) -> AimRay;

[[nodiscard]] auto raycast_enemy_bodies(Engine::Core::World& world,
                                        Engine::Core::Entity& commander,
                                        const AimRay& ray,
                                        float max_range,
                                        float forgiveness) -> std::optional<AimHit>;

[[nodiscard]] auto
scatter_ray(const AimRay& ray, float spread_degrees, std::uint32_t seed) -> AimRay;

[[nodiscard]] auto resolve_bow_shot(Engine::Core::World& world,
                                    Engine::Core::Entity& commander,
                                    const Engine::Core::RpgCommanderAimComponent& aim,
                                    const AimRay& sight,
                                    float max_range) -> BowShot;

} // namespace Game::Systems::RpgCombat
