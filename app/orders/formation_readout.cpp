#include "app/orders/formation_readout.h"

#include "game/core/component_core.h"
#include "game/core/world.h"
#include "game/formation/army_formation_registry.h"
#include "game/formation/formation_doctrine.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/spawn_type.h"
#include "game/util/asset_text.h"

namespace App::Controllers {

auto to_variant_map(const FormationOptionsReadout& readout) -> QVariantMap {
  QVariantMap map;
  map["intent"] = readout.intent;
  map["doctrine"] = readout.doctrine;
  map["doctrine_display_name"] = readout.doctrine_display_name;
  map["doctrine_locked"] = readout.doctrine_locked;
  map["frontage_scale"] = readout.frontage_scale;
  map["depth_scale"] = readout.depth_scale;
  map["spacing_scale"] = readout.spacing_scale;
  map["reserve_rows"] = readout.reserve_rows;
  map["preserve_member_order"] = readout.preserve_member_order;
  map["flank"] = readout.flank;
  map["ranged"] = readout.ranged;
  map["movement"] = readout.movement;
  map["movement_from_doctrine"] = readout.movement_from_doctrine;
  map["effective_movement_index"] = readout.effective_movement_index;
  map["mixed"] = readout.mixed;
  map["frontage"] = readout.frontage;
  map["blocked_slots"] = readout.blocked_slots;
  map["adjusted_slots"] = readout.adjusted_slots;
  map["warning"] = readout.warning;
  map["frontage_index"] = readout.frontage_index;
  map["depth_index"] = readout.depth_index;
  map["spacing_index"] = readout.spacing_index;
  map["flank_index"] = readout.flank_index;
  map["ranged_index"] = readout.ranged_index;
  map["reserve_index"] = readout.reserve_index;
  map["movement_index"] = readout.movement_index;
  map["mixed_index"] = readout.mixed_index;
  map["preserve_index"] = readout.preserve_index;
  map["intent_display_name"] = readout.intent_display_name;
  map["unit_count"] = readout.unit_count;
  map["single_unit"] = readout.single_unit;
  map["unit_label"] = readout.unit_label;
  map["gesture"] = readout.gesture;
  map["facing_degrees"] = readout.facing_degrees;
  map["facing_explicit"] = readout.facing_explicit;
  map["aim_distance"] = readout.aim_distance;
  map["placed_count"] = readout.placed_count;
  map["slot_count"] = readout.slot_count;
  map["ranks"] = readout.ranks;
  map["files"] = readout.files;
  map["plan_frontage"] = readout.plan_frontage;
  map["plan_depth"] = readout.plan_depth;
  map["plan_valid"] = readout.plan_valid;
  return map;
}

auto to_variant_map(const SelectedFormationStatus& status) -> QVariantMap {
  QVariantMap map;
  map["active"] = status.active;
  if (!status.active) {
    return map;
  }
  map["intent"] = status.intent;
  map["intent_display_name"] = status.intent_display_name;
  map["doctrine_display_name"] = status.doctrine_display_name;
  map["cohesion"] = status.cohesion;
  map["phase"] = status.phase;
  map["member_count"] = status.member_count;
  map["selected_in_group"] = status.selected_in_group;
  map["mixed_groups"] = status.mixed_groups;
  map["blocked_slots"] = status.blocked_slots;
  map["movement"] = status.movement;
  map["compressed"] = status.compressed;
  return map;
}

auto read_selected_formation(Engine::Core::World& world,
                             const std::vector<std::uint64_t>& selected)
    -> SelectedFormationStatus {
  auto& registry = Game::Formation::ArmyFormationRegistry::for_world(world);

  Game::Formation::FormationGroupID group = Game::Formation::k_invalid_group;
  int in_group = 0;
  int group_count = 0;
  for (auto const id : selected) {
    auto const owner = registry.group_of(id);
    if (owner == Game::Formation::k_invalid_group) {
      continue;
    }
    if (group == Game::Formation::k_invalid_group) {
      group = owner;
    }
    if (owner == group) {
      ++in_group;
    } else {
      ++group_count;
    }
  }

  const auto* formation = registry.find(group);
  if (formation == nullptr) {
    return {};
  }

  SelectedFormationStatus status;
  status.active = true;
  status.intent =
      QString::fromLatin1(Game::Formation::intent_to_string(formation->intent));
  status.intent_display_name = Game::Formation::intent_display_name(formation->intent);
  status.doctrine_display_name =
      QString::fromStdString(Game::Formation::DoctrineRegistry::instance()
                                 .get_or_neutral(formation->doctrine)
                                 .display_name);
  status.cohesion = formation->cohesion;
  status.phase = QString::fromLatin1(phase_to_string(formation->phase));
  status.member_count = static_cast<int>(formation->members.size());
  status.selected_in_group = in_group;
  status.mixed_groups = group_count > 0;
  status.blocked_slots = formation->blocked_slot_count();
  status.movement = QString::fromLatin1(
      Game::Formation::movement_policy_to_string(formation->options.movement_policy));
  status.compressed = formation->compressed;
  return status;
}

auto single_unit_label(Engine::Core::World* world,
                       const std::vector<std::uint64_t>& units) -> QString {
  if (world == nullptr || units.size() != 1) {
    return {};
  }
  const auto* unit = world->try_get<Engine::Core::UnitComponent>(units.front());
  if (unit == nullptr) {
    return {};
  }
  const auto troop_type = Game::Units::spawn_typeToTroopType(unit->spawn_type);
  if (!troop_type.has_value()) {
    return QString::fromStdString(Game::Units::spawn_typeToString(unit->spawn_type));
  }
  const auto profile = Game::Systems::TroopProfileService::instance().get_profile(
      unit->nation_id, *troop_type);
  return Game::Util::tr_asset(Game::Util::k_units_context, profile.display_name);
}

} // namespace App::Controllers
