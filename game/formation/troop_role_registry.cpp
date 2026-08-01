#include "troop_role_registry.h"

#include <algorithm>

namespace Game::Formation {

namespace {

using Game::Units::TroopType;

auto mask(std::initializer_list<RoleTag> tags) -> RoleTagSet {
  RoleTagSet set = 0U;
  for (auto tag : tags) {
    set |= to_mask(tag);
  }
  return set;
}

} // namespace

auto default_troop_formation_profile(TroopType troop) -> TroopFormationProfile {
  TroopFormationProfile profile;

  switch (troop) {
  case TroopType::Swordsman:
    profile.roles =
        mask({RoleTag::LineInfantry, RoleTag::HeavyInfantry, RoleTag::Shielded});
    profile.army_roles = {ArmyRole::Centre, ArmyRole::Vanguard, ArmyRole::Reserve};
    profile.unit_layout = "close_order_infantry";
    profile.defensive_layout = "shield_wall";
    profile.marching_layout = "marching_column";
    break;

  case TroopType::Spearman:
    profile.roles =
        mask({RoleTag::LineInfantry, RoleTag::SpearInfantry, RoleTag::Shielded});
    profile.army_roles = {ArmyRole::Centre, ArmyRole::Screen, ArmyRole::Reserve};
    profile.unit_layout = "spear_ranks";
    profile.defensive_layout = "spear_brace";
    profile.marching_layout = "marching_column";
    break;

  case TroopType::Archer:
    profile.roles = mask({RoleTag::Ranged, RoleTag::Skirmisher});
    profile.army_roles = {ArmyRole::Ranged, ArmyRole::Screen};
    profile.unit_layout = "loose_order_ranged";
    profile.defensive_layout = "loose_order_ranged";
    profile.marching_layout = "marching_column";
    break;

  case TroopType::SkeletonSwordsman:
    profile.roles = mask({RoleTag::LineInfantry,
                          RoleTag::Expendable,
                          RoleTag::Awakened,
                          RoleTag::Shielded});
    profile.army_roles = {ArmyRole::Vanguard, ArmyRole::Centre};
    profile.unit_layout = "burial_guard_block";
    profile.defensive_layout = "burial_guard_block";
    profile.marching_layout = "dense_advance";
    break;

  case TroopType::SkeletonArcher:
    profile.roles = mask({RoleTag::Ranged, RoleTag::Expendable, RoleTag::Awakened});
    profile.army_roles = {ArmyRole::Ranged};
    profile.unit_layout = "sepulcher_ranged_line";
    profile.defensive_layout = "sepulcher_ranged_line";
    profile.marching_layout = "dense_advance";
    break;

  case TroopType::GravePriest:
    profile.roles =
        mask({RoleTag::Caster, RoleTag::Support, RoleTag::Elite, RoleTag::Awakened});
    profile.army_roles = {ArmyRole::Reserve, ArmyRole::Command};
    profile.unit_layout = "procession";
    profile.defensive_layout = "procession";
    profile.marching_layout = "procession";
    break;

  case TroopType::MountedKnight:
    profile.roles = mask({RoleTag::Cavalry, RoleTag::Mounted, RoleTag::Shielded});
    profile.army_roles = {ArmyRole::LeftFlank, ArmyRole::RightFlank};
    profile.unit_layout = "cavalry_wedge";
    profile.defensive_layout = "cavalry_line";
    profile.marching_layout = "cavalry_column";
    break;

  case TroopType::HorseSpearman:
    profile.roles = mask({RoleTag::Cavalry, RoleTag::Mounted, RoleTag::SpearInfantry});
    profile.army_roles = {ArmyRole::LeftFlank, ArmyRole::RightFlank, ArmyRole::Screen};
    profile.unit_layout = "cavalry_wedge";
    profile.defensive_layout = "cavalry_line";
    profile.marching_layout = "cavalry_column";
    break;

  case TroopType::HorseArcher:
    profile.roles = mask(
        {RoleTag::Cavalry, RoleTag::Mounted, RoleTag::Ranged, RoleTag::Skirmisher});
    profile.army_roles = {ArmyRole::Screen, ArmyRole::LeftFlank, ArmyRole::RightFlank};
    profile.unit_layout = "cavalry_loose";
    profile.defensive_layout = "cavalry_loose";
    profile.marching_layout = "cavalry_column";
    break;

  case TroopType::Elephant:
    profile.roles = mask({RoleTag::Elephant, RoleTag::HeavyInfantry, RoleTag::Elite});
    profile.army_roles = {ArmyRole::Vanguard, ArmyRole::Centre};
    profile.unit_layout = "beast_spread";
    profile.defensive_layout = "beast_spread";
    profile.marching_layout = "beast_spread";
    break;

  case TroopType::Catapult:
  case TroopType::Ballista:
    profile.roles = mask({RoleTag::Siege, RoleTag::Ranged});
    profile.army_roles = {ArmyRole::Siege, ArmyRole::Reserve};
    profile.unit_layout = "siege_crew";
    profile.defensive_layout = "siege_crew";
    profile.marching_layout = "siege_crew";
    break;

  case TroopType::Healer:
    profile.roles = mask({RoleTag::Support, RoleTag::Caster});
    profile.army_roles = {ArmyRole::Reserve};
    profile.unit_layout = "support_cluster";
    profile.defensive_layout = "support_cluster";
    profile.marching_layout = "support_cluster";
    break;

  case TroopType::Builder:
    profile.roles = mask({RoleTag::Worker, RoleTag::Civilian});
    profile.army_roles = {ArmyRole::Reserve};
    profile.unit_layout = "work_party";
    profile.defensive_layout = "work_party";
    profile.marching_layout = "work_party";
    break;

  case TroopType::Civilian:
    profile.roles = mask({RoleTag::Civilian});
    profile.army_roles = {ArmyRole::Reserve};
    profile.unit_layout = "civilian_crowd";
    profile.defensive_layout = "civilian_crowd";
    profile.marching_layout = "civilian_crowd";
    break;

  case TroopType::RomanLegionOrganizer:
  case TroopType::RomanVeteranConsul:
  case TroopType::RomanFieldCommander:
  case TroopType::CarthageMercenaryBroker:
  case TroopType::CarthageCavalryPatron:
  case TroopType::CarthageElephantMaster:
    profile.roles = mask({RoleTag::Command, RoleTag::Elite});
    profile.army_roles = {ArmyRole::Command, ArmyRole::Reserve};
    profile.unit_layout = "command_retinue";
    profile.defensive_layout = "command_retinue";
    profile.marching_layout = "command_retinue";
    break;
  }

  if (profile.roles == 0U) {
    profile.roles = to_mask(RoleTag::LineInfantry);
    profile.army_roles = {ArmyRole::Centre};
    profile.unit_layout = "close_order_infantry";
    profile.defensive_layout = "close_order_infantry";
    profile.marching_layout = "marching_column";
  }

  return profile;
}

TroopRoleRegistry::TroopRoleRegistry() {
  reset_to_defaults();
}

auto TroopRoleRegistry::instance() -> TroopRoleRegistry& {
  static TroopRoleRegistry registry;
  return registry;
}

void TroopRoleRegistry::reset_to_defaults() {
  m_profiles.clear();
  constexpr int k_last_troop = static_cast<int>(TroopType::Builder);
  for (int i = 0; i <= k_last_troop; ++i) {
    auto const troop = static_cast<TroopType>(i);
    m_profiles[troop] = default_troop_formation_profile(troop);
  }
  m_fallback = default_troop_formation_profile(TroopType::Swordsman);
}

void TroopRoleRegistry::clear_overrides() {
  reset_to_defaults();
}

void TroopRoleRegistry::set_profile(TroopType troop, TroopFormationProfile profile) {
  m_profiles[troop] = std::move(profile);
}

void TroopRoleRegistry::merge_profile(TroopType troop,
                                      const TroopFormationProfile& overrides) {
  auto& target = m_profiles[troop];
  if (overrides.roles != 0U) {
    target.roles = overrides.roles;
  }
  if (!overrides.army_roles.empty()) {
    target.army_roles = overrides.army_roles;
  }
  if (!overrides.unit_layout.empty()) {
    target.unit_layout = overrides.unit_layout;
  }
  if (!overrides.defensive_layout.empty()) {
    target.defensive_layout = overrides.defensive_layout;
  }
  if (!overrides.marching_layout.empty()) {
    target.marching_layout = overrides.marching_layout;
  }
}

auto TroopRoleRegistry::profile(TroopType troop) const -> const TroopFormationProfile& {
  auto it = m_profiles.find(troop);
  return it == m_profiles.end() ? m_fallback : it->second;
}

auto TroopRoleRegistry::roles(TroopType troop) const -> RoleTagSet {
  return profile(troop).roles;
}

auto TroopRoleRegistry::army_roles(TroopType troop) const
    -> const std::vector<ArmyRole>& {
  return profile(troop).army_roles;
}

auto TroopRoleRegistry::prefers_army_role(TroopType troop,
                                          ArmyRole role) const -> bool {
  auto const& list = army_roles(troop);
  return std::find(list.begin(), list.end(), role) != list.end();
}

} // namespace Game::Formation
