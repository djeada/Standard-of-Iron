#include "combat_animation_timing.h"

#include <algorithm>
#include <optional>

#include "animation/clip_manifest.h"

namespace Game::Systems::Combat {

namespace {

auto fallback_melee_contact_time(float cooldown) noexcept -> float {
  return Engine::Core::CombatStateComponent::k_melee_contact_fraction * cooldown;
}

auto attack_family_for_combat_family(Engine::Core::CombatAttackFamily family) noexcept
    -> std::optional<Animation::AttackClipFamily> {
  using Engine::Core::CombatAttackFamily;
  switch (family) {
  case CombatAttackFamily::Sword:
    return Animation::AttackClipFamily::Sword;
  case CombatAttackFamily::Spear:
    return Animation::AttackClipFamily::Spear;
  case CombatAttackFamily::Bow:
    return Animation::AttackClipFamily::Bow;
  case CombatAttackFamily::None:
    return std::nullopt;
  }
  return std::nullopt;
}

auto is_mounted_spawn(Game::Units::SpawnType spawn_type) noexcept -> bool {
  using Game::Units::SpawnType;
  switch (spawn_type) {
  case SpawnType::MountedKnight:
  case SpawnType::HorseArcher:
  case SpawnType::HorseSpearman:
    return true;
  default:
    return false;
  }
}

auto humanoid_clip_profile_for_spawn(Game::Units::SpawnType spawn_type) noexcept
    -> Animation::HumanoidClipProfile {
  using Game::Units::SpawnType;
  switch (spawn_type) {
  case SpawnType::SkeletonSwordsman:
  case SpawnType::SkeletonArcher:
  case SpawnType::GravePriest:
    return Animation::HumanoidClipProfile::Skeleton;
  case SpawnType::Spearman:
  case SpawnType::HorseSpearman:
    return Animation::HumanoidClipProfile::SpearReady;
  case SpawnType::Knight:
  case SpawnType::MountedKnight:
  case SpawnType::RomanLegionOrganizer:
  case SpawnType::RomanVeteranConsul:
  case SpawnType::RomanFieldCommander:
  case SpawnType::CarthageMercenaryBroker:
  case SpawnType::CarthageCavalryPatron:
  case SpawnType::CarthageElephantMaster:
    return Animation::HumanoidClipProfile::SwordReady;
  default:
    return Animation::HumanoidClipProfile::Default;
  }
}

} // namespace

auto melee_contact_time_for_unit(Game::Units::SpawnType spawn_type,
                                 Engine::Core::CombatAttackFamily family,
                                 std::uint8_t attack_variant,
                                 float cooldown) noexcept -> float {
  if (cooldown <= 0.0F) {
    return fallback_melee_contact_time(cooldown);
  }

  auto const attack_family = attack_family_for_combat_family(family);
  if (!attack_family.has_value() ||
      *attack_family == Animation::AttackClipFamily::Bow) {
    return fallback_melee_contact_time(cooldown);
  }

  auto const clip_id = Animation::humanoid_attack_clip(
      *attack_family, is_mounted_spawn(spawn_type), attack_variant);
  auto const markers = Animation::authored_humanoid_clip_markers(
      clip_id, humanoid_clip_profile_for_spawn(spawn_type));
  if (markers.contact <= 0.0F || markers.contact >= 1.0F) {
    return fallback_melee_contact_time(cooldown);
  }
  return markers.contact * cooldown;
}

auto melee_contact_time_for_attack(const Engine::Core::Entity* attacker,
                                   float cooldown) noexcept -> float {
  if (attacker == nullptr) {
    return fallback_melee_contact_time(cooldown);
  }

  auto const* unit = attacker->get_component<Engine::Core::UnitComponent>();
  auto const* combat_state =
      attacker->get_component<Engine::Core::CombatStateComponent>();
  auto const* attack = attacker->get_component<Engine::Core::AttackComponent>();
  auto const spawn_type =
      unit != nullptr ? unit->spawn_type : Game::Units::SpawnType::Archer;
  auto const family = combat_state != nullptr
                          ? combat_state->attack_family
                          : (unit != nullptr && attack != nullptr
                                 ? Engine::Core::resolve_combat_attack_family(
                                       unit->spawn_type, attack->current_mode)
                                 : Engine::Core::CombatAttackFamily::None);
  auto const variant = combat_state != nullptr ? combat_state->attack_variant : 0U;
  return melee_contact_time_for_unit(spawn_type, family, variant, cooldown);
}

} // namespace Game::Systems::Combat
