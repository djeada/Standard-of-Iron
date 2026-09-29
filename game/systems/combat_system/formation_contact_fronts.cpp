#include "formation_contact_fronts.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../formation_combat_geometry.h"
#include "combat_utils.h"
#include "structure_combat.h"

namespace Game::Systems::Combat {
namespace {

using FrontMap = std::unordered_map<Engine::Core::EntityID,
                                    std::vector<Engine::Core::FormationContactFront>>;

struct PairEvaluation {
  std::uint64_t signature{0};
  FormationCombat::ContactGeometry geometry;
  bool in_contact{false};
  std::vector<Engine::Core::FormationEngagementPair> outgoing_pairs;
  std::vector<Engine::Core::FormationEngagementPair> incoming_pairs;
};

thread_local std::unordered_map<std::uint64_t, PairEvaluation> g_pair_cache;

void signature_combine(std::uint64_t& seed, std::uint64_t value) {
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
}

auto pair_signature(const Engine::Core::Entity& attacker,
                    const Engine::Core::Entity& target) -> std::uint64_t {
  std::uint64_t signature = 0xcbf29ce484222325ULL;
  for (auto const* entity : {&attacker, &target}) {
    auto const* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform != nullptr) {
      signature_combine(signature, std::bit_cast<std::uint32_t>(transform->position.x));
      signature_combine(signature, std::bit_cast<std::uint32_t>(transform->position.z));
      signature_combine(signature, std::bit_cast<std::uint32_t>(transform->rotation.y));
      signature_combine(signature, std::bit_cast<std::uint32_t>(transform->scale.x));
      signature_combine(signature, std::bit_cast<std::uint32_t>(transform->scale.z));
    }
    auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr) {
      signature_combine(signature, static_cast<std::uint64_t>(unit->health));
      signature_combine(signature, static_cast<std::uint64_t>(unit->max_health));
      signature_combine(signature, static_cast<std::uint64_t>(unit->spawn_type));
      signature_combine(signature, static_cast<std::uint64_t>(unit->nation_id));
      signature_combine(
          signature,
          static_cast<std::uint64_t>(unit->render_individuals_per_unit_override));
      signature_combine(signature, static_cast<std::uint64_t>(unit->squad_strength));
    }
    auto const* contact =
        entity->get_component<Engine::Core::FormationContactComponent>();
    signature_combine(signature, contact != nullptr && contact->in_contact ? 1U : 0U);
    auto const* attack = entity->get_component<Engine::Core::AttackComponent>();
    signature_combine(signature, attack != nullptr && attack->in_melee_lock ? 1U : 0U);
    if (auto const* roster =
            entity
                ->get_component<Engine::Core::FormationRosterPresentationComponent>()) {
      signature_combine(signature, roster->revision);
      for (std::uint8_t const alive : roster->alive) {
        signature_combine(signature, alive);
      }
    }
    if (auto const* casualties =
            entity->get_component<Engine::Core::SoldierCasualtyAnimationComponent>()) {
      for (auto const& casualty : casualties->entries) {
        signature_combine(signature, casualty.slot_index);
        signature_combine(signature, casualty.has_local_anchor ? 1U : 0U);
        signature_combine(signature, std::bit_cast<std::uint32_t>(casualty.local_x));
        signature_combine(signature, std::bit_cast<std::uint32_t>(casualty.local_z));
      }
    }
  }
  return signature;
}

auto pair_cache_key(Engine::Core::EntityID attacker,
                    Engine::Core::EntityID target) -> std::uint64_t {
  std::uint64_t key = attacker;
  signature_combine(key, target);
  return key;
}
auto broad_phase_geometry(const Engine::Core::Entity& attacker,
                          const Engine::Core::Entity& target)
    -> std::optional<FormationCombat::ContactGeometry> {
  auto const* attacker_transform =
      attacker.get_component<Engine::Core::TransformComponent>();
  auto const* target_transform =
      target.get_component<Engine::Core::TransformComponent>();
  if (attacker_transform == nullptr || target_transform == nullptr) {
    return std::nullopt;
  }
  auto const* attacker_contact =
      attacker.get_component<Engine::Core::FormationContactComponent>();
  auto const* target_contact =
      target.get_component<Engine::Core::FormationContactComponent>();
  if ((attacker_contact != nullptr && attacker_contact->in_contact) ||
      (target_contact != nullptr && target_contact->in_contact)) {
    return std::nullopt;
  }

  float const dx = target_transform->position.x - attacker_transform->position.x;
  float const dz = target_transform->position.z - attacker_transform->position.z;
  float const center_distance = std::hypot(dx, dz);
  auto extent = [](const Engine::Core::Entity& entity,
                   const Engine::Core::TransformComponent& transform) {
    float const body =
        std::max(std::abs(transform.scale.x), std::abs(transform.scale.z)) *
        (entity.has_component<Engine::Core::ElephantComponent>() ? 1.2F : 0.5F);
    return FormationCombat::formation_turn_radius(entity) + std::max(0.05F, body);
  };
  float const attacker_extent = extent(attacker, *attacker_transform);
  float const target_extent = extent(target, *target_transform);
  auto const* attack = attacker.get_component<Engine::Core::AttackComponent>();
  float const melee_reach = attack != nullptr ? attack->melee_range : 1.5F;
  constexpr float k_detailed_contact_margin = 2.0F;
  if (center_distance <=
      attacker_extent + target_extent + melee_reach + k_detailed_contact_margin) {
    return std::nullopt;
  }

  FormationCombat::ContactGeometry geometry;
  geometry.center_distance = center_distance;
  geometry.surface_gap = center_distance - attacker_extent - target_extent;
  geometry.contact_center_distance = attacker_extent + target_extent;
  geometry.engagement_center_distance = 0.0F;
  geometry.uses_formation_slots = true;
  geometry.formation_overlap_required =
      FormationCombat::has_formation_slots(attacker) &&
      FormationCombat::has_formation_slots(target);
  return geometry;
}

auto evaluate_pair(Engine::Core::Entity& attacker,
                   Engine::Core::Entity& target) -> const PairEvaluation& {
  std::uint64_t const key = pair_cache_key(attacker.get_id(), target.get_id());
  std::uint64_t const signature = pair_signature(attacker, target);
  auto cached = g_pair_cache.find(key);
  if (cached != g_pair_cache.end() && cached->second.signature == signature) {
    return cached->second;
  }
  if (g_pair_cache.size() > 4096U) {
    g_pair_cache.clear();
  }

  PairEvaluation evaluation;
  evaluation.signature = signature;
  if (auto const broad_phase = broad_phase_geometry(attacker, target)) {
    evaluation.geometry = *broad_phase;
    return g_pair_cache.insert_or_assign(key, std::move(evaluation)).first->second;
  }

  auto context = FormationCombat::resolve_contact_context(attacker, target);
  evaluation.geometry = context.geometry;
  evaluation.in_contact =
      FormationCombat::contact_is_active(attacker, target, evaluation.geometry);
  if (evaluation.in_contact) {
    evaluation.outgoing_pairs = FormationCombat::engagement_pairs(
        attacker, target, context.attacker_layout, context.target_layout);
    evaluation.incoming_pairs = FormationCombat::engagement_pairs(
        target, attacker, context.target_layout, context.attacker_layout);
  }
  return g_pair_cache.insert_or_assign(key, std::move(evaluation)).first->second;
}

auto valid_melee_edge(Engine::Core::World& world,
                      Engine::Core::Entity& attacker,
                      Engine::Core::Entity*& target) -> bool {
  auto const* attack = attacker.get_component<Engine::Core::AttackComponent>();
  auto const* target_ref =
      attacker.get_component<Engine::Core::AttackTargetComponent>();
  if (!is_melee_mode(attack) || target_ref == nullptr || target_ref->target_id == 0) {
    return false;
  }

  target = world.get_entity(target_ref->target_id);
  auto const* attacker_unit = attacker.get_component<Engine::Core::UnitComponent>();
  auto const* target_unit = target != nullptr
                                ? target->get_component<Engine::Core::UnitComponent>()
                                : nullptr;
  return target != nullptr && attacker_unit != nullptr && target_unit != nullptr &&
         attacker_unit->health > 0 && target_unit->health > 0 &&
         !target->has_component<Engine::Core::PendingRemovalComponent>() &&
         (FormationCombat::has_formation_slots(attacker) ||
          FormationCombat::has_formation_slots(*target)) &&
         !structure_separates_combatants(&attacker, target);
}

void sort_fronts(std::vector<Engine::Core::FormationContactFront>& fronts) {
  std::sort(fronts.begin(), fronts.end(), [](auto const& lhs, auto const& rhs) {
    if (lhs.outgoing != rhs.outgoing) {
      return lhs.outgoing > rhs.outgoing;
    }
    return lhs.opponent_id < rhs.opponent_id;
  });
}

auto build_fronts(Engine::Core::World& world) -> FrontMap {
  FrontMap result;
  const auto attacker_span = world.entities_with<Engine::Core::AttackTargetComponent>();
  std::vector<Engine::Core::EntityID> attackers(attacker_span.begin(),
                                                attacker_span.end());
  std::sort(attackers.begin(), attackers.end());

  for (const Engine::Core::EntityID attacker_id : attackers) {
    Engine::Core::Entity* attacker = world.get_entity(attacker_id);
    if (attacker == nullptr) {
      continue;
    }
    Engine::Core::Entity* target = nullptr;
    if (!valid_melee_edge(world, *attacker, target)) {
      continue;
    }

    auto const& evaluation = evaluate_pair(*attacker, *target);
    auto const& geometry = evaluation.geometry;
    bool const in_contact = evaluation.in_contact;

    result[attacker->get_id()].push_back(
        {.opponent_id = target->get_id(),
         .surface_gap = geometry.surface_gap,
         .in_contact = in_contact,
         .outgoing = true,
         .engagement_pairs = evaluation.outgoing_pairs});
    result[target->get_id()].push_back({.opponent_id = attacker->get_id(),
                                        .surface_gap = geometry.surface_gap,
                                        .in_contact = in_contact,
                                        .outgoing = false,
                                        .engagement_pairs = evaluation.incoming_pairs});
  }

  for (auto& [_, fronts] : result) {
    sort_fronts(fronts);
  }
  return result;
}

void clear_contact(Engine::Core::FormationContactComponent& contact) {
  if (contact.target_id == 0 && !contact.in_contact &&
      contact.engaged_soldier_indices.empty() && contact.engagement_pairs.empty() &&
      contact.fronts.empty()) {
    return;
  }
  contact.target_id = 0;
  contact.surface_gap = 0.0F;
  contact.in_contact = false;
  contact.engaged_soldier_indices.clear();
  contact.engagement_pairs.clear();
  contact.fronts.clear();
  ++contact.revision;
}

void publish_contacts(Engine::Core::World& world, FrontMap fronts_by_entity) {
  for (auto [entity_id, contact] :
       world.view<Engine::Core::FormationContactComponent>()) {
    if (!fronts_by_entity.contains(entity_id)) {
      clear_contact(contact);
    }
  }

  for (auto& [entity_id, fronts] : fronts_by_entity) {
    auto* entity = world.get_entity(entity_id);
    if (entity == nullptr) {
      continue;
    }
    auto* contact =
        Engine::Core::get_or_add_component<Engine::Core::FormationContactComponent>(
            entity);
    if (contact == nullptr) {
      continue;
    }

    auto const* target_ref =
        world.try_get<Engine::Core::AttackTargetComponent>(entity->get_id());
    Engine::Core::EntityID const outgoing_target =
        target_ref != nullptr ? target_ref->target_id : 0U;
    auto const primary = std::find_if(
        fronts.begin(), fronts.end(), [outgoing_target](auto const& front) {
          return front.outgoing && front.opponent_id == outgoing_target;
        });

    Engine::Core::EntityID const next_target =
        primary != fronts.end() ? primary->opponent_id : 0U;
    float const next_gap = primary != fronts.end() ? primary->surface_gap : 0.0F;
    bool const next_in_contact = primary != fronts.end() && primary->in_contact;
    std::vector<Engine::Core::FormationEngagementPair> next_pairs =
        primary != fronts.end() ? primary->engagement_pairs
                                : std::vector<Engine::Core::FormationEngagementPair>{};
    std::vector<std::uint16_t> next_engaged;
    next_engaged.reserve(next_pairs.size());
    for (auto const& pair : next_pairs) {
      next_engaged.push_back(pair.attacker_slot);
    }

    bool const changed =
        contact->target_id != next_target || contact->surface_gap != next_gap ||
        contact->in_contact != next_in_contact ||
        contact->engaged_soldier_indices != next_engaged ||
        contact->engagement_pairs != next_pairs || contact->fronts != fronts;
    contact->target_id = next_target;
    contact->surface_gap = next_gap;
    contact->in_contact = next_in_contact;
    contact->engaged_soldier_indices = std::move(next_engaged);
    contact->engagement_pairs = std::move(next_pairs);
    contact->fronts = std::move(fronts);
    if (changed) {
      ++contact->revision;
    }
  }
}

} // namespace

void publish_formation_contacts(Engine::Core::World& world) {
  publish_contacts(world, build_fronts(world));
}

} // namespace Game::Systems::Combat
