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

  TroopClass horse_swordsman{};
  horse_swordsman.unit_type = Game::Units::TroopType::MountedKnight;
  horse_swordsman.display_name = "Mounted Knight";
  horse_swordsman.production.cost = 150;
  horse_swordsman.production.build_time = 10.0F;
  horse_swordsman.production.priority = 15;
  horse_swordsman.production.is_melee = true;

  horse_swordsman.combat.health = 200;
  horse_swordsman.combat.max_health = 200;
  horse_swordsman.combat.speed = 4.F;
  horse_swordsman.combat.vision_range = 16.0F;
  horse_swordsman.combat.ranged_range = 1.5F;
  horse_swordsman.combat.ranged_damage = 5;
  horse_swordsman.combat.ranged_cooldown = 2.0F;
  horse_swordsman.combat.melee_range = 2.0F;
  horse_swordsman.combat.melee_damage = 25;
  horse_swordsman.combat.melee_cooldown = 0.8F;
  horse_swordsman.combat.can_ranged = false;
  horse_swordsman.combat.can_melee = true;

  horse_swordsman.visuals.render_scale = 0.8F;
  horse_swordsman.visuals.selection_ring_size = 2.0F;
  horse_swordsman.visuals.selection_ring_ground_offset = 1.35F;
  horse_swordsman.visuals.selection_ring_y_offset = 0.0F;
  horse_swordsman.visuals.renderer_id = "troops/kingdom/horse_swordsman";

  horse_swordsman.individuals_per_unit = 9;
  horse_swordsman.max_units_per_row = 3;

  TroopClass horse_archer{};
  horse_archer.unit_type = Game::Units::TroopType::HorseArcher;
  horse_archer.display_name = "Horse Archer";
  horse_archer.production.cost = 120;
  horse_archer.production.build_time = 9.0F;
  horse_archer.production.priority = 12;
  horse_archer.production.is_melee = false;

  horse_archer.combat.health = 160;
  horse_archer.combat.max_health = 160;
  horse_archer.combat.speed = 3.0F;
  horse_archer.combat.vision_range = 15.0F;
  horse_archer.combat.ranged_range = 7.0F;
  horse_archer.combat.ranged_damage = 12;
  horse_archer.combat.ranged_cooldown = 2.2F;
  horse_archer.combat.melee_range = 1.5F;
  horse_archer.combat.melee_damage = 10;
  horse_archer.combat.melee_cooldown = 1.0F;
  horse_archer.combat.can_ranged = true;
  horse_archer.combat.can_melee = true;

  horse_archer.visuals.render_scale = 0.8F;
  horse_archer.visuals.selection_ring_size = 2.0F;
  horse_archer.visuals.selection_ring_ground_offset = 1.35F;
  horse_archer.visuals.selection_ring_y_offset = 0.0F;
  horse_archer.visuals.renderer_id = "troops/kingdom/horse_archer";

  horse_archer.individuals_per_unit = 8;
  horse_archer.max_units_per_row = 3;

  register_class(std::move(horse_archer));

  register_class(std::move(horse_swordsman));

  TroopClass healer{};
  healer.unit_type = Game::Units::TroopType::Healer;
  healer.display_name = "Healer";
  healer.production.cost = 75;
  healer.production.build_time = 7.0F;
  healer.production.priority = 8;
  healer.production.is_melee = false;

  healer.combat.health = 100;
  healer.combat.max_health = 100;
  healer.combat.speed = 2.5F;
  healer.combat.vision_range = 14.0F;
  healer.combat.ranged_range = 8.0F;
  healer.combat.ranged_damage = 5;
  healer.combat.ranged_cooldown = 2.0F;
  healer.combat.melee_range = 1.5F;
  healer.combat.melee_damage = 3;
  healer.combat.melee_cooldown = 1.5F;
  healer.combat.can_ranged = false;
  healer.combat.can_melee = true;

  healer.visuals.render_scale = 0.55F;
  healer.visuals.selection_ring_size = 1.2F;
  healer.visuals.selection_ring_ground_offset = 0.0F;
  healer.visuals.selection_ring_y_offset = 0.0F;
  healer.visuals.renderer_id = "troops/kingdom/healer";

  healer.individuals_per_unit = 1;
  healer.max_units_per_row = 1;

  register_class(std::move(healer));

  TroopClass horse_spearman{};
  horse_spearman.unit_type = Game::Units::TroopType::HorseSpearman;
  horse_spearman.display_name = "Horse Spearman";
  horse_spearman.production.cost = 130;
  horse_spearman.production.build_time = 9.5F;
  horse_spearman.production.priority = 13;
  horse_spearman.production.is_melee = true;

  horse_spearman.combat.health = 180;
  horse_spearman.combat.max_health = 180;
  horse_spearman.combat.speed = 3.0F;
  horse_spearman.combat.vision_range = 15.0F;
  horse_spearman.combat.ranged_range = 2.5F;
  horse_spearman.combat.ranged_damage = 9;
  horse_spearman.combat.ranged_cooldown = 1.8F;
  horse_spearman.combat.melee_range = 2.2F;
  horse_spearman.combat.melee_damage = 20;
  horse_spearman.combat.melee_cooldown = 0.9F;
  horse_spearman.combat.can_ranged = false;
  horse_spearman.combat.can_melee = true;

  horse_spearman.visuals.render_scale = 0.8F;
  horse_spearman.visuals.selection_ring_size = 2.0F;
  horse_spearman.visuals.selection_ring_ground_offset = 1.35F;
  horse_spearman.visuals.selection_ring_y_offset = 0.0F;
  horse_spearman.visuals.renderer_id = "troops/kingdom/horse_spearman";

  horse_spearman.individuals_per_unit = 8;
  horse_spearman.max_units_per_row = 3;

  register_class(std::move(horse_spearman));
}

} // namespace Game::Units
