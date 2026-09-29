#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "../core/component_core.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../systems/formation_combat_geometry.h"
#include "../systems/nation_registry.h"
#include "../units/spawn_type.h"
#include "../units/troop_config.h"
#include "army_formation_planner.h"
#include "troop_role_registry.h"

namespace Game::Formation {

namespace {

auto doctrine_for_entity(Engine::Core::World& world,
                         EntityID id) -> FormationDoctrineId {
  auto* entity = world.get_entity(id);
  if (entity == nullptr) {
    return k_neutral_doctrine;
  }
  const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return k_neutral_doctrine;
  }
  const auto* nation =
      Game::Systems::NationRegistry::instance().get_nation(unit->nation_id);
  if (nation != nullptr && !nation->doctrine.empty()) {
    return nation->doctrine;
  }
  return default_doctrine_for_nation(unit->nation_id);
}

void tabulate_extents_by_files(const Engine::Core::Entity& entity,
                               ArmyFormationMember& member) {
  constexpr int k_max_tabulated_files = 48;
  int const tabulated = std::min(member.individuals, k_max_tabulated_files);
  member.extents_by_files.reserve(static_cast<std::size_t>(tabulated));
  for (int files = 1; files <= tabulated; ++files) {
    auto const reach =
        Game::Systems::FormationCombat::layout_reach_for_files(entity, files);
    member.extents_by_files.emplace_back(reach.half_x + reach.body_radius,
                                         reach.half_z + reach.body_radius);
  }
}

auto majority_doctrine(const std::vector<ArmyFormationMember>& members,
                       const FormationDoctrineId& requested) -> FormationDoctrineId {
  std::unordered_map<FormationDoctrineId, int> counts;
  for (const auto& member : members) {
    ++counts[member.doctrine];
  }
  FormationDoctrineId best = members.front().doctrine;
  int best_count = 0;
  for (const auto& entry : counts) {
    if (entry.second > best_count ||
        (entry.second == best_count && entry.first < best)) {
      best = entry.first;
      best_count = entry.second;
    }
  }
  if (!requested.empty() && counts.count(requested) != 0U) {
    return requested;
  }
  return best;
}

} // namespace

auto ArmyFormationPlanner::collect_members(Engine::Core::World& world,
                                           const std::vector<EntityID>& entities)
    -> std::vector<ArmyFormationMember> {
  std::vector<ArmyFormationMember> members;
  members.reserve(entities.size());

  for (auto const id : entities) {
    auto* entity = world.get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || transform == nullptr) {
      continue;
    }
    auto troop = Game::Units::spawn_typeToTroopType(unit->spawn_type);
    if (!troop.has_value()) {
      continue;
    }

    ArmyFormationMember member;
    member.entity_id = id;
    member.troop_type = *troop;
    member.roles = TroopRoleRegistry::instance().roles(*troop);
    member.current_position =
        QVector3D(transform->position.x, transform->position.y, transform->position.z);
    member.footprint =
        Game::Units::TroopConfig::instance().get_selection_ring_size(*troop);
    measure_footprint(*entity, member.footprint, member);
    if (const auto* movement =
            entity->get_component<Engine::Core::MovementComponent>()) {
      member.heavy = !movement->get_can_enter_forest();
    }
    member.doctrine = doctrine_for_entity(world, id);
    members.push_back(member);
  }

  return members;
}

auto ArmyFormationPlanner::combined_roles(
    const std::vector<ArmyFormationMember>& members) -> RoleTagSet {
  RoleTagSet set = 0U;
  for (const auto& member : members) {
    set |= member.roles;
  }
  return set;
}

auto ArmyFormationPlanner::resolve_doctrine(
    const std::vector<ArmyFormationMember>& members,
    const ArmyFormationRequest& request) -> FormationDoctrineId {
  if (!request.doctrine.empty() && request.options.doctrine_locked) {
    return request.doctrine;
  }
  if (members.empty()) {
    return request.doctrine.empty() ? k_neutral_doctrine : request.doctrine;
  }

  switch (request.options.mixed_policy) {
  case MixedDoctrinePolicy::CompositeByRole:
    return k_neutral_doctrine;

  case MixedDoctrinePolicy::CommanderDoctrine: {
    for (const auto& member : members) {
      if (has_role(member.roles, RoleTag::Command)) {
        return member.doctrine;
      }
    }
    break;
  }

  case MixedDoctrinePolicy::SeparateContingents:
  case MixedDoctrinePolicy::MajorityDoctrine:
    break;
  }

  return majority_doctrine(members, request.doctrine);
}

auto ArmyFormationPlanner::make_member(EntityID entity_id,
                                       Game::Units::TroopType troop_type,
                                       const QVector3D& position,
                                       const FormationDoctrineId& doctrine)
    -> ArmyFormationMember {
  ArmyFormationMember member;
  member.entity_id = entity_id;
  member.troop_type = troop_type;
  member.roles = TroopRoleRegistry::instance().roles(troop_type);
  member.current_position = position;
  member.footprint =
      Game::Units::TroopConfig::instance().get_selection_ring_size(troop_type);

  member.half_width = member.footprint;
  member.half_depth = member.footprint;
  member.doctrine = doctrine.empty() ? k_neutral_doctrine : doctrine;
  return member;
}

void ArmyFormationPlanner::measure_footprint(const Engine::Core::Entity& entity,
                                             float fallback_radius,
                                             ArmyFormationMember& member) {
  member.half_width = fallback_radius;
  member.half_depth = fallback_radius;
  member.individuals = 1;
  member.files = 1;
  member.soldier_body_radius = fallback_radius;
  member.extents_by_files.clear();

  auto const layout = Game::Systems::FormationCombat::resolve_layout(entity);
  if (layout.all_slots.empty()) {
    return;
  }
  member.individuals = std::max(1, static_cast<int>(layout.all_slots.size()));
  member.soldier_body_radius = std::max(0.05F, layout.body_radius);

  auto const natural =
      Game::Systems::FormationCombat::layout_reach_for_files(entity, 0);
  member.files = std::max(1, natural.files);
  member.half_width = natural.half_x + natural.body_radius;
  member.half_depth = natural.half_z + natural.body_radius;
  member.soldier_file_step =
      member.files > 1 ? (natural.half_x * 2.0F) / static_cast<float>(member.files - 1)
                       : std::max(0.1F, layout.spacing);
  int const rows = std::max(1, (member.individuals + member.files - 1) / member.files);
  member.soldier_rank_step =
      rows > 1 ? (natural.half_z * 2.0F) / static_cast<float>(rows - 1)
               : std::max(0.1F, layout.spacing);
  tabulate_extents_by_files(entity, member);
}

void ArmyFormationPlanner::shape_member_for_intent(ArmyFormationMember& member,
                                                   float aspect) {
  if (aspect <= 0.0F || member.individuals <= 1) {
    return;
  }
  int const files = std::clamp(static_cast<int>(std::lround(std::sqrt(
                                   static_cast<float>(member.individuals) * aspect))),
                               1,
                               member.individuals);
  member.files = files;
  if (static_cast<std::size_t>(files) <= member.extents_by_files.size()) {
    member.half_width =
        member.extents_by_files[static_cast<std::size_t>(files) - 1U].first;
    member.half_depth =
        member.extents_by_files[static_cast<std::size_t>(files) - 1U].second;
    return;
  }
  int const rows = (member.individuals + files - 1) / files;
  member.half_width = static_cast<float>(files - 1) * member.soldier_file_step * 0.5F +
                      member.soldier_body_radius;
  member.half_depth = static_cast<float>(rows - 1) * member.soldier_rank_step * 0.5F +
                      member.soldier_body_radius;
}

} // namespace Game::Formation
