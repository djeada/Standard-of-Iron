#include "army_formation_registry.h"

#include <QJsonArray>

#include <algorithm>

#include "../core/ambient_session.h"
#include "army_formation_ambient.h"
#include "army_formation_codec.h"
#include "army_formation_planner.h"

namespace Game::Formation {

namespace {

void vacate_slots(ArmyFormation& formation, EntityID member) {
  for (auto& slot : formation.slot_list) {
    if (slot.occupant == member) {
      slot.occupant = 0U;
    }
  }
}

} // namespace

auto ambient_formation_registry() -> ArmyFormationRegistry& {
  return ArmyFormationRegistry::instance();
}

auto ArmyFormationRegistry::instance() -> ArmyFormationRegistry& {
  return *Game::Session::ambient_services().army_formations;
}

auto ArmyFormationRegistry::for_world(const Engine::Core::World& world)
    -> ArmyFormationRegistry& {
  return *Game::Session::services_for(world).army_formations;
}

void ArmyFormationRegistry::link_member(EntityID member, FormationGroupID group) {
  m_membership[member] = group;
}

void ArmyFormationRegistry::unlink_member(EntityID member, FormationGroupID group) {
  auto membership = m_membership.find(member);
  if (membership != m_membership.end() && membership->second == group) {
    m_membership.erase(membership);
  }
}

void ArmyFormationRegistry::take_from_other_group(EntityID member,
                                                  FormationGroupID new_group) {
  auto existing = m_membership.find(member);
  if (existing == m_membership.end() || existing->second == new_group) {
    return;
  }
  auto group = m_groups.find(existing->second);
  if (group == m_groups.end()) {
    return;
  }
  auto& old = group->second;
  old.members.erase(std::remove(old.members.begin(), old.members.end(), member),
                    old.members.end());
  vacate_slots(old, member);
  old.stragglers.erase(
      std::remove(old.stragglers.begin(), old.stragglers.end(), member),
      old.stragglers.end());
  old.needs_replan = true;
  if (old.members.empty()) {
    m_groups.erase(group);
  }
}

auto ArmyFormationRegistry::create_group(FormationDoctrineId doctrine,
                                         ArmyFormationIntent intent,
                                         std::vector<EntityID> members)
    -> FormationGroupID {
  ArmyFormation formation;
  formation.id = m_next_id++;
  formation.doctrine = std::move(doctrine);
  formation.intent = intent;
  formation.members = std::move(members);
  formation.needs_replan = true;

  for (auto const member : formation.members) {
    take_from_other_group(member, formation.id);
    link_member(member, formation.id);
  }

  auto const id = formation.id;
  m_groups.emplace(id, std::move(formation));
  return id;
}

auto ArmyFormationRegistry::find(FormationGroupID id) -> ArmyFormation* {
  auto it = m_groups.find(id);
  return it == m_groups.end() ? nullptr : &it->second;
}

auto ArmyFormationRegistry::find(FormationGroupID id) const -> const ArmyFormation* {
  auto it = m_groups.find(id);
  return it == m_groups.end() ? nullptr : &it->second;
}

void ArmyFormationRegistry::remove_group(FormationGroupID id) {
  auto it = m_groups.find(id);
  if (it == m_groups.end()) {
    return;
  }
  for (auto const member : it->second.members) {
    unlink_member(member, id);
  }
  m_groups.erase(it);
}

auto ArmyFormationRegistry::replace_members(
    FormationGroupID id, std::vector<EntityID> members) -> std::vector<EntityID> {
  auto* formation = find(id);
  if (formation == nullptr) {
    return {};
  }

  std::vector<EntityID> dropped;
  for (auto const previous : formation->members) {
    if (std::find(members.begin(), members.end(), previous) == members.end()) {
      dropped.push_back(previous);
    }
  }

  for (auto const member : dropped) {
    unlink_member(member, id);
    vacate_slots(*formation, member);
  }

  for (auto const member : members) {
    take_from_other_group(member, id);
    link_member(member, id);
  }

  formation->members = std::move(members);
  formation->needs_replan = true;
  return dropped;
}

auto ArmyFormationRegistry::members_of(FormationGroupID id) const
    -> std::vector<EntityID> {
  const auto* formation = find(id);
  return formation == nullptr ? std::vector<EntityID>{} : formation->members;
}

auto ArmyFormationRegistry::add_member(FormationGroupID id, EntityID entity) -> bool {
  auto* formation = find(id);
  if (formation == nullptr) {
    return false;
  }
  if (formation->has_member(entity)) {
    return false;
  }
  remove_member(entity);
  formation->members.push_back(entity);
  formation->needs_replan = true;
  link_member(entity, id);
  return true;
}

auto ArmyFormationRegistry::remove_member(EntityID entity) -> bool {
  auto membership = m_membership.find(entity);
  if (membership == m_membership.end()) {
    return false;
  }
  auto const group_id = membership->second;
  m_membership.erase(membership);

  auto* formation = find(group_id);
  if (formation == nullptr) {
    return false;
  }
  auto& list = formation->members;
  list.erase(std::remove(list.begin(), list.end(), entity), list.end());
  vacate_slots(*formation, entity);
  formation->needs_replan = true;
  if (list.empty()) {
    m_groups.erase(group_id);
  }
  return true;
}

auto ArmyFormationRegistry::group_of(EntityID entity) const -> FormationGroupID {
  auto it = m_membership.find(entity);
  return it == m_membership.end() ? k_invalid_group : it->second;
}

void ArmyFormationRegistry::apply_plan(FormationGroupID id,
                                       const ArmyFormationPlan& plan) {
  auto* formation = find(id);
  if (formation == nullptr) {
    return;
  }
  formation->doctrine = plan.doctrine;
  formation->intent = plan.intent;
  formation->anchor = plan.anchor;
  formation->facing = plan.facing;
  formation->frontage = plan.frontage;
  formation->depth = plan.depth;
  formation->spacing = plan.spacing;
  formation->slot_spacing = plan.slot_spacing;
  formation->slot_list = plan.slot_list;
  formation->compressed = plan.narrowed;
  if (!plan.narrowed) {
    formation->reference_slots = plan.slot_list;
  }
  formation->needs_replan = false;
  ++formation->plan_revision;
  reindex_membership(*formation);
}

void ArmyFormationRegistry::reindex_membership(const ArmyFormation& formation) {
  for (auto const member : formation.members) {
    link_member(member, formation.id);
  }
}

auto ArmyFormationRegistry::group_ids() const -> std::vector<FormationGroupID> {
  std::vector<FormationGroupID> ids;
  ids.reserve(m_groups.size());
  for (const auto& entry : m_groups) {
    ids.push_back(entry.first);
  }
  std::sort(ids.begin(), ids.end());
  return ids;
}

void ArmyFormationRegistry::clear() {
  m_groups.clear();
  m_membership.clear();
  m_next_id = 1U;
}

auto ArmyFormationRegistry::to_json() const -> QJsonObject {
  QJsonObject root;
  QJsonArray groups;
  for (auto const id : group_ids()) {
    const auto* formation = find(id);
    if (formation == nullptr) {
      continue;
    }
    groups.append(Codec::formation_to_json(*formation));
  }
  root["groups"] = groups;
  root["next_id"] = static_cast<qint64>(m_next_id);
  return root;
}

void ArmyFormationRegistry::from_json(const QJsonObject& root) {
  clear();
  for (const auto value : root["groups"].toArray()) {
    ArmyFormation formation = Codec::formation_from_json(value.toObject());
    if (formation.id == k_invalid_group) {
      continue;
    }
    auto const id = formation.id;
    m_groups.emplace(id, std::move(formation));
    reindex_membership(m_groups.at(id));
  }
  m_next_id = static_cast<FormationGroupID>(root["next_id"].toVariant().toULongLong());
  if (m_next_id == 0U) {
    m_next_id = 1U;
    for (const auto& entry : m_groups) {
      m_next_id = std::max(m_next_id, entry.first + 1U);
    }
  }
}

} // namespace Game::Formation
