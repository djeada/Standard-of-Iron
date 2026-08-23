#include "formation_contact_processor.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
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

constexpr float k_target_switch_hysteresis = 0.35F;

constexpr float k_max_reposition_speed = 1.8F;
constexpr float k_contact_yaw_hold_seconds = 0.6F;
constexpr float k_disengage_turn_degrees = 120.0F;
constexpr float k_tight_file_spacing_scale = 0.72F;
constexpr float k_tight_rank_spacing_scale = 0.82F;
constexpr float k_tight_body_spacing_scale = 1.08F;

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

auto mix_hash(std::uint32_t value) noexcept -> std::uint32_t {
  value ^= value >> 16U;
  value *= 0x7feb352dU;
  value ^= value >> 15U;
  value *= 0x846ca68bU;
  value ^= value >> 16U;
  return value;
}

auto hash_unit_float(std::uint32_t seed, std::uint32_t salt) noexcept -> float {
  return static_cast<float>(mix_hash(seed ^ salt) & 0x00ffffffU) /
         static_cast<float>(0x01000000U);
}

auto valid_melee_edge(Engine::Core::World& world,
                      Engine::Core::Entity& attacker,
                      Engine::Core::Entity*& target) -> bool {
  auto const* attack = attacker.get_component<Engine::Core::AttackComponent>();
  auto const* target_ref =
      attacker.get_component<Engine::Core::AttackTargetComponent>();
  if (attack == nullptr || target_ref == nullptr || target_ref->target_id == 0 ||
      attack->current_mode != Engine::Core::AttackComponent::CombatMode::Melee) {
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
        entity->get_component<Engine::Core::AttackTargetComponent>();
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

auto find_live_slot(const FormationCombat::FormationLayout& layout,
                    std::uint16_t stable_index) -> const FormationCombat::SoldierSlot* {
  auto const found = std::lower_bound(
      layout.live_slots.begin(),
      layout.live_slots.end(),
      stable_index,
      [](auto const& slot, std::uint16_t index) { return slot.index < index; });
  return found != layout.live_slots.end() && found->index == stable_index ? &*found
                                                                          : nullptr;
}

auto opponent_alive(Engine::Core::World& world,
                    Engine::Core::EntityID opponent_id) -> bool {
  auto* opponent = world.get_entity(opponent_id);
  auto const* unit = opponent != nullptr
                         ? opponent->get_component<Engine::Core::UnitComponent>()
                         : nullptr;
  return opponent != nullptr && unit != nullptr && unit->health > 0 &&
         !opponent->has_component<Engine::Core::PendingRemovalComponent>();
}

auto combat_role_for(std::uint32_t formation_seed,
                     std::uint16_t stable_slot,
                     bool engaged) -> Engine::Core::FormationSoldierCombatRole {
  if (!engaged) {
    return Engine::Core::FormationSoldierCombatRole::Ready;
  }
  std::uint32_t const choice =
      mix_hash(formation_seed ^
               (static_cast<std::uint32_t>(stable_slot) * 0x9e3779b9U)) %
      100U;
  if (choice < 26U) {
    return Engine::Core::FormationSoldierCombatRole::LeadStrike;
  }
  if (choice < 48U) {
    return Engine::Core::FormationSoldierCombatRole::SupportStrike;
  }
  if (choice < 66U) {
    return Engine::Core::FormationSoldierCombatRole::Guard;
  }
  if (choice < 79U) {
    return Engine::Core::FormationSoldierCombatRole::StepIn;
  }
  if (choice < 90U) {
    return Engine::Core::FormationSoldierCombatRole::StepOut;
  }
  return Engine::Core::FormationSoldierCombatRole::Ready;
}

auto action_for_role(Engine::Core::FormationSoldierCombatRole role)
    -> Engine::Core::FormationSoldierAction {
  using Action = Engine::Core::FormationSoldierAction;
  using Role = Engine::Core::FormationSoldierCombatRole;
  switch (role) {
  case Role::LeadStrike:
  case Role::SupportStrike:
    return Action::MeleeEngaged;
  case Role::Guard:
    return Action::MeleeGuard;
  case Role::StepIn:
  case Role::StepOut:
    return Action::MeleeReposition;
  case Role::Ready:
    return Action::MeleeReady;
  case Role::None:
    return Action::FollowUnit;
  }
  return Action::FollowUnit;
}

struct SoldierAssignment {
  const Engine::Core::FormationContactFront* front{nullptr};
  const Engine::Core::FormationEngagementPair* pair{nullptr};
};

auto assignment_for_slot(const Engine::Core::FormationContactComponent* contact,
                         const Engine::Core::FormationSoldierPresentation* previous,
                         std::uint16_t stable_slot) -> SoldierAssignment {
  if (contact == nullptr) {
    return {};
  }

  SoldierAssignment best;
  float best_distance = std::numeric_limits<float>::infinity();
  for (auto const& front : contact->fronts) {
    if (!front.in_contact) {
      continue;
    }
    auto const pair = std::lower_bound(front.engagement_pairs.begin(),
                                       front.engagement_pairs.end(),
                                       stable_slot,
                                       [](auto const& candidate, std::uint16_t slot) {
                                         return candidate.attacker_slot < slot;
                                       });
    if (pair == front.engagement_pairs.end() || pair->attacker_slot != stable_slot) {
      continue;
    }
    if (previous != nullptr && previous->opponent_id == front.opponent_id) {
      return {&front, &*pair};
    }
    if (pair->root_distance < best_distance) {
      best = {&front, &*pair};
      best_distance = pair->root_distance;
    }
  }
  return best;
}

struct RetainedTarget {
  std::uint16_t slot{0};
  float root_distance{0.0F};
};

auto retained_target_slot(const FormationCombat::FormationLayout& opponent_layout,
                          const FormationCombat::SoldierSlot* attacker_slot,
                          const Engine::Core::FormationSoldierPresentation* previous,
                          const SoldierAssignment& assignment,
                          float spacing) -> RetainedTarget {
  RetainedTarget result{assignment.pair->target_slot, assignment.pair->root_distance};
  if (previous == nullptr || attacker_slot == nullptr ||
      previous->opponent_id != assignment.front->opponent_id ||
      previous->target_slot == result.slot) {
    return result;
  }

  auto const* held = find_live_slot(opponent_layout, previous->target_slot);
  if (held == nullptr) {
    return result;
  }
  float const held_distance = std::hypot(held->world_x - attacker_slot->world_x,
                                         held->world_z - attacker_slot->world_z);
  if (held_distance <= result.root_distance + spacing * k_target_switch_hysteresis) {
    return {previous->target_slot, held_distance};
  }
  return result;
}

struct LocalContactVector {
  float x{0.0F};
  float z{0.0F};
  float distance{0.0F};
  float yaw{0.0F};
};

auto local_to_world(const Engine::Core::TransformComponent& actor,
                    float local_x,
                    float local_z) -> QVector3D {
  float const yaw = actor.rotation.y * std::numbers::pi_v<float> / 180.0F;
  float const sin_yaw = std::sin(yaw);
  float const cos_yaw = std::cos(yaw);
  return {actor.position.x + cos_yaw * local_x + sin_yaw * local_z,
          actor.position.y,
          actor.position.z - sin_yaw * local_x + cos_yaw * local_z};
}

auto local_contact_vector(const Engine::Core::TransformComponent& actor,
                          float source_local_x,
                          float source_local_z,
                          float target_world_x,
                          float target_world_z) -> LocalContactVector {
  float const actor_yaw = actor.rotation.y * std::numbers::pi_v<float> / 180.0F;
  float const sin_yaw = std::sin(actor_yaw);
  float const cos_yaw = std::cos(actor_yaw);
  float const source_world_x =
      actor.position.x + cos_yaw * source_local_x + sin_yaw * source_local_z;
  float const source_world_z =
      actor.position.z - sin_yaw * source_local_x + cos_yaw * source_local_z;
  float const world_x = target_world_x - source_world_x;
  float const world_z = target_world_z - source_world_z;

  LocalContactVector result;
  result.x = cos_yaw * world_x - sin_yaw * world_z;
  result.z = sin_yaw * world_x + cos_yaw * world_z;
  result.distance = std::hypot(result.x, result.z);
  if (result.distance > 0.0001F) {
    result.yaw = std::atan2(result.x, result.z) * 180.0F / std::numbers::pi_v<float>;
  }
  return result;
}

void tick_formation_hit(Engine::Core::Entity& entity, float delta_time) {
  auto* hit = entity.get_component<Engine::Core::FormationHitPresentationComponent>();
  if (hit == nullptr) {
    return;
  }
  hit->remaining = std::max(0.0F, hit->remaining - std::max(0.0F, delta_time));
  if (hit->remaining <= 0.0F) {
    entity.remove_component<Engine::Core::FormationHitPresentationComponent>();
  }
}

void publish_formation_presentation(Engine::Core::World& world, float delta_time) {
  std::unordered_map<Engine::Core::EntityID, FormationCombat::FormationLayout>
      layout_cache;
  layout_cache.reserve(world.entities_with<Engine::Core::UnitComponent>().size());
  auto layout_for =
      [&layout_cache](
          Engine::Core::Entity& entity) -> const FormationCombat::FormationLayout& {
    auto const [entry, inserted] = layout_cache.try_emplace(entity.get_id());
    if (inserted) {
      entry->second = FormationCombat::resolve_layout(entity);
    }
    return entry->second;
  };

  for (auto [entity_ref, entity_unit] :
       world.entity_view<Engine::Core::UnitComponent>()) {
    (void)entity_unit;
    Engine::Core::Entity* entity = &entity_ref;
    if (!FormationCombat::has_formation_slots(*entity)) {
      continue;
    }
    tick_formation_hit(*entity, delta_time);

    auto const& layout = layout_for(*entity);
    auto* presentation = Engine::Core::get_or_add_component<
        Engine::Core::FormationPresentationComponent>(entity);
    if (presentation == nullptr) {
      continue;
    }

    auto const* attack = entity->get_component<Engine::Core::AttackComponent>();
    auto const* target_ref =
        entity->get_component<Engine::Core::AttackTargetComponent>();
    auto const* contact =
        entity->get_component<Engine::Core::FormationContactComponent>();
    auto const* motion =
        entity->get_component<Engine::Core::MotionPresentationComponent>();
    float const traversal_lateral_scale =
        motion != nullptr && motion->traversal_squeeze_active
            ? std::clamp(motion->traversal_lateral_scale, 0.1F, 1.0F)
            : 1.0F;
    bool traversal_reflows = false;
    int traversal_files = 1;
    int traversal_rows = 1;
    float traversal_file_spacing = layout.spacing;
    float traversal_rank_spacing = layout.spacing;
    float traversal_min_lateral_scale = k_tight_file_spacing_scale;
    bool const tight_corridor_active =
        motion != nullptr && motion->traversal_target_lateral_scale < 0.999F;
    if (tight_corridor_active && layout.live_slots.size() > 1U) {
      int normal_files = 1;
      for (auto const& slot : layout.live_slots) {
        int const files_in_rank = static_cast<int>(std::count_if(
            layout.live_slots.begin(),
            layout.live_slots.end(),
            [&slot](auto const& candidate) { return candidate.row == slot.row; }));
        normal_files = std::max(normal_files, files_in_rank);
      }

      float const body_diameter = layout.body_radius * 2.0F;
      float minimum_authored_file_spacing = std::numeric_limits<float>::infinity();
      for (std::size_t first = 0; first < layout.live_slots.size(); ++first) {
        for (std::size_t second = first + 1; second < layout.live_slots.size();
             ++second) {
          auto const& left = layout.live_slots[first];
          auto const& right = layout.live_slots[second];
          if (left.row != right.row) {
            continue;
          }
          float const separation = std::abs(left.local_x - right.local_x);
          if (separation > 0.01F) {
            minimum_authored_file_spacing =
                std::min(minimum_authored_file_spacing, separation);
          }
        }
      }
      if (std::isfinite(minimum_authored_file_spacing)) {
        traversal_min_lateral_scale = std::clamp(
            body_diameter * k_tight_body_spacing_scale / minimum_authored_file_spacing,
            k_tight_file_spacing_scale,
            1.0F);
      }
      traversal_file_spacing = std::max(body_diameter * k_tight_body_spacing_scale,
                                        layout.spacing * k_tight_file_spacing_scale);
      traversal_rank_spacing = std::max(body_diameter * k_tight_body_spacing_scale,
                                        layout.spacing * k_tight_rank_spacing_scale);
      float const available_center_span =
          std::max(0.0F, motion->traversal_available_half_width * 2.0F - body_diameter);
      int const files_that_fit =
          1 + static_cast<int>(std::floor(available_center_span /
                                          std::max(0.05F, traversal_file_spacing)));
      traversal_files = std::clamp(
          files_that_fit,
          1,
          std::min(normal_files, static_cast<int>(layout.live_slots.size())));
      traversal_reflows = traversal_files < normal_files;
      traversal_rows =
          (static_cast<int>(layout.live_slots.size()) + traversal_files - 1) /
          traversal_files;
    }
    Engine::Core::EntityID const outgoing_target =
        target_ref != nullptr ? target_ref->target_id : 0U;
    bool const outgoing_melee =
        attack != nullptr && outgoing_target != 0U &&
        attack->current_mode == Engine::Core::AttackComponent::CombatMode::Melee;
    bool const incoming_contact =
        contact != nullptr && std::any_of(contact->fronts.begin(),
                                          contact->fronts.end(),
                                          [](auto const& front) {
                                            return !front.outgoing && front.in_contact;
                                          });
    bool const melee_ordered = outgoing_melee || incoming_contact;

    Engine::Core::EntityID display_target = outgoing_target;
    if (display_target == 0U && contact != nullptr) {
      auto const first_contact =
          std::find_if(contact->fronts.begin(),
                       contact->fronts.end(),
                       [](auto const& front) { return front.in_contact; });
      if (first_contact != contact->fronts.end()) {
        display_target = first_contact->opponent_id;
      }
    }
    bool const target_alive = opponent_alive(world, display_target);
    float const combat_motion_time =
        melee_ordered ? presentation->combat_motion_time + std::max(0.0F, delta_time)
                      : presentation->combat_motion_time;

    auto const* actor_transform =
        entity->get_component<Engine::Core::TransformComponent>();
    auto* display_opponent = world.get_entity(display_target);
    bool const attacks_structure =
        outgoing_melee && display_opponent != nullptr && is_building(display_opponent);
    float closest_structure_gap = std::numeric_limits<float>::infinity();
    StructureSurfaceContact structure_facade{};
    float structure_render_shift = 0.0F;
    float structure_shift_local_x = 0.0F;
    float structure_shift_local_z = 0.0F;
    if (attacks_structure && actor_transform != nullptr) {
      QVector3D const root(actor_transform->position.x,
                           actor_transform->position.y,
                           actor_transform->position.z);
      structure_facade = closest_structure_surface(*display_opponent, root);
      for (auto const& slot : layout.live_slots) {
        QVector3D const anchor(slot.world_x, actor_transform->position.y, slot.world_z);
        QVector3D const from_facade = anchor - structure_facade.point;
        closest_structure_gap = std::min(
            closest_structure_gap,
            QVector3D::dotProduct(from_facade, structure_facade.outward_normal));
      }
      if (std::isfinite(closest_structure_gap)) {
        float const desired_gap = structure_attack_profile(entity).contact_clearance;
        structure_render_shift = std::max(0.0F, desired_gap - closest_structure_gap);

        float const yaw =
            actor_transform->rotation.y * std::numbers::pi_v<float> / 180.0F;
        float const sin_yaw = std::sin(yaw);
        float const cos_yaw = std::cos(yaw);
        QVector3D const world_shift =
            structure_facade.outward_normal * structure_render_shift;
        structure_shift_local_x = cos_yaw * world_shift.x() - sin_yaw * world_shift.z();
        structure_shift_local_z = sin_yaw * world_shift.x() + cos_yaw * world_shift.z();
      }
    }
    struct DamageCarrier {
      const Engine::Core::FormationContactFront* front{nullptr};
      std::optional<std::uint16_t> attacker_slot;
    };
    std::vector<DamageCarrier> damage_carriers;
    if (contact != nullptr) {
      damage_carriers.reserve(contact->fronts.size());
      for (auto const& front : contact->fronts) {
        if (!front.outgoing || !front.in_contact) {
          continue;
        }
        auto const carrier = FormationCombat::select_damage_engagement_pair(
            *entity, front.opponent_id, front.engagement_pairs);
        damage_carriers.push_back(
            {&front,
             carrier.has_value() ? std::optional<std::uint16_t>{carrier->attacker_slot}
                                 : std::nullopt});
      }
    }

    auto& directives = presentation->soldiers;
    std::size_t const previous_directive_count = directives.size();
    bool soldiers_changed = previous_directive_count != layout.all_slots.size();
    directives.resize(layout.all_slots.size());
    int traversal_live_ordinal = 0;
    for (auto const& original_slot : layout.all_slots) {
      auto const* live_slot = find_live_slot(layout, original_slot.index);
      std::optional<Engine::Core::FormationSoldierPresentation> previous_value;
      if (original_slot.index < previous_directive_count) {
        previous_value = directives[original_slot.index];
      }
      auto const* previous = previous_value.has_value() ? &*previous_value : nullptr;

      Engine::Core::FormationSoldierPresentation directive;
      directive.slot_index = original_slot.index;
      directive.row = live_slot != nullptr ? live_slot->row : original_slot.row;
      directive.col = live_slot != nullptr ? live_slot->col : original_slot.col;
      directive.local_x =
          live_slot != nullptr ? live_slot->local_x : original_slot.local_x;
      directive.local_z =
          live_slot != nullptr ? live_slot->local_z : original_slot.local_z;
      if (live_slot != nullptr && traversal_reflows) {
        int const rank_from_front = traversal_live_ordinal / traversal_files;
        int const col = traversal_live_ordinal % traversal_files;
        int const files_in_rank = std::min(traversal_files,
                                           static_cast<int>(layout.live_slots.size()) -
                                               rank_from_front * traversal_files);
        int const row = traversal_rows - 1 - rank_from_front;
        directive.row = static_cast<std::uint16_t>(row);
        directive.col = static_cast<std::uint16_t>(col);
        directive.local_x = (static_cast<float>(col) -
                             (static_cast<float>(files_in_rank) - 1.0F) * 0.5F) *
                            traversal_file_spacing;
        directive.local_z = (static_cast<float>(row) -
                             (static_cast<float>(traversal_rows) - 1.0F) * 0.5F) *
                            traversal_rank_spacing;
      } else if (live_slot != nullptr) {

        float const safe_scale =
            motion != nullptr && motion->traversal_squeeze_active
                ? std::max(traversal_lateral_scale, traversal_min_lateral_scale)
                : 1.0F;
        directive.local_x *= safe_scale;
      }
      if (live_slot != nullptr) {
        ++traversal_live_ordinal;
      }
      if (live_slot != nullptr && structure_render_shift > 0.0F) {

        directive.local_x += structure_shift_local_x;
        directive.local_z += structure_shift_local_z;
      }
      directive.local_yaw =
          live_slot != nullptr ? live_slot->local_yaw : original_slot.local_yaw;
      directive.alive = live_slot != nullptr;
      directive.combat_speed_scale =
          0.94F + hash_unit_float(layout.seed, original_slot.index * 73U + 19U) * 0.12F;
      directive.combat_phase_bias =
          (hash_unit_float(layout.seed, original_slot.index * 131U + 41U) - 0.5F) *
          0.56F;

      auto const assignment =
          directive.alive ? assignment_for_slot(contact, previous, original_slot.index)
                          : SoldierAssignment{};
      if (assignment.front != nullptr && assignment.pair != nullptr) {
        auto* opponent = world.get_entity(assignment.front->opponent_id);
        static const FormationCombat::FormationLayout k_empty_layout;
        auto const& opponent_layout =
            opponent != nullptr ? layout_for(*opponent) : k_empty_layout;
        auto const retained = retained_target_slot(
            opponent_layout, live_slot, previous, assignment, layout.spacing);

        directive.opponent_id = assignment.front->opponent_id;
        directive.target_slot = retained.slot;
        directive.engagement_surface_gap =
            assignment.pair->surface_gap +
            (retained.root_distance - assignment.pair->root_distance);
        directive.combat_role = combat_role_for(layout.seed, original_slot.index, true);
        directive.action = action_for_role(directive.combat_role);

        auto const* target_slot = find_live_slot(opponent_layout, retained.slot);
        auto const* opponent_transform =
            opponent != nullptr
                ? opponent->get_component<Engine::Core::TransformComponent>()
                : nullptr;
        if (actor_transform != nullptr && opponent_transform != nullptr) {
          float const target_x = target_slot != nullptr
                                     ? target_slot->world_x
                                     : opponent_transform->position.x;
          float const target_z = target_slot != nullptr
                                     ? target_slot->world_z
                                     : opponent_transform->position.z;
          auto const contact_vector = local_contact_vector(*actor_transform,
                                                           directive.local_x,
                                                           directive.local_z,
                                                           target_x,
                                                           target_z);
          float const desired_yaw = contact_vector.yaw;
          float const prior_yaw =
              previous != nullptr ? previous->local_yaw : directive.local_yaw;
          float const yaw_delta = std::remainder(desired_yaw - prior_yaw, 360.0F);
          float const max_turn = 300.0F * std::max(0.0F, delta_time);
          directive.local_yaw = prior_yaw + std::clamp(yaw_delta, -max_turn, max_turn);

          constexpr float k_weapon_contact_distance = 0.72F;
          float const pull_distance =
              std::clamp(contact_vector.distance - k_weapon_contact_distance,
                         0.0F,
                         layout.spacing * 1.35F);
          if (contact_vector.distance > 0.0001F) {
            directive.local_x +=
                contact_vector.x / contact_vector.distance * pull_distance;
            directive.local_z +=
                contact_vector.z / contact_vector.distance * pull_distance;
          }

          float const yaw_rad = desired_yaw * std::numbers::pi_v<float> / 180.0F;
          float const forward_x = std::sin(yaw_rad);
          float const forward_z = std::cos(yaw_rad);
          float const right_x = std::cos(yaw_rad);
          float const right_z = -std::sin(yaw_rad);
          std::uint32_t const soldier_seed =
              layout.seed ^
              (static_cast<std::uint32_t>(original_slot.index) * 0x9e3779b9U);
          float lateral = (hash_unit_float(soldier_seed, 0x4f1bbcdcU) - 0.5F) *
                          std::min(0.45F, layout.spacing * 0.48F);
          float depth = (hash_unit_float(soldier_seed, 0x94d049bbU) - 0.5F) *
                        std::min(0.34F, layout.spacing * 0.36F);
          float const motion_phase =
              combat_motion_time * directive.combat_speed_scale *
                  (2.0F * std::numbers::pi_v<float> / 0.95F) +
              directive.combat_phase_bias * 2.0F * std::numbers::pi_v<float>;
          float const forward_pulse = std::max(0.0F, std::sin(motion_phase));
          using Role = Engine::Core::FormationSoldierCombatRole;
          switch (directive.combat_role) {
          case Role::LeadStrike:
            depth += forward_pulse * 0.14F;
            break;
          case Role::SupportStrike:
            depth += forward_pulse * 0.09F;
            break;
          case Role::StepIn:
            depth += 0.08F + forward_pulse * 0.08F;
            break;
          case Role::StepOut:
            depth -= 0.08F + forward_pulse * 0.06F;
            break;
          case Role::Guard:
            depth -= 0.03F;
            break;
          case Role::Ready:
          case Role::None:
            break;
          }
          lateral += std::sin(motion_phase * 0.47F) * 0.035F;
          directive.local_x += right_x * lateral + forward_x * depth;
          directive.local_z += right_z * lateral + forward_z * depth;
        }

        if (assignment.front->outgoing) {
          auto const carrier =
              std::find_if(damage_carriers.begin(),
                           damage_carriers.end(),
                           [&assignment](auto const& candidate) {
                             return candidate.front == assignment.front;
                           });
          directive.damage_carrier = carrier != damage_carriers.end() &&
                                     carrier->attacker_slot.has_value() &&
                                     *carrier->attacker_slot == directive.slot_index;
        }
      } else if (directive.alive && melee_ordered) {
        directive.combat_role =
            combat_role_for(layout.seed, original_slot.index, false);
        directive.action = action_for_role(directive.combat_role);
        if (attacks_structure && actor_transform != nullptr) {
          QVector3D const anchor_world =
              local_to_world(*actor_transform, directive.local_x, directive.local_z);
          auto const surface =
              closest_structure_surface(*display_opponent, anchor_world);
          float const facade_gap = QVector3D::dotProduct(
              anchor_world - structure_facade.point, structure_facade.outward_normal);
          bool const facade_rank = facade_gap <= closest_structure_gap +
                                                     structure_render_shift +
                                                     layout.spacing * 0.55F;
          if (facade_rank) {
            auto const contact_vector = local_contact_vector(*actor_transform,
                                                             directive.local_x,
                                                             directive.local_z,
                                                             surface.point.x(),
                                                             surface.point.z());
            float const prior_yaw =
                previous != nullptr ? previous->local_yaw : directive.local_yaw;
            float const yaw_delta =
                std::remainder(contact_vector.yaw - prior_yaw, 360.0F);
            float const max_turn = 300.0F * std::max(0.0F, delta_time);
            directive.local_yaw =
                prior_yaw + std::clamp(yaw_delta, -max_turn, max_turn);

            float const desired_gap =
                structure_attack_profile(entity).contact_clearance;
            float const pull_distance =
                std::clamp(surface.distance - desired_gap, 0.0F, layout.spacing * 1.6F);
            if (contact_vector.distance > 0.0001F) {
              directive.local_x +=
                  contact_vector.x / contact_vector.distance * pull_distance;
              directive.local_z +=
                  contact_vector.z / contact_vector.distance * pull_distance;
            }
          }
        }
        if (previous != nullptr && presentation->target_id == display_target &&
            (previous->action == Engine::Core::FormationSoldierAction::MeleeEngaged ||
             previous->action ==
                 Engine::Core::FormationSoldierAction::MeleeFollowThrough)) {
          directive.action = Engine::Core::FormationSoldierAction::MeleeFollowThrough;
          directive.combat_role = previous->combat_role;
          directive.target_slot = previous->target_slot;
          directive.engagement_surface_gap = previous->engagement_surface_gap;
        }
      } else {
        directive.action = Engine::Core::FormationSoldierAction::FollowUnit;
      }

      bool const assigned = assignment.front != nullptr && assignment.pair != nullptr;
      if (!assigned && previous != nullptr && directive.alive) {
        directive.unassigned_seconds =
            std::min(previous->unassigned_seconds + std::max(0.0F, delta_time),
                     k_contact_yaw_hold_seconds + 1.0F);
        if (directive.unassigned_seconds < k_contact_yaw_hold_seconds) {
          directive.local_yaw = previous->local_yaw;
        } else {
          float const yaw_delta =
              std::remainder(directive.local_yaw - previous->local_yaw, 360.0F);
          float const max_turn = k_disengage_turn_degrees * std::max(0.0F, delta_time);
          directive.local_yaw =
              previous->local_yaw + std::clamp(yaw_delta, -max_turn, max_turn);
        }
      }

      if (previous != nullptr && directive.alive && !traversal_reflows) {
        float const step_time = std::max(0.0F, delta_time);
        float const blend = 1.0F - std::exp(-6.5F * step_time);
        float step_x = (directive.local_x - previous->local_x) * blend;
        float step_z = (directive.local_z - previous->local_z) * blend;

        float const step_length = std::hypot(step_x, step_z);
        float const max_step = k_max_reposition_speed * step_time;
        if (step_length > max_step && step_length > 0.0001F) {
          float const scale = max_step / step_length;
          step_x *= scale;
          step_z *= scale;
        }
        directive.local_x = previous->local_x + step_x;
        directive.local_z = previous->local_z + step_z;
      }
      if (directive.alive && attacks_structure && actor_transform != nullptr &&
          structure_facade.outward_normal.lengthSquared() > 0.000001F) {

        auto const rendered =
            local_to_world(*actor_transform, directive.local_x, directive.local_z);
        float const facade_gap = QVector3D::dotProduct(
            rendered - structure_facade.point, structure_facade.outward_normal);
        if (facade_gap < 0.0F) {
          QVector3D const correction = structure_facade.outward_normal * (-facade_gap);
          float const yaw =
              actor_transform->rotation.y * std::numbers::pi_v<float> / 180.0F;
          float const sin_yaw = std::sin(yaw);
          float const cos_yaw = std::cos(yaw);
          directive.local_x += cos_yaw * correction.x() - sin_yaw * correction.z();
          directive.local_z += sin_yaw * correction.x() + cos_yaw * correction.z();
        }
      }
      soldiers_changed =
          soldiers_changed || previous == nullptr || *previous != directive;
      directives[original_slot.index] = directive;
    }

    bool const changed = presentation->formation_seed != layout.seed ||
                         presentation->rows != layout.rows ||
                         presentation->cols != layout.cols ||
                         presentation->spacing != layout.spacing ||
                         presentation->target_id != display_target ||
                         presentation->target_alive != target_alive ||
                         presentation->melee_ordered != melee_ordered ||
                         presentation->allow_full_body_hit_reaction || soldiers_changed;
    presentation->formation_seed = layout.seed;
    presentation->rows = static_cast<std::uint16_t>(layout.rows);
    presentation->cols = static_cast<std::uint16_t>(layout.cols);
    presentation->spacing = layout.spacing;
    presentation->target_id = display_target;
    presentation->target_alive = target_alive;
    presentation->melee_ordered = melee_ordered;
    presentation->allow_full_body_hit_reaction = layout.live_slots.size() == 1U;
    presentation->combat_motion_time = combat_motion_time;
    if (changed) {
      ++presentation->revision;
    }
  }
}

} // namespace

void update_formation_contacts(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  publish_contacts(*world, build_fronts(*world));
  publish_formation_presentation(*world, delta_time);
}

} // namespace Game::Systems::Combat
