#include "death_variant.h"

#include <cmath>
#include <numbers>

#include "../../core/component.h"
#include "animation/death_pose_manifest.h"
#include "damage_application.h"

namespace Game::Systems::Combat {

namespace {

auto infantry_death_variant(Engine::Core::Entity* target,
                            Engine::Core::Entity* attacker,
                            std::uint16_t slot) -> std::uint8_t {
  using Animation::HumanoidDeathCollapse;

  auto const variant_for = [](HumanoidDeathCollapse collapse) -> std::uint8_t {
    for (std::uint8_t v = 0U; v < Animation::k_humanoid_infantry_death_variant_count;
         ++v) {
      if (Animation::humanoid_infantry_death_collapse(v) == collapse) {
        return v;
      }
    }
    return 0U;
  };

  auto const jitter = static_cast<std::uint32_t>(
      (target != nullptr ? target->get_id() * 2654435761U : 0U) + (slot * 40503U));
  bool const flanked_by_jitter = slot != 0U && (jitter >> 13U) % 3U == 0U;

  auto const* target_transform =
      target != nullptr ? target->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  auto const* attacker_transform =
      attacker != nullptr ? attacker->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
  if (target_transform == nullptr || attacker_transform == nullptr) {
    return variant_for(flanked_by_jitter ? HumanoidDeathCollapse::SideCrumple
                                         : HumanoidDeathCollapse::BackSprawl);
  }

  float const to_attacker_x =
      attacker_transform->position.x - target_transform->position.x;
  float const to_attacker_z =
      attacker_transform->position.z - target_transform->position.z;
  float const length_sq =
      (to_attacker_x * to_attacker_x) + (to_attacker_z * to_attacker_z);
  if (length_sq < 1.0e-4F) {
    return variant_for(HumanoidDeathCollapse::BackSprawl);
  }

  float const yaw = target_transform->rotation.y * std::numbers::pi_v<float> / 180.0F;
  float const facing_dot =
      ((std::sin(yaw) * to_attacker_x) + (std::cos(yaw) * to_attacker_z)) /
      std::sqrt(length_sq);

  if (facing_dot > 0.42F) {
    return variant_for(flanked_by_jitter ? HumanoidDeathCollapse::SideCrumple
                                         : HumanoidDeathCollapse::BackSprawl);
  }
  if (facing_dot < -0.42F) {
    return variant_for(flanked_by_jitter ? HumanoidDeathCollapse::SideCrumple
                                         : HumanoidDeathCollapse::FacePlant);
  }
  return variant_for(HumanoidDeathCollapse::SideCrumple);
}

auto structure_fall_heading(Engine::Core::Entity* target,
                            Engine::Core::Entity* attacker) -> std::uint8_t {
  auto const* target_tf =
      target != nullptr ? target->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  auto const* attacker_tf =
      attacker != nullptr ? attacker->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
  if (target_tf == nullptr || attacker_tf == nullptr) {
    return static_cast<std::uint8_t>(
        (target != nullptr ? target->get_id() : 0U) * 2654435761U >> 24U);
  }
  float const dx = target_tf->position.x - attacker_tf->position.x;
  float const dz = target_tf->position.z - attacker_tf->position.z;
  if (dx * dx + dz * dz < 1.0e-6F) {
    return static_cast<std::uint8_t>(target->get_id() * 2654435761U >> 24U);
  }
  float const turns = std::atan2(dx, dz) / (2.0F * std::numbers::pi_v<float>);
  return static_cast<std::uint8_t>(
      static_cast<int>(std::lround((turns + 1.0F) * 256.0F)) & 0xFF);
}

} // namespace

auto resolve_death_variant(Engine::Core::Entity* target,
                           Engine::Core::Entity* attacker,
                           Engine::Core::DeathSequenceProfile profile,
                           std::uint16_t slot) -> std::uint8_t {
  switch (profile) {
  case Engine::Core::DeathSequenceProfile::Infantry:
    return infantry_death_variant(target, attacker, slot);
  case Engine::Core::DeathSequenceProfile::Structure:
    return structure_fall_heading(target, attacker);
  case Engine::Core::DeathSequenceProfile::MountedRider:
  case Engine::Core::DeathSequenceProfile::Elephant:
  case Engine::Core::DeathSequenceProfile::Horse:
  default:
    return 0U;
  }
}

void begin_death_sequence(Engine::Core::Entity* target,
                          Engine::Core::Entity* attacker) {
  if (target == nullptr) {
    return;
  }
  auto const profile = Engine::Core::resolve_death_profile(*target);
  Engine::Core::begin_death_sequence(*target,
                                     resolve_death_variant(target, attacker, profile));
}

} // namespace Game::Systems::Combat
