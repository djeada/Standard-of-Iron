#include "army_formation_service.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_map>

#include "../core/component_core.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "army_formation_registry.h"
#include "formation_doctrine.h"

namespace Game::Formation {

namespace {

constexpr float k_rad_to_deg = 180.0F / std::numbers::pi_v<float>;

auto rank_and_file(const std::vector<FormationSlot>& slot_list,
                   float spacing) -> std::pair<std::vector<int>, std::vector<int>> {
  std::vector<int> ranks(slot_list.size(), 0);
  std::vector<int> files(slot_list.size(), 0);
  if (slot_list.empty()) {
    return {ranks, files};
  }

  std::vector<std::size_t> order(slot_list.size());
  for (std::size_t i = 0; i < order.size(); ++i) {
    order[i] = i;
  }
  std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
    if (slot_list[a].local_offset.z() != slot_list[b].local_offset.z()) {
      return slot_list[a].local_offset.z() > slot_list[b].local_offset.z();
    }
    return slot_list[a].local_offset.x() < slot_list[b].local_offset.x();
  });

  int rank = -1;
  float previous_depth = 0.0F;
  std::vector<std::vector<std::size_t>> rank_members;
  for (std::size_t position = 0; position < order.size(); ++position) {
    auto const index = order[position];
    float const depth = slot_list[index].local_offset.z();
    if (position == 0 || std::fabs(depth - previous_depth) > spacing * 0.35F) {
      ++rank;
      previous_depth = depth;
      rank_members.emplace_back();
    }
    ranks[index] = rank;
    rank_members.back().push_back(index);
  }

  for (auto& members : rank_members) {
    std::stable_sort(members.begin(), members.end(), [&](std::size_t a, std::size_t b) {
      return slot_list[a].local_offset.x() < slot_list[b].local_offset.x();
    });
    for (std::size_t file = 0; file < members.size(); ++file) {
      files[members[file]] = static_cast<int>(file);
    }
  }

  return {ranks, files};
}

} // namespace

auto ArmyFormationService::spread(int count,
                                  const QVector3D& centre,
                                  float spacing) -> std::vector<QVector3D> {
  std::vector<QVector3D> out;
  if (count <= 0) {
    return out;
  }
  out.reserve(static_cast<std::size_t>(count));
  int const side = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
  for (int i = 0; i < count; ++i) {
    int const gx = i % side;
    int const gy = i / side;
    float const ox =
        (static_cast<float>(gx) - static_cast<float>(side - 1) * 0.5F) * spacing;
    float const oz =
        (static_cast<float>(gy) - static_cast<float>(side - 1) * 0.5F) * spacing;
    out.emplace_back(centre.x() + ox, centre.y(), centre.z() + oz);
  }
  return out;
}

auto ArmyFormationService::auto_facing(Engine::Core::World& world,
                                       const std::vector<EntityID>& members,
                                       const QVector3D& anchor) -> float {
  QVector3D sum(0.0F, 0.0F, 0.0F);
  int count = 0;
  for (auto const id : members) {
    auto* entity = world.get_entity(id);
    if (entity == nullptr) {
      continue;
    }
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform == nullptr) {
      continue;
    }
    sum +=
        QVector3D(transform->position.x, transform->position.y, transform->position.z);
    ++count;
  }
  if (count == 0) {
    return 0.0F;
  }
  QVector3D forward = anchor - (sum / static_cast<float>(count));
  forward.setY(0.0F);
  if (forward.lengthSquared() <= 1.0e-4F) {
    return 0.0F;
  }
  forward.normalize();
  return std::atan2(forward.x(), forward.z()) * k_rad_to_deg;
}

auto ArmyFormationService::doctrine_for_selection(Engine::Core::World& world,
                                                  const std::vector<EntityID>& members,
                                                  MixedDoctrinePolicy policy)
    -> FormationDoctrineId {
  ArmyFormationRequest request;
  request.members = members;
  request.options.mixed_policy = policy;
  auto const collected = ArmyFormationPlanner::collect_members(world, members);
  return ArmyFormationPlanner::resolve_doctrine(collected, request);
}

auto ArmyFormationService::availability(Engine::Core::World& world,
                                        const std::vector<EntityID>& members,
                                        ArmyFormationIntent intent,
                                        const FormationDoctrineId& doctrine)
    -> std::string {
  auto const collected = ArmyFormationPlanner::collect_members(world, members);
  if (collected.empty()) {
    return QCoreApplication::translate("Formation", "No units selected.").toStdString();
  }
  ArmyFormationRequest request;
  request.members = members;
  request.doctrine = doctrine;
  auto const resolved = doctrine.empty()
                            ? ArmyFormationPlanner::resolve_doctrine(collected, request)
                            : doctrine;
  return DoctrineRegistry::instance().availability_reason(
      resolved,
      intent,
      ArmyFormationPlanner::combined_roles(collected),
      static_cast<int>(collected.size()));
}

auto ArmyFormationService::positions_for(
    const std::vector<ArmyFormationMember>& members,
    const ArmyFormationRequest& request) -> std::vector<QVector3D> {
  std::vector<QVector3D> positions;
  positions.assign(members.size(), request.anchor);
  if (members.empty()) {
    return positions;
  }

  auto const plan = ArmyFormationPlanner::plan(members, request);
  if (!plan.valid) {
    return spread(static_cast<int>(members.size()),
                  request.anchor,
                  std::max(0.5F, request.spacing));
  }

  std::unordered_map<EntityID, std::size_t> index_of;
  index_of.reserve(members.size());
  for (std::size_t i = 0; i < members.size(); ++i) {
    index_of.emplace(members[i].entity_id, i);
  }
  for (const auto& slot : plan.slot_list) {
    auto it = index_of.find(slot.occupant);
    if (it != index_of.end()) {
      positions[it->second] = slot.world_position;
    }
  }
  return positions;
}

auto ArmyFormationService::preview(Engine::Core::World& world,
                                   const ArmyFormationRequest& request)
    -> ArmyFormationResult {
  return build(world, request, false);
}

auto ArmyFormationService::commit(Engine::Core::World& world,
                                  const ArmyFormationRequest& request)
    -> ArmyFormationResult {
  return build(world, request, true);
}

auto ArmyFormationService::build(Engine::Core::World& world,
                                 const ArmyFormationRequest& request,
                                 bool commit_group) -> ArmyFormationResult {
  ArmyFormationResult result;
  auto const member_count = request.members.size();
  result.positions.assign(member_count, request.anchor);
  result.facing_angles.assign(member_count, 0.0F);
  result.stable_slot_ids.assign(member_count, k_invalid_slot);
  result.stable_ranks.assign(member_count, -1);
  result.stable_files.assign(member_count, -1);
  result.slot_status.assign(member_count, SlotStatus::Blocked);

  if (member_count == 0) {
    result.rejection_reason =
        QCoreApplication::translate("Formation", "No units selected.").toStdString();
    return result;
  }

  ArmyFormationRequest effective = request;
  if (effective.group_id == k_invalid_group) {
    effective.group_id =
        ArmyFormationRegistry::for_world(world).group_of(request.members.front());
  }

  auto const plan = ArmyFormationPlanner::plan(world, effective);
  result.doctrine = plan.doctrine;
  result.intent = plan.intent;
  result.formation_facing = plan.facing;
  result.frontage = plan.frontage;
  result.depth = plan.depth;
  result.blocked_count = plan.blocked_count;

  if (!plan.valid) {
    result.rejection_reason = plan.rejection_reason;
    result.positions = spread(static_cast<int>(member_count),
                              request.anchor,
                              std::max(0.5F, request.spacing));
    return result;
  }

  std::unordered_map<EntityID, std::size_t> index_of;
  index_of.reserve(member_count);
  for (std::size_t i = 0; i < member_count; ++i) {
    index_of.emplace(request.members[i], i);
  }

  auto const [ranks, files] = rank_and_file(plan.slot_list, plan.spacing);

  for (std::size_t slot_index = 0; slot_index < plan.slot_list.size(); ++slot_index) {
    const auto& slot = plan.slot_list[slot_index];
    auto it = index_of.find(slot.occupant);
    if (it == index_of.end()) {
      continue;
    }
    auto const target = it->second;
    result.positions[target] = slot.world_position;
    result.facing_angles[target] = slot.facing;
    result.stable_slot_ids[target] = slot.id;
    result.stable_ranks[target] = ranks[slot_index];
    result.stable_files[target] = files[slot_index];
    result.slot_status[target] = slot.status;
  }

  result.valid = true;
  result.used_army_formation = true;

  if (!commit_group || member_count < 2) {
    return result;
  }

  auto& registry = ArmyFormationRegistry::for_world(world);
  FormationGroupID group_id = effective.group_id;
  auto* existing = registry.find(group_id);
  if (existing == nullptr) {
    group_id = registry.create_group(plan.doctrine, plan.intent, request.members);
  } else {
    existing->members = request.members;
    existing->doctrine = plan.doctrine;
    existing->intent = plan.intent;
  }

  auto* formation = registry.find(group_id);
  if (formation != nullptr) {
    formation->options = request.options;
  }
  registry.apply_plan(group_id, plan);

  const auto* committed = registry.find(group_id);
  if (committed != nullptr) {
    ArmyFormationRuntime::sync_membership_components(world, *committed);
  }
  ArmyFormationRuntime::begin_move(world, group_id, plan.anchor, plan.facing);
  result.group_id = group_id;
  return result;
}

void ArmyFormationService::release(Engine::Core::World& world,
                                   const std::vector<EntityID>& members) {
  for (auto const id : members) {
    ArmyFormationRuntime::detach(world, id);
  }
}

} // namespace Game::Formation
