#include "troop_profile_service.h"

#include "nation_registry.h"
#include "units/troop_catalog.h"

namespace Game::Systems {

auto TroopProfileService::instance() -> TroopProfileService & {
  static TroopProfileService inst;
  return inst;
}

void TroopProfileService::clear() { m_cache.clear(); }

auto TroopProfileService::get_profile(
    NationID nation_id, Game::Units::TroopType type) -> TroopProfile {
  auto &nation_cache = m_cache[nation_id];
  auto cached = nation_cache.find(type);
  if (cached != nation_cache.end()) {
    return cached->second;
  }

  const Nation *nation = NationRegistry::instance().get_nation(nation_id);
  if (nation == nullptr) {
    const auto fallback_id = NationRegistry::instance().default_nation_id();
    nation = NationRegistry::instance().get_nation(fallback_id);
    if (nation == nullptr) {
      const auto &all = NationRegistry::instance().get_all_nations();
      if (all.empty()) {
        const auto &catalog_class =
            Game::Units::TroopCatalog::instance().get_class_or_fallback(type);
        TroopProfile fallback{};
        fallback.display_name = catalog_class.display_name;
        fallback.production = catalog_class.production;
        fallback.combat = catalog_class.combat;
        fallback.visuals = catalog_class.visuals;
        fallback.individuals_per_unit = catalog_class.individuals_per_unit;
        fallback.max_units_per_row = catalog_class.max_units_per_row;
        fallback.formation_type = FormationType::Roman;
        return fallback;
      }
      nation = &all.front();
    }
  }

  TroopProfile profile = build_profile(*nation, type);
  nation_cache.emplace(type, profile);
  return profile;
}

auto TroopProfileService::build_profile(
    const Nation &nation, Game::Units::TroopType type) -> TroopProfile {
  const auto &catalog_class =
      Game::Units::TroopCatalog::instance().get_class_or_fallback(type);

  TroopProfile profile{};
  profile.display_name = catalog_class.display_name;
  profile.production = catalog_class.production;
  profile.combat = catalog_class.combat;
  profile.visuals = catalog_class.visuals;
  profile.individuals_per_unit = catalog_class.individuals_per_unit;
  profile.max_units_per_row = catalog_class.max_units_per_row;
  profile.formation_type = nation.formation_type;

  if (const auto *nation_troop = nation.get_troop(type)) {
    profile.display_name = nation_troop->display_name;
    profile.production.cost = nation_troop->cost;
    profile.production.build_time = nation_troop->build_time;
    profile.production.priority = nation_troop->priority;
    profile.production.is_melee = nation_troop->is_melee;
  }

  auto variant_it = nation.troop_variants.find(type);
  if (variant_it != nation.troop_variants.end()) {
    const auto &variant = variant_it->second;
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
    if (variant.selection_ring_y_offset) {
      profile.visuals.selection_ring_y_offset =
          *variant.selection_ring_y_offset;
    }
    if (variant.selection_ring_ground_offset) {
      profile.visuals.selection_ring_ground_offset =
          *variant.selection_ring_ground_offset;
    }
    if (variant.renderer_id) {
      profile.visuals.renderer_id = *variant.renderer_id;
    }
    if (variant.render_scale) {
      profile.visuals.render_scale = *variant.render_scale;
    }
    if (variant.formation_type) {
      profile.formation_type = *variant.formation_type;
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
  }

  return profile;
}

} // namespace Game::Systems
