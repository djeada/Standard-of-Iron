#include "formation_casualties.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <limits>

#include "../../core/component.h"
#include "../../core/death_sequence.h"
#include "../../core/world.h"
#include "../formation_combat_geometry.h"
#include "../movement/command_service.h"
#include "death_variant.h"

namespace Game::Systems::Combat {

namespace {

auto ensure_formation_roster(Engine::Core::Entity& target,
                             int total_count,
                             int expected_live_count)
    -> Engine::Core::FormationRosterPresentationComponent* {
  auto* roster = Engine::Core::get_or_add_component<
      Engine::Core::FormationRosterPresentationComponent>(&target);
  if (roster == nullptr) {
    return nullptr;
  }

  int const current_live_count = static_cast<int>(std::count(
      roster->alive.begin(), roster->alive.end(), static_cast<std::uint8_t>(1U)));
  bool const shaped_for_this_unit =
      roster->total_count == total_count &&
      roster->alive.size() == static_cast<std::size_t>(total_count);
  if (shaped_for_this_unit && current_live_count <= expected_live_count) {

    roster->live_count = static_cast<std::uint16_t>(current_live_count);
    return roster;
  }

  roster->total_count = static_cast<std::uint16_t>(total_count);
  roster->live_count = static_cast<std::uint16_t>(expected_live_count);
  roster->alive.assign(static_cast<std::size_t>(total_count), 0U);
  int const first_live = std::max(0, total_count - expected_live_count);
  for (int slot = first_live; slot < total_count; ++slot) {
    roster->alive[static_cast<std::size_t>(slot)] = 1U;
  }
  ++roster->revision;
  return roster;
}

struct CasualtyPlan {
  int individuals_per_unit{0};
  int prev_survivors{0};
  int first_casualty{0};
  int end_casualty{0};
};

[[nodiscard]] auto plan_casualties(const Engine::Core::UnitComponent& unit,
                                   int prev_health,
                                   int new_health) -> std::optional<CasualtyPlan> {
  int const individuals_per_unit =
      FormationCombat::resolve_definition(unit).total_count;
  if (individuals_per_unit <= 1) {
    return std::nullopt;
  }
  int const prev_survivors = Engine::Core::resolve_surviving_individual_count(
      prev_health, unit.max_health, individuals_per_unit);
  int const new_survivors = Engine::Core::resolve_surviving_individual_count(
      new_health, unit.max_health, individuals_per_unit);
  CasualtyPlan plan{.individuals_per_unit = individuals_per_unit,
                    .prev_survivors = prev_survivors,
                    .first_casualty = individuals_per_unit - prev_survivors,
                    .end_casualty = individuals_per_unit - new_survivors};
  if (plan.end_casualty <= plan.first_casualty) {
    return std::nullopt;
  }
  return plan;
}

[[nodiscard]] auto next_casualty_slot(
    const Engine::Core::FormationRosterPresentationComponent* roster,
    std::optional<std::uint16_t>& preferred_slot) -> std::optional<std::uint16_t> {
  if (roster == nullptr) {
    return std::nullopt;
  }
  if (preferred_slot.has_value() && *preferred_slot < roster->alive.size() &&
      roster->alive[*preferred_slot] != 0U) {
    auto const selected = preferred_slot;
    preferred_slot.reset();
    return selected;
  }
  for (std::size_t slot = 0; slot < roster->alive.size(); ++slot) {
    if (roster->alive[slot] != 0U) {
      return static_cast<std::uint16_t>(slot);
    }
  }
  return std::nullopt;
}

void store_casualty_entry(
    Engine::Core::SoldierCasualtyAnimationComponent& casualties,
    const Engine::Core::SoldierCasualtyAnimationComponent::Entry& entry) {
  auto existing = std::find_if(
      casualties.entries.begin(),
      casualties.entries.end(),
      [&entry](const auto& active) { return active.slot_index == entry.slot_index; });
  if (existing != casualties.entries.end()) {
    *existing = entry;
  } else {
    casualties.entries.push_back(entry);
  }
}

} // namespace

auto preferred_formation_hit_slot(Engine::Core::Entity* target,
                                  Engine::Core::Entity* attacker)
    -> std::optional<std::uint16_t> {
  if (target == nullptr || attacker == nullptr) {
    return std::nullopt;
  }
  if (!FormationCombat::has_formation_slots(*target)) {
    return std::nullopt;
  }

  auto const* contact =
      attacker->get_component<Engine::Core::FormationContactComponent>();
  if (contact != nullptr) {
    auto const* pairs = &contact->engagement_pairs;
    auto const front =
        std::find_if(contact->fronts.begin(),
                     contact->fronts.end(),
                     [target](auto const& candidate) {
                       return candidate.outgoing && candidate.in_contact &&
                              candidate.opponent_id == target->get_id() &&
                              !candidate.engagement_pairs.empty();
                     });
    if (front != contact->fronts.end()) {
      pairs = &front->engagement_pairs;
    }
    if (auto const selected = FormationCombat::select_damage_engagement_pair(
            *attacker, target->get_id(), *pairs);
        selected.has_value()) {
      return selected->target_slot;
    }
  }

  auto const layout = FormationCombat::resolve_layout(*target);
  auto const* attacker_transform =
      attacker->get_component<Engine::Core::TransformComponent>();
  if (layout.live_slots.empty()) {
    return std::nullopt;
  }
  if (attacker_transform == nullptr) {
    return layout.live_slots.front().index;
  }
  auto const closest = std::min_element(
      layout.live_slots.begin(),
      layout.live_slots.end(),
      [attacker_transform](auto const& lhs, auto const& rhs) {
        float const lhs_dx = lhs.world_x - attacker_transform->position.x;
        float const lhs_dz = lhs.world_z - attacker_transform->position.z;
        float const rhs_dx = rhs.world_x - attacker_transform->position.x;
        float const rhs_dz = rhs.world_z - attacker_transform->position.z;
        return lhs_dx * lhs_dx + lhs_dz * lhs_dz < rhs_dx * rhs_dx + rhs_dz * rhs_dz;
      });
  return closest->index;
}

void publish_formation_hit(Engine::Core::Entity& target,
                           Engine::Core::EntityID attacker_id,
                           std::optional<std::uint16_t> slot,
                           Engine::Core::HitReactionKind kind,
                           Engine::Core::World* world) {
  if (!slot.has_value()) {
    return;
  }
  auto* hit = Engine::Core::get_or_add_component<
      Engine::Core::FormationHitPresentationComponent>(&target);
  if (hit == nullptr) {
    return;
  }
  hit->attacker_id = attacker_id;
  hit->soldier_slot = *slot;
  hit->duration = Engine::Core::hit_reaction_duration(kind);
  hit->remaining = hit->duration;
  hit->intensity = kind == Engine::Core::HitReactionKind::Stagger ? 1.2F : 0.85F;
  hit->reaction_kind = kind;
  hit->hit_direction_x = 0.0F;
  hit->hit_direction_z = 0.0F;
  if (world != nullptr && attacker_id != 0) {
    auto const* attacker_transform =
        world->try_get<Engine::Core::TransformComponent>(attacker_id);
    auto const* target_transform =
        target.get_component<Engine::Core::TransformComponent>();
    if (attacker_transform != nullptr && target_transform != nullptr) {
      float const dx = target_transform->position.x - attacker_transform->position.x;
      float const dz = target_transform->position.z - attacker_transform->position.z;
      float const dist = std::hypot(dx, dz);
      if (dist > 0.001F) {
        hit->hit_direction_x = dx / dist;
        hit->hit_direction_z = dz / dist;
      }
    }
  }
  ++hit->revision;
}

auto begin_soldier_casualties(Engine::Core::Entity* target,
                              Engine::Core::Entity* attacker,
                              int prev_health,
                              int new_health,
                              std::optional<std::uint16_t> preferred_slot,
                              const FormationCombat::FormationLayout& previous_layout)
    -> int {
  if (target == nullptr) {
    return 0;
  }
  auto* unit = target->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return 0;
  }
  auto const plan = plan_casualties(*unit, prev_health, new_health);
  if (!plan.has_value()) {
    return 0;
  }

  auto* roster = ensure_formation_roster(
      *target, plan->individuals_per_unit, plan->prev_survivors);
  auto const profile = Engine::Core::resolve_death_profile(*target);
  auto* casualties = Engine::Core::get_or_add_component<
      Engine::Core::SoldierCasualtyAnimationComponent>(target);
  if (casualties == nullptr) {
    return 0;
  }

  auto const spatial_anchors =
      FormationCombat::soldier_spatial_anchors(*target, previous_layout);
  int queued_casualties = 0;
  for (int casualty_index = plan->first_casualty; casualty_index < plan->end_casualty;
       ++casualty_index) {
    auto const selected_slot = next_casualty_slot(roster, preferred_slot);
    int const slot =
        selected_slot.has_value() ? static_cast<int>(*selected_slot) : casualty_index;
    Engine::Core::SoldierCasualtyAnimationComponent::Entry entry{};
    entry.slot_index = static_cast<std::uint16_t>(slot);
    auto const anchor = std::find_if(spatial_anchors.begin(),
                                     spatial_anchors.end(),
                                     [&entry](auto const& candidate) {
                                       return candidate.slot_index == entry.slot_index;
                                     });
    if (anchor != spatial_anchors.end()) {
      entry.has_local_anchor = true;
      entry.local_x = anchor->local_x;
      entry.local_z = anchor->local_z;
      entry.local_yaw = anchor->local_yaw;
    }
    auto const variant = resolve_death_variant(
        target, attacker, profile, static_cast<std::uint16_t>(slot));
    entry.profile = profile;
    Engine::Core::apply_death_sequence_timing(
        entry, Engine::Core::resolve_death_timing(profile, variant));
    store_casualty_entry(*casualties, entry);
    if (roster != nullptr && slot >= 0 && slot < plan->individuals_per_unit) {
      roster->alive[static_cast<std::size_t>(slot)] = 0U;
      ++roster->revision;
    }
    ++queued_casualties;
  }
  if (roster != nullptr && queued_casualties > 0) {
    roster->live_count = static_cast<std::uint16_t>(std::count(
        roster->alive.begin(), roster->alive.end(), static_cast<std::uint8_t>(1U)));
  }
  return queued_casualties;
}

void fill_formation_front_vacancy(Engine::Core::World* world,
                                  Engine::Core::Entity* casualty) {
  if (world == nullptr || casualty == nullptr) {
    return;
  }
  auto const* vacant = casualty->get_component<Engine::Core::FormationModeComponent>();
  if (vacant == nullptr || !vacant->active || vacant->formation_id == 0 ||
      vacant->stable_rank < 0 || vacant->stable_file < 0) {
    return;
  }
  Engine::Core::Entity* replacement = nullptr;
  int best_rank = std::numeric_limits<int>::max();
  float best_distance_sq = std::numeric_limits<float>::max();
  for (auto [candidate_ref, mode_ref, unit_ref, transform_ref] :
       world->entity_view<Engine::Core::FormationModeComponent,
                          Engine::Core::UnitComponent,
                          Engine::Core::TransformComponent>()) {
    Engine::Core::Entity* candidate = &candidate_ref;
    const auto* mode = &mode_ref;
    const auto* unit = &unit_ref;
    const auto* transform = &transform_ref;
    if (candidate == casualty || unit->health <= 0 ||
        mode->formation_id != vacant->formation_id ||
        mode->stable_file != vacant->stable_file ||
        mode->stable_rank <= vacant->stable_rank) {
      continue;
    }
    float const dx = transform->position.x - vacant->stable_slot_x;
    float const dz = transform->position.z - vacant->stable_slot_z;
    float const distance_sq = dx * dx + dz * dz;
    if (mode->stable_rank < best_rank ||
        (mode->stable_rank == best_rank && distance_sq < best_distance_sq)) {
      replacement = candidate;
      best_rank = mode->stable_rank;
      best_distance_sq = distance_sq;
    }
  }
  if (replacement == nullptr) {
    return;
  }
  auto* replacement_mode =
      replacement->get_component<Engine::Core::FormationModeComponent>();
  replacement_mode->stable_slot_id = vacant->stable_slot_id;
  replacement_mode->stable_rank = vacant->stable_rank;
  replacement_mode->stable_file = vacant->stable_file;
  replacement_mode->stable_slot_x = vacant->stable_slot_x;
  replacement_mode->stable_slot_z = vacant->stable_slot_z;
  CommandService::move_unit(
      *world,
      replacement->get_id(),
      QVector3D(vacant->stable_slot_x, 0.0F, vacant->stable_slot_z),
      {.kind = MoveOrderKind::FormationMove, .preserve_formation_mode = true});
}

} // namespace Game::Systems::Combat
