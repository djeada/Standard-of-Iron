#include "app/economy/unit_profile.h"

#include <QStringList>

#include <algorithm>

#include "app/economy/resource_text.h"
#include "game/formation/formation_roles.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/commander_catalog.h"
#include "game/units/troop_catalog.h"
#include "game/units/troop_type.h"
#include "game/util/asset_text.h"

namespace App::Economy {

namespace {

using Game::Util::k_commanders_context;
using Game::Util::k_units_context;
using Game::Util::tr_asset;

[[nodiscard]] auto damage_per_second(int damage, float cooldown) -> double {
  if (cooldown <= 0.0F) {
    return 0.0;
  }
  return static_cast<double>(damage) / static_cast<double>(cooldown);
}

void write_lore(QVariantMap& info, const Game::Units::TroopLore& lore) {
  info["role"] = lore.role.empty() ? QString() : tr_asset(k_units_context, lore.role);
  info["strengths"] =
      lore.strengths.empty() ? QString() : tr_asset(k_units_context, lore.strengths);
  info["weaknesses"] =
      lore.weaknesses.empty() ? QString() : tr_asset(k_units_context, lore.weaknesses);
  info["history"] =
      lore.history.empty() ? QString() : tr_asset(k_units_context, lore.history);
  info["has_lore"] = !lore.empty();
}

void write_abilities(QVariantMap& info, const std::vector<std::string>& ability_ids) {
  const auto& catalog = Game::Units::TroopCatalog::instance();
  QVariantList abilities;
  for (const auto& ability_id : ability_ids) {
    QVariantMap entry;
    entry["id"] = QString::fromStdString(ability_id);
    if (const auto* definition = catalog.get_ability(ability_id)) {
      entry["name"] = tr_asset(k_units_context, definition->display_name);
      entry["effect"] = tr_asset(k_units_context, definition->effect);
    } else {
      entry["name"] = QString::fromStdString(ability_id);
      entry["effect"] = QString();
    }
    abilities.append(entry);
  }
  info["abilities"] = abilities;
}

void write_commander_prose(QVariantMap& info,
                           const Game::Units::CommanderDefinition& commander) {
  info["is_commander"] = true;
  info["strategic_identity"] =
      tr_asset(k_commanders_context, commander.strategic_identity);
  info["recruitment_effect"] =
      tr_asset(k_commanders_context, commander.recruitment_effect);
  info["battlefield_role"] = tr_asset(k_commanders_context, commander.battlefield_role);
  info["passive_aura"] = tr_asset(k_commanders_context, commander.passive_aura);
  info["bonus_type"] = QString::fromStdString(commander.bonus_type);
  info["bonus_summary"] = tr_asset(k_commanders_context, commander.bonus_summary);
  info["aura_bonus_value"] = static_cast<double>(commander.aura_bonus_value);
  info["rally_ability"] = tr_asset(k_commanders_context, commander.rally_ability);
  info["death_consequence"] =
      tr_asset(k_commanders_context, commander.death_consequence);
  info["visual_requirements"] = QString::fromStdString(commander.visual_requirements);
  info["bodyguard_count"] = commander.bodyguard_count;
  info["aura_radius"] = static_cast<double>(commander.aura_radius);
  info["rally_cooldown"] = static_cast<double>(commander.rally_cooldown);

  info["role"] = tr_asset(k_commanders_context, commander.battlefield_role);
  info["strengths"] = tr_asset(k_commanders_context, commander.strengths);
  info["weaknesses"] = tr_asset(k_commanders_context, commander.weaknesses);
  info["history"] = tr_asset(k_commanders_context, commander.strategic_identity);
  info["has_lore"] = true;
}

} // namespace

auto unit_profile(const QString& unit_type, const QString& nation_id) -> QVariantMap {
  QVariantMap info;
  info["unit_type"] = unit_type;
  info["display_name"] = unit_type;
  info["is_commander"] = false;
  info["valid"] = false;
  info["role_tags"] = QStringList{};
  info["abilities"] = QVariantList{};
  write_lore(info, {});

  const auto troop_type = Game::Units::try_parse_troop_type(unit_type.toStdString());
  if (!troop_type.has_value()) {
    return info;
  }

  auto& registry = Game::Systems::NationRegistry::instance();
  const auto parsed_nation =
      Game::Systems::nation_id_from_string(nation_id.toStdString());
  const auto resolved_nation = parsed_nation.value_or(registry.default_nation_id());
  const auto profile = Game::Systems::TroopProfileService::instance().get_profile(
      resolved_nation, *troop_type);

  info["valid"] = true;
  info["nation_id"] = nation_id;
  info["display_name"] = tr_asset(k_units_context, profile.display_name);

  info["health"] = profile.combat.max_health;
  info["speed"] = static_cast<double>(profile.combat.speed);
  info["vision_range"] = static_cast<double>(profile.combat.vision_range);
  info["can_ranged"] = profile.combat.can_ranged;
  info["can_melee"] = profile.combat.can_melee;
  info["melee_damage"] = profile.combat.melee_damage;
  info["melee_range"] = static_cast<double>(profile.combat.melee_range);
  info["melee_cooldown"] = static_cast<double>(profile.combat.melee_cooldown);
  info["ranged_damage"] = profile.combat.ranged_damage;
  info["ranged_range"] = static_cast<double>(profile.combat.ranged_range);
  info["ranged_cooldown"] = static_cast<double>(profile.combat.ranged_cooldown);

  const double melee_dps =
      damage_per_second(profile.combat.melee_damage, profile.combat.melee_cooldown);
  const double ranged_dps =
      damage_per_second(profile.combat.ranged_damage, profile.combat.ranged_cooldown);
  const bool prefers_ranged = profile.combat.can_ranged && ranged_dps >= melee_dps;

  info["attack_damage"] =
      prefers_ranged ? profile.combat.ranged_damage : profile.combat.melee_damage;
  info["attack_range"] = static_cast<double>(
      prefers_ranged ? profile.combat.ranged_range : profile.combat.melee_range);
  info["attack_cooldown"] = static_cast<double>(
      prefers_ranged ? profile.combat.ranged_cooldown : profile.combat.melee_cooldown);
  info["damage_per_second"] = prefers_ranged ? ranged_dps : melee_dps;
  info["prefers_ranged"] = prefers_ranged;

  info["cost"] = profile.production.cost;
  info["population_cost"] = profile.production.cost;
  info["resource_costs"] = to_variant_map(profile.production.resource_costs);
  info["build_time"] = static_cast<double>(profile.production.build_time);
  info["individuals_per_unit"] = profile.individuals_per_unit;

  const auto& catalog_class =
      Game::Units::TroopCatalog::instance().get_class_or_fallback(*troop_type);
  if (catalog_class.formation_profile) {
    info["role_tags"] =
        Game::Formation::role_tag_set_to_labels(catalog_class.formation_profile->roles);
  }

  write_lore(info, profile.lore);
  write_abilities(info, profile.documented_abilities);

  if (const auto* commander = Game::Units::commander_definition(*troop_type)) {
    write_commander_prose(info, *commander);
  }

  return info;
}

} // namespace App::Economy
