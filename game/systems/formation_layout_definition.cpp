#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "../core/component.h"
#include "../formation/unit_layout_resolver.h"
#include "../units/spawn_type.h"
#include "../units/troop_catalog.h"
#include "../units/troop_config.h"
#include "formation_combat_geometry.h"
#include "formation_geometry_internal.h"
#include "troop_profile_service.h"

namespace Game::Systems::FormationCombat {
namespace {

auto resolve_layout_id(const Engine::Core::UnitComponent& unit,
                       const Game::Formation::FormationDoctrineId& doctrine,
                       Game::Formation::UnitLayoutState state) noexcept
    -> Game::Formation::UnitLayoutId {
  auto const troop_type = Game::Units::spawn_typeToTroopType(unit.spawn_type);
  if (!troop_type.has_value()) {
    return Game::Formation::UnitLayoutLibrary::instance().resolve(
        doctrine, "close_order_infantry");
  }
  return Game::Formation::select_unit_layout(doctrine, *troop_type, state);
}

void apply_unit_overrides(const Engine::Core::UnitComponent& unit,
                          FormationDefinition& definition) {
  if (unit.render_individuals_per_unit_override > 0) {
    definition.total_count = unit.render_individuals_per_unit_override;
  } else if (unit.squad_strength > 0) {
    definition.total_count = std::min(definition.total_count, unit.squad_strength);
  }
  definition.total_count = std::max(1, definition.total_count);
  if (unit.formation_files_override > 0) {
    definition.max_per_row = unit.formation_files_override;
  }
  definition.max_per_row =
      std::clamp(definition.max_per_row, 1, definition.total_count);
  definition.spacing = std::max(0.1F, definition.spacing);
}

constexpr float k_max_slot_body_radius = 3.0F;

} // namespace

auto formation_seed(const Engine::Core::Entity& entity) noexcept -> std::uint32_t {
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  std::uint32_t seed = entity.get_id() * 0x9E3779B9U;
  if (unit != nullptr) {
    seed ^= static_cast<std::uint32_t>(unit->owner_id) * 0x85EBCA6BU;
  }
  return seed;
}

auto max_contact_extent() -> float {
  static const float extent = [] {
    float widest = 0.0F;
    for (const auto& [_, troop_class] :
         Game::Units::TroopCatalog::instance().get_all_classes()) {
      const float row_width =
          static_cast<float>(std::max(1, troop_class.max_units_per_row)) *
          std::max(0.1F, troop_class.visuals.formation_spacing);
      widest = std::max(widest, row_width);
    }

    return widest * 0.5F + k_max_slot_body_radius;
  }();
  return extent;
}

auto resolve_definition(const Engine::Core::UnitComponent& unit)
    -> FormationDefinition {
  if (auto troop_type = Game::Units::spawn_typeToTroopType(unit.spawn_type);
      unit.uses_nation_formation_profile && troop_type) {
    auto const& profile =
        TroopProfileService::instance().get_profile_ref(unit.nation_id, *troop_type);
    return resolve_definition(unit, profile);
  }

  FormationDefinition definition;
  definition.total_count =
      Game::Units::TroopConfig::instance().get_individuals_per_unit(unit.spawn_type);
  definition.max_per_row =
      Game::Units::TroopConfig::instance().get_max_units_per_row(unit.spawn_type);
  definition.spacing =
      Game::Units::TroopConfig::instance().get_formation_spacing(unit.spawn_type);
  definition.doctrine = Game::Formation::default_doctrine_for_nation(unit.nation_id);
  definition.layout =
      resolve_layout_id(unit, definition.doctrine, definition.layout_state);
  apply_unit_overrides(unit, definition);
  return definition;
}

auto resolve_definition(const Engine::Core::UnitComponent& unit,
                        const TroopProfile& profile) -> FormationDefinition {
  FormationDefinition definition;
  definition.total_count = profile.individuals_per_unit;
  definition.max_per_row = profile.max_units_per_row;
  definition.spacing = profile.visuals.formation_spacing;
  definition.doctrine = profile.doctrine;
  definition.layout =
      resolve_layout_id(unit, definition.doctrine, definition.layout_state);
  apply_unit_overrides(unit, definition);
  return definition;
}

auto living_slot_count(const Engine::Core::Entity& entity, int total_count) -> int {
  if (total_count <= 0) {
    return 0;
  }

  auto const* roster =
      entity.get_component<Engine::Core::FormationRosterPresentationComponent>();
  if (roster != nullptr &&
      roster->alive.size() == static_cast<std::size_t>(total_count)) {
    return static_cast<int>(std::count_if(
        roster->alive.begin(), roster->alive.end(), [](std::uint8_t alive) {
          return alive != 0U;
        }));
  }

  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return total_count;
  }
  return Engine::Core::resolve_surviving_individual_count(
      unit->health, unit->max_health, total_count);
}

auto living_slot_indices(const Engine::Core::Entity& entity,
                         int total_count) -> std::vector<std::uint16_t> {
  std::vector<std::uint16_t> living;
  if (total_count <= 0) {
    return living;
  }
  living.reserve(static_cast<std::size_t>(total_count));

  auto const* roster =
      entity.get_component<Engine::Core::FormationRosterPresentationComponent>();
  if (roster != nullptr &&
      roster->alive.size() == static_cast<std::size_t>(total_count)) {
    for (int idx = 0; idx < total_count; ++idx) {
      if (roster->alive[static_cast<std::size_t>(idx)] != 0U) {
        living.push_back(static_cast<std::uint16_t>(idx));
      }
    }
    return living;
  }

  int const live_count = living_slot_count(entity, total_count);
  for (int idx = std::max(0, total_count - live_count); idx < total_count; ++idx) {
    living.push_back(static_cast<std::uint16_t>(idx));
  }
  return living;
}

auto layout_reach_for_files(const Engine::Core::Entity& entity,
                            int files) -> LayoutReach {
  LayoutReach reach;
  auto const* unit_ptr = entity.get_component<Engine::Core::UnitComponent>();
  auto const* transform = entity.get_component<Engine::Core::TransformComponent>();
  if (unit_ptr == nullptr) {
    return reach;
  }
  float const scale =
      transform != nullptr ? std::max(transform->scale.x, transform->scale.z) : 1.0F;
  reach.body_radius = std::max(0.05F, scale * 0.5F);
  if (entity.has_component<Engine::Core::BuildingComponent>() ||
      entity.has_component<Engine::Core::ElephantComponent>()) {
    return reach;
  }
  Engine::Core::UnitComponent unit = *unit_ptr;
  unit.formation_files_override = std::max(0, files);
  auto const definition = resolve_definition(unit);
  int const count = definition.total_count;
  int const cols = std::max(1, definition.max_per_row);
  int const rows = std::max(1, (count + cols - 1) / cols);
  auto const seed = formation_seed(entity);
  for (int idx = 0; idx < count; ++idx) {
    auto const slot = Game::Formation::rank_slot_for(idx, count, cols);
    Game::Formation::UnitLayoutQuery query;
    query.layout = definition.layout;
    query.index = idx;
    query.row = slot.row;
    query.col = slot.col;
    query.rows = rows;
    query.cols = cols;
    query.count = count;
    query.spacing = definition.spacing;
    query.seed = seed;
    auto const offset = Game::Formation::UnitLayoutSystem::instance().offset(query);
    reach.half_x = std::max(reach.half_x, std::abs(offset.offset_x));
    reach.half_z = std::max(reach.half_z, std::abs(offset.offset_z));
  }
  reach.files = cols;
  return reach;
}

auto has_formation_slots(const Engine::Core::Entity& entity) -> bool {
  if (entity.has_component<Engine::Core::BuildingComponent>() ||
      entity.has_component<Engine::Core::ElephantComponent>()) {
    return false;
  }
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  return unit != nullptr && Detail::definition_total_count(*unit) > 1;
}

namespace Detail {

auto definition_total_count(const Engine::Core::UnitComponent& unit) -> int {
  if (unit.render_individuals_per_unit_override > 0) {
    return unit.render_individuals_per_unit_override;
  }
  if (unit.squad_strength == 1) {
    return 1;
  }
  int total_count = 0;
  if (auto troop_type = Game::Units::spawn_typeToTroopType(unit.spawn_type);
      unit.uses_nation_formation_profile && troop_type) {
    total_count = TroopProfileService::instance()
                      .get_profile_ref(unit.nation_id, *troop_type)
                      .individuals_per_unit;
  } else {
    total_count =
        Game::Units::TroopConfig::instance().get_individuals_per_unit(unit.spawn_type);
  }
  if (unit.squad_strength > 0) {
    total_count = std::min(total_count, unit.squad_strength);
  }
  return std::max(1, total_count);
}

} // namespace Detail

} // namespace Game::Systems::FormationCombat
