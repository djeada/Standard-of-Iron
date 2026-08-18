#include "troop_profile_service.h"

#include <algorithm>

#include "nation_registry.h"
#include "units/troop_catalog.h"

namespace Game::Systems {
namespace {

constexpr float k_archer_range_multiplier = 1.5F;

void apply_archer_range_bonus(Game::Units::TroopType type, TroopProfile& profile) {
  switch (type) {
  case Game::Units::TroopType::Archer:
  case Game::Units::TroopType::HorseArcher:
    profile.combat.ranged_range *= k_archer_range_multiplier;
    break;
  default:
    break;
  }
}

} // namespace

auto TroopProfileService::instance() -> TroopProfileService& {
  static TroopProfileService inst;
  return inst;
}

auto TroopProfile::has_ability(const std::string& ability_id) const -> bool {
  return std::find(abilities.begin(), abilities.end(), ability_id) != abilities.end();
}

void TroopProfileService::clear() {
  m_cache.clear();
}

void TroopProfileService::prime() {
  for (const auto& nation : NationRegistry::instance().get_all_nations()) {
    for (const auto& [type, troop_class] :
         Game::Units::TroopCatalog::instance().get_all_classes()) {

      (void)get_profile(nation.id, type);
    }
  }
}

auto TroopProfileService::find_profile(
    NationID nation_id, Game::Units::TroopType type) const -> const TroopProfile* {
  auto const nation_cache = m_cache.find(nation_id);
  if (nation_cache == m_cache.end()) {
    return nullptr;
  }
  auto const profile = nation_cache->second.find(type);
  return profile != nation_cache->second.end() ? &profile->second : nullptr;
}

auto TroopProfileService::get_profile(NationID nation_id,
                                      Game::Units::TroopType type) -> TroopProfile {
  return get_profile_ref(nation_id, type);
}

auto TroopProfileService::get_profile_ref(
    NationID nation_id, Game::Units::TroopType type) -> const TroopProfile& {
  auto& nation_cache = m_cache[nation_id];
  auto cached = nation_cache.find(type);
  if (cached != nation_cache.end()) {
    return cached->second;
  }

  const Nation* nation = NationRegistry::instance().get_nation(nation_id);
  if (nation == nullptr) {
    const auto fallback_id = NationRegistry::instance().default_nation_id();
    nation = NationRegistry::instance().get_nation(fallback_id);
    if (nation == nullptr) {
      const auto& all = NationRegistry::instance().get_all_nations();
      if (all.empty()) {
        const auto& catalog_class =
            Game::Units::TroopCatalog::instance().get_class_or_fallback(type);
        TroopProfile fallback{};
        fallback.display_name = catalog_class.display_name;
        fallback.production = catalog_class.production;
        fallback.combat = catalog_class.combat;
        fallback.visuals = catalog_class.visuals;
        fallback.individuals_per_unit = catalog_class.individuals_per_unit;
        fallback.max_units_per_row = catalog_class.max_units_per_row;
        fallback.doctrine = "rome";
        apply_archer_range_bonus(type, fallback);
        return nation_cache.emplace(type, std::move(fallback)).first->second;
      }
      nation = &all.front();
    }
  }

  TroopProfile profile = build_profile(*nation, type);
  return nation_cache.emplace(type, std::move(profile)).first->second;
}

auto TroopProfileService::build_profile(const Nation& nation,
                                        Game::Units::TroopType type) -> TroopProfile {
  const auto& catalog_class =
      Game::Units::TroopCatalog::instance().get_class_or_fallback(type);

  TroopProfile profile{};
  profile.display_name = catalog_class.display_name;
  profile.production = catalog_class.production;
  profile.combat = catalog_class.combat;
  profile.visuals = catalog_class.visuals;
  profile.lore = catalog_class.lore;
  profile.documented_abilities = catalog_class.documented_abilities;
  profile.individuals_per_unit = catalog_class.individuals_per_unit;
  profile.max_units_per_row = catalog_class.max_units_per_row;
  profile.doctrine = nation.doctrine;

  if (const auto* nation_troop = nation.get_troop(type)) {
    profile.display_name = nation_troop->display_name;
    profile.production.cost = nation_troop->cost;
    if (!nation_troop->resource_costs.empty()) {
      profile.production.resource_costs = nation_troop->resource_costs;
    }
    profile.production.build_time = nation_troop->build_time;
    profile.production.priority = nation_troop->priority;
    profile.production.is_melee = nation_troop->is_melee;
  }

  auto variant_it = nation.troop_variants.find(type);
  if (variant_it != nation.troop_variants.end()) {
    const auto& variant = variant_it->second;
    if (variant.health) {
      profile.combat.health = *variant.health;
    }
    if (variant.max_health) {
      profile.combat.max_health = *variant.max_health;
    }
    if (variant.speed) {
      profile.combat.speed = *variant.speed;
    }
    if (variant.vision_range) {
      profile.combat.vision_range = *variant.vision_range;
    }
    if (variant.attack_damage) {
      profile.combat.ranged_damage = *variant.attack_damage;
    }
    if (variant.attack_range) {
      profile.combat.ranged_range = *variant.attack_range;
    }
    if (variant.attack_cooldown) {
      profile.combat.ranged_cooldown = *variant.attack_cooldown;
    }
    if (variant.melee_damage) {
      profile.combat.melee_damage = *variant.melee_damage;
    }
    if (variant.melee_range) {
      profile.combat.melee_range = *variant.melee_range;
    }
    if (variant.melee_cooldown) {
      profile.combat.melee_cooldown = *variant.melee_cooldown;
    }
    if (variant.individuals_per_unit) {
      profile.individuals_per_unit = *variant.individuals_per_unit;
    }
    if (variant.max_units_per_row) {
      profile.max_units_per_row = *variant.max_units_per_row;
    }
    if (variant.selection_ring_size) {
      profile.visuals.selection_ring_size = *variant.selection_ring_size;
    }
    if (variant.selection_ring_ground_offset) {
      profile.visuals.selection_ring_ground_offset =
          *variant.selection_ring_ground_offset;
    }
    if (variant.formation_spacing) {
      profile.visuals.formation_spacing = *variant.formation_spacing;
    }
    if (variant.renderer_id) {
      profile.visuals.renderer_id = *variant.renderer_id;
    }
    if (variant.render_scale) {
      profile.visuals.render_scale = *variant.render_scale;
    }
    if (variant.doctrine) {
      profile.doctrine = *variant.doctrine;
    }
    if (variant.can_ranged) {
      profile.combat.can_ranged = *variant.can_ranged;
    }
    if (variant.can_melee) {
      profile.combat.can_melee = *variant.can_melee;
    }
    if (variant.max_stamina) {
      profile.combat.max_stamina = *variant.max_stamina;
    }
    if (variant.stamina_regen_rate) {
      profile.combat.stamina_regen_rate = *variant.stamina_regen_rate;
    }
    if (variant.stamina_depletion_rate) {
      profile.combat.stamina_depletion_rate = *variant.stamina_depletion_rate;
    }
    if (!variant.abilities.empty()) {
      profile.abilities = variant.abilities;
    }
    if (variant.lore_history) {
      profile.lore.history = *variant.lore_history;
    }
  }

  apply_archer_range_bonus(type, profile);
  return profile;
}

} // namespace Game::Systems
