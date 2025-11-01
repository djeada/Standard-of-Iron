#include "troop_catalog.h"

#include <utility>

namespace Game::Units {

auto TroopCatalog::instance() -> TroopCatalog & {
  static TroopCatalog inst;
  return inst;
}

TroopCatalog::TroopCatalog() { register_defaults(); }

void TroopCatalog::register_class(TroopClass troop_class) {
  m_classes[troop_class.unit_type] = std::move(troop_class);
}

auto TroopCatalog::get_class(Game::Units::TroopType type) const
    -> const TroopClass * {
  auto it = m_classes.find(type);
  if (it != m_classes.end()) {
    return &it->second;
  }
  return nullptr;
}

auto TroopCatalog::get_class_or_fallback(Game::Units::TroopType type) const
    -> const TroopClass & {
  auto it = m_classes.find(type);
  if (it != m_classes.end()) {
    return it->second;
  }
  return m_fallback;
}

void TroopCatalog::clear() { m_classes.clear(); }

void TroopCatalog::register_defaults() {
  m_fallback.display_name = "Unknown Troop";
  m_fallback.visuals.renderer_id = "troops/unknown";

  TroopClass archer{};
  archer.unit_type = Game::Units::TroopType::Archer;
  archer.display_name = "Archer";
  archer.production.cost = 50;
  archer.production.build_time = 5.0F;
  archer.production.priority = 10;
  archer.production.is_melee = false;

  archer.combat.health = 80;
  archer.combat.max_health = 80;
  archer.combat.speed = 3.0F;
  archer.combat.vision_range = 16.0F;
  archer.combat.ranged_range = 6.0F;
  archer.combat.ranged_damage = 12;
  archer.combat.ranged_cooldown = 1.2F;
  archer.combat.melee_range = 1.5F;
  archer.combat.melee_damage = 5;
  archer.combat.melee_cooldown = 0.8F;
  archer.combat.can_ranged = true;
  archer.combat.can_melee = true;

  archer.visuals.render_scale = 0.5F;
  archer.visuals.selection_ring_size = 1.2F;
  archer.visuals.selection_ring_ground_offset = 0.0F;
  archer.visuals.selection_ring_y_offset = 0.0F;
  archer.visuals.renderer_id = "troops/kingdom/archer";

  archer.individuals_per_unit = 20;
  archer.max_units_per_row = 5;

  register_class(std::move(archer));

  TroopClass swordsman{};
  swordsman.unit_type = Game::Units::TroopType::Swordsman;
  swordsman.display_name = "Swordsman";
  swordsman.production.cost = 90;
  swordsman.production.build_time = 7.0F;
  swordsman.production.priority = 10;
  swordsman.production.is_melee = true;

  swordsman.combat.health = 140;
  swordsman.combat.max_health = 140;
  swordsman.combat.speed = 2.2F;
  swordsman.combat.vision_range = 14.0F;
  swordsman.combat.ranged_range = 1.5F;
  swordsman.combat.ranged_damage = 6;
  swordsman.combat.ranged_cooldown = 1.8F;
  swordsman.combat.melee_range = 1.6F;
  swordsman.combat.melee_damage = 18;
  swordsman.combat.melee_cooldown = 0.6F;
  swordsman.combat.can_ranged = false;
  swordsman.combat.can_melee = true;

  swordsman.visuals.render_scale = 0.6F;
  swordsman.visuals.selection_ring_size = 1.1F;
  swordsman.visuals.selection_ring_ground_offset = 0.0F;
  swordsman.visuals.selection_ring_y_offset = 0.0F;
  swordsman.visuals.renderer_id = "troops/kingdom/swordsman";

  swordsman.individuals_per_unit = 15;
  swordsman.max_units_per_row = 5;

  register_class(std::move(swordsman));

  TroopClass spearman{};
  spearman.unit_type = Game::Units::TroopType::Spearman;
  spearman.display_name = "Spearman";
  spearman.production.cost = 75;
  spearman.production.build_time = 6.0F;
  spearman.production.priority = 5;
  spearman.production.is_melee = true;

  spearman.combat.health = 120;
  spearman.combat.max_health = 120;
  spearman.combat.speed = 2.5F;
  spearman.combat.vision_range = 15.0F;
  spearman.combat.ranged_range = 2.5F;
  spearman.combat.ranged_damage = 8;
  spearman.combat.ranged_cooldown = 1.5F;
  spearman.combat.melee_range = 2.5F;
  spearman.combat.melee_damage = 18;
  spearman.combat.melee_cooldown = 0.8F;
  spearman.combat.can_ranged = false;
  spearman.combat.can_melee = true;

  spearman.visuals.render_scale = 0.55F;
  spearman.visuals.selection_ring_size = 1.4F;
  spearman.visuals.selection_ring_ground_offset = 0.0F;
  spearman.visuals.selection_ring_y_offset = 0.0F;
  spearman.visuals.renderer_id = "troops/kingdom/spearman";

  spearman.individuals_per_unit = 24;
  spearman.max_units_per_row = 6;

  register_class(std::move(spearman));

  TroopClass mounted_knight{};
  mounted_knight.unit_type = Game::Units::TroopType::MountedKnight;
  mounted_knight.display_name = "Mounted Knight";
  mounted_knight.production.cost = 150;
  mounted_knight.production.build_time = 10.0F;
  mounted_knight.production.priority = 15;
  mounted_knight.production.is_melee = true;

  mounted_knight.combat.health = 200;
  mounted_knight.combat.max_health = 200;
  mounted_knight.combat.speed = 8.0F;
  mounted_knight.combat.vision_range = 16.0F;
  mounted_knight.combat.ranged_range = 1.5F;
  mounted_knight.combat.ranged_damage = 5;
  mounted_knight.combat.ranged_cooldown = 2.0F;
  mounted_knight.combat.melee_range = 2.0F;
  mounted_knight.combat.melee_damage = 25;
  mounted_knight.combat.melee_cooldown = 0.8F;
  mounted_knight.combat.can_ranged = false;
  mounted_knight.combat.can_melee = true;

  mounted_knight.visuals.render_scale = 0.8F;
  mounted_knight.visuals.selection_ring_size = 2.0F;
  mounted_knight.visuals.selection_ring_ground_offset = 1.35F;
  mounted_knight.visuals.selection_ring_y_offset = 0.0F;
  mounted_knight.visuals.renderer_id = "troops/kingdom/mounted_knight";

  mounted_knight.individuals_per_unit = 9;
  mounted_knight.max_units_per_row = 3;

  register_class(std::move(mounted_knight));
}

} // namespace Game::Units
