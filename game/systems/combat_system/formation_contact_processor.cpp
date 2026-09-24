#include "formation_contact_processor.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <limits>
#include <numbers>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../../units/combat_role.h"
#include "../../util/planar_math.h"
#include "../formation_combat_geometry.h"
#include "../nav_grid.h"
#include "../pathfinding.h"
#include "combat_random.h"
#include "combat_utils.h"
#include "structure_combat.h"

namespace Game::Systems::Combat {
namespace {

using FrontMap = std::unordered_map<Engine::Core::EntityID,
                                    std::vector<Engine::Core::FormationContactFront>>;

constexpr float k_target_switch_hysteresis = 0.60F;
constexpr float k_target_hold_seconds = 1.2F;
constexpr float k_target_hold_slack = 1.5F;
constexpr float k_engage_close_speed = 3.2F;
constexpr float k_contact_turn_degrees = 180.0F;

constexpr float k_contact_yaw_hold_seconds = 0.6F;
constexpr float k_disengage_turn_degrees = 120.0F;

constexpr float k_reform_walk_speed = 2.4F;
constexpr float k_reform_catch_up_scale = 1.35F;
constexpr float k_reform_turn_degrees = 420.0F;
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

auto hash_unit_float(std::uint32_t seed, std::uint32_t salt) noexcept -> float {
  return hash_to_unit_open(seed ^ salt);
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
      mix_hash32(formation_seed ^
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

auto brawls_as_a_crowd(const Engine::Core::World& world,
                       Engine::Core::EntityID entity_id) -> bool {
  auto const* unit = world.try_get<Engine::Core::UnitComponent>(entity_id);
  return unit != nullptr && Game::Units::combat_role(unit->spawn_type) ==
                                Game::Units::CombatRole::Noncombatant;
}

auto crowd_brawl_role(std::uint16_t stable_slot)
    -> Engine::Core::FormationSoldierCombatRole {
  return (stable_slot % 2U) == 0U
             ? Engine::Core::FormationSoldierCombatRole::LeadStrike
             : Engine::Core::FormationSoldierCombatRole::SupportStrike;
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
  bool const within_hysteresis =
      held_distance <= result.root_distance + spacing * k_target_switch_hysteresis;
  bool const still_holding =
      previous->target_held_seconds < k_target_hold_seconds &&
      held_distance <= result.root_distance + spacing * k_target_hold_slack;
  if (within_hysteresis || still_holding) {
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

auto world_vector_to_local(const Engine::Core::TransformComponent& actor,
                           float world_x,
                           float world_z) -> std::pair<float, float> {
  float const yaw = actor.rotation.y * std::numbers::pi_v<float> / 180.0F;
  float const sin_yaw = std::sin(yaw);
  float const cos_yaw = std::cos(yaw);
  return {cos_yaw * world_x - sin_yaw * world_z, sin_yaw * world_x + cos_yaw * world_z};
}

auto world_to_local(const Engine::Core::TransformComponent& actor,
                    float world_x,
                    float world_z) -> std::pair<float, float> {
  return world_vector_to_local(
      actor, world_x - actor.position.x, world_z - actor.position.z);
}

auto walk_to_new_slot(Engine::Core::SquadReformComponent& reform,
                      const Engine::Core::TransformComponent& actor,
                      float squad_speed,
                      float delta_time,
                      const Engine::Core::FormationSoldierPresentation* previous,
                      Engine::Core::FormationSoldierPresentation& directive) -> bool {
  auto walker = std::find_if(
      reform.soldiers.begin(), reform.soldiers.end(), [&directive](auto const& entry) {
        return entry.slot_index == directive.slot_index;
      });
  if (walker == reform.soldiers.end()) {
    return false;
  }
  QVector3D const slot = local_to_world(actor, directive.local_x, directive.local_z);
  float const dx = slot.x() - walker->world_x;
  float const dz = slot.z() - walker->world_z;
  float const distance = std::hypot(dx, dz);
  float const speed =
      std::max(k_reform_walk_speed, squad_speed * k_reform_catch_up_scale);
  float const step = speed * std::max(0.0F, delta_time);
  if (distance <= step) {
    reform.soldiers.erase(walker);
    return false;
  }
  walker->world_x += dx / distance * step;
  walker->world_z += dz / distance * step;

  auto const [local_x, local_z] =
      world_to_local(actor, walker->world_x, walker->world_z);
  auto const heading =
      world_to_local(actor, actor.position.x + dx, actor.position.z + dz);
  float const travel_yaw =
      std::atan2(heading.first, heading.second) * 180.0F / std::numbers::pi_v<float>;
  directive.local_x = local_x;
  directive.local_z = local_z;
  directive.local_yaw = Game::Systems::turn_yaw_toward(
      previous != nullptr ? previous->local_yaw : travel_yaw,
      travel_yaw,
      k_reform_turn_degrees * std::max(0.0F, delta_time));
  return true;
}

struct ForeignSoldier {
  Engine::Core::EntityID entity_id{0};
  std::uint16_t slot_index{0};
  float x{0.0F};
  float z{0.0F};
};

constexpr float k_foreign_soldier_cell = 0.75F;

constexpr float k_foreign_push_speed = 1.1F;
constexpr float k_crowd_offset_relax_seconds = 2.5F;
constexpr float k_crowd_offset_max_spacing = 0.9F;
constexpr float k_crowd_offset_settled = 0.005F;
constexpr float k_mounted_crowd_width = 0.75F;
constexpr float k_mounted_crowd_length_ratio = 0.55F;
constexpr float k_authored_velocity_smoothing_seconds = 0.10F;

class ForeignSoldierGrid {
public:
  void clear() { m_cells.clear(); }

  void insert(const ForeignSoldier& soldier) {
    m_cells[key(cell_of(soldier.x), cell_of(soldier.z))].push_back(soldier);
  }

  void gather(Engine::Core::EntityID self,
              std::uint16_t self_slot,
              float x,
              float z,
              float radius,
              std::vector<ForeignSoldier>& out) const {
    out.clear();
    if (m_cells.empty()) {
      return;
    }
    int const reach = static_cast<int>(std::ceil(radius / k_foreign_soldier_cell));
    int const cx = cell_of(x);
    int const cz = cell_of(z);
    for (int ix = cx - reach; ix <= cx + reach; ++ix) {
      for (int iz = cz - reach; iz <= cz + reach; ++iz) {
        auto const found = m_cells.find(key(ix, iz));
        if (found == m_cells.end()) {
          continue;
        }
        for (auto const& soldier : found->second) {
          bool const is_self =
              soldier.entity_id == self && soldier.slot_index == self_slot;
          if (!is_self && std::hypot(soldier.x - x, soldier.z - z) < radius) {
            out.push_back(soldier);
          }
        }
      }
    }
  }

private:
  [[nodiscard]] static auto cell_of(float value) -> int {
    return static_cast<int>(std::floor(value / k_foreign_soldier_cell));
  }
  [[nodiscard]] static auto key(int x, int z) -> std::uint64_t {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32U) |
           static_cast<std::uint64_t>(static_cast<std::uint32_t>(z));
  }

  std::unordered_map<std::uint64_t, std::vector<ForeignSoldier>> m_cells;
};

constexpr float k_slot_settle_distance = 0.12F;
constexpr float k_squared_distance_clear_margin = 1.001F;

void walk_formation_slot(
    const Engine::Core::TransformComponent& actor,
    const Engine::Core::FormationPresentationComponent& formation,
    const Engine::Core::FormationSoldierPresentation* previous,
    const std::vector<Engine::Core::FormationSoldierPresentation>& neighbors,
    const std::vector<ForeignSoldier>& foreign_neighbors,
    float crowd_step_budget,
    float squad_speed,
    float march_speed,
    float spacing,
    int rows,
    std::uint32_t seed,
    bool mounted,
    bool engaged,
    bool external_reform,

    bool position_is_authored,
    Pathfinding::Passability passability,
    float delta_time,
    Engine::Core::FormationSoldierPresentation& soldier) {
  const float dt = std::max(0.0F, delta_time);
  QVector3D destination = local_to_world(actor, soldier.local_x, soldier.local_z);
  const float desired_facing = actor.rotation.y + soldier.local_yaw;
  const float variation = hash_unit_float(seed, soldier.slot_index * 97U + 43U);
  const float max_speed = std::max(1.4F, std::max(march_speed, squad_speed) * 1.15F) *
                          (0.94F + variation * 0.12F);
  const float root_travel = std::hypot(actor.position.x - formation.motion_root_x,
                                       actor.position.z - formation.motion_root_z);
  const bool reset = previous == nullptr || !previous->alive ||
                     !previous->world_motion_valid || !formation.motion_root_valid ||
                     root_travel > std::max(12.0F, max_speed * dt * 4.0F);
  if (reset || external_reform) {
    soldier.world_x = destination.x();
    soldier.world_z = destination.z();
    soldier.world_yaw = desired_facing;
    soldier.world_motion_valid = true;
    if (!reset && dt > 0.0F) {
      soldier.world_velocity_x = (soldier.world_x - previous->world_x) / dt;
      soldier.world_velocity_z = (soldier.world_z - previous->world_z) / dt;
    }
    return;
  }

  soldier.world_motion_valid = true;
  soldier.world_x = previous->world_x;
  soldier.world_z = previous->world_z;
  soldier.world_yaw = previous->world_yaw;
  soldier.world_velocity_x = previous->world_velocity_x;
  soldier.world_velocity_z = previous->world_velocity_z;
  soldier.turning = previous->turning;
  soldier.turn_response_remaining = previous->turn_response_remaining;

  const float personal_space = std::max(0.28F, spacing * 0.72F);

  float const crowd_radius =
      mounted ? std::max(k_mounted_crowd_width, personal_space) : personal_space;
  float const along_scale = mounted ? k_mounted_crowd_length_ratio : 1.0F;
  float const heading = soldier.world_yaw * std::numbers::pi_v<float> / 180.0F;
  float const heading_x = std::sin(heading);
  float const heading_z = std::cos(heading);
  float const crowd_radius_sq_clear =
      crowd_radius * crowd_radius * k_squared_distance_clear_margin;
  float foreign_push_x = 0.0F;
  float foreign_push_z = 0.0F;
  for (const auto& neighbor : foreign_neighbors) {
    float away_x = soldier.world_x - neighbor.x;
    float away_z = soldier.world_z - neighbor.z;
    float const along = (away_x * heading_x) + (away_z * heading_z);
    float const across = (away_x * heading_z) - (away_z * heading_x);
    float const scaled_along = along * along_scale;
    if ((across * across) + (scaled_along * scaled_along) >= crowd_radius_sq_clear) {
      continue;
    }
    float const separation = std::hypot(across, scaled_along);
    if (separation >= crowd_radius) {
      continue;
    }
    if (separation < 0.001F) {

      float const angle = hash_unit_float(seed, soldier.slot_index * 211U + 7U) * 2.0F *
                          std::numbers::pi_v<float>;
      away_x = std::cos(angle);
      away_z = std::sin(angle);
    } else {
      float const length = std::hypot(away_x, away_z);
      away_x /= length;
      away_z /= length;
    }
    float const overlap = 1.0F - separation / crowd_radius;
    foreign_push_x += away_x * overlap;
    foreign_push_z += away_z * overlap;
  }
  float const push = std::hypot(foreign_push_x, foreign_push_z);
  if (push > 1.0F) {
    foreign_push_x /= push;
    foreign_push_z /= push;
  }
  float const relax = std::exp(-dt / k_crowd_offset_relax_seconds);
  float crowd_step_x = previous->crowd_offset_x * (relax - 1.0F) +
                       foreign_push_x * k_foreign_push_speed * dt;
  float crowd_step_z = previous->crowd_offset_z * (relax - 1.0F) +
                       foreign_push_z * k_foreign_push_speed * dt;

  float const crowd_step = std::hypot(crowd_step_x, crowd_step_z);
  float const budget = std::max(0.0F, crowd_step_budget);
  if (crowd_step > budget && crowd_step > 1.0e-6F) {
    crowd_step_x *= budget / crowd_step;
    crowd_step_z *= budget / crowd_step;
  }
  soldier.crowd_offset_x = previous->crowd_offset_x + crowd_step_x;
  soldier.crowd_offset_z = previous->crowd_offset_z + crowd_step_z;
  float const crowd_offset = std::hypot(soldier.crowd_offset_x, soldier.crowd_offset_z);
  float const crowd_limit = spacing * k_crowd_offset_max_spacing;
  if (crowd_offset > crowd_limit && crowd_offset > 0.0001F) {
    soldier.crowd_offset_x *= crowd_limit / crowd_offset;
    soldier.crowd_offset_z *= crowd_limit / crowd_offset;
  }
  destination.setX(destination.x() + soldier.crowd_offset_x);
  destination.setZ(destination.z() + soldier.crowd_offset_z);
  const float heading_change =
      signed_yaw_delta(formation.motion_root_yaw, actor.rotation.y);
  if (std::abs(heading_change) > 0.05F && !soldier.turning) {
    const float rear_rank =
        rows > 1 ? static_cast<float>(std::max(0, rows - 1 - soldier.row)) /
                       static_cast<float>(rows - 1)
                 : 0.0F;
    soldier.turn_response_remaining =
        engaged ? 0.0F : 0.035F + variation * 0.14F + rear_rank * 0.20F;
    soldier.turning = true;
  }
  soldier.turn_response_remaining =
      std::max(0.0F, soldier.turn_response_remaining - dt);

  const float target_x = destination.x() - actor.position.x;
  const float target_z = destination.z() - actor.position.z;
  const float heading_rate =
      dt > 0.0F ? heading_change * std::numbers::pi_v<float> / 180.0F / dt : 0.0F;
  const float slot_velocity_x =
      dt > 0.0F
          ? (actor.position.x - formation.motion_root_x) / dt + heading_rate * target_z
          : 0.0F;
  const float slot_velocity_z =
      dt > 0.0F
          ? (actor.position.z - formation.motion_root_z) / dt - heading_rate * target_x
          : 0.0F;

  const float dx = destination.x() - soldier.world_x;
  const float dz = destination.z() - soldier.world_z;
  const float distance = std::hypot(dx, dz);
  float catch_x = distance > 0.0001F ? dx / distance : 0.0F;
  float catch_z = distance > 0.0001F ? dz / distance : 0.0F;

  const float radial_x = soldier.world_x - actor.position.x;
  const float radial_z = soldier.world_z - actor.position.z;
  const float radius = std::hypot(radial_x, radial_z);
  const float target_radius = std::hypot(target_x, target_z);
  if (!engaged && std::abs(heading_change) > 0.05F && radius > 0.5F &&
      target_radius > 0.5F) {
    const float angle = std::atan2(radial_z * target_x - radial_x * target_z,
                                   radial_x * target_x + radial_z * target_z);
    const float bend = std::clamp((std::abs(angle) - 0.20F) / 0.70F, 0.0F, 1.0F);
    const float sign = angle < 0.0F ? -1.0F : 1.0F;
    const float radial_correction =
        std::clamp((target_radius - radius) / radius, -0.4F, 0.4F);
    catch_x +=
        (sign * radial_z / radius + radial_x / radius * radial_correction - catch_x) *
        bend;
    catch_z +=
        (-sign * radial_x / radius + radial_z / radius * radial_correction - catch_z) *
        bend;
  }

  float crowd_speed = 1.0F;
  const float personal_space_sq_clear =
      personal_space * personal_space * k_squared_distance_clear_margin;
  for (const auto& neighbor : neighbors) {
    if (!neighbor.alive || !neighbor.world_motion_valid ||
        neighbor.slot_index == soldier.slot_index) {
      continue;
    }
    const float away_x = soldier.world_x - neighbor.world_x;
    const float away_z = soldier.world_z - neighbor.world_z;
    if ((away_x * away_x) + (away_z * away_z) >= personal_space_sq_clear) {
      continue;
    }
    const float separation = std::hypot(away_x, away_z);
    if (separation > 0.001F && separation < personal_space) {
      const bool approaching = catch_x * away_x + catch_z * away_z < 0.0F;
      if (approaching) {
        crowd_speed = std::min(
            crowd_speed,
            std::clamp((separation - personal_space * 0.5F) / (personal_space * 0.5F),
                       0.0F,
                       1.0F));
      }
      const float pressure = std::max(0.0F, 1.0F - separation / personal_space) * 2.5F;
      catch_x += away_x / separation * pressure;
      catch_z += away_z / separation * pressure;

      if (approaching) {
        catch_x -= away_z / separation * pressure * 0.45F;
        catch_z += away_x / separation * pressure * 0.45F;
      }
    }
  }
  const float catch_length = std::hypot(catch_x, catch_z);
  if (catch_length > 0.0001F) {
    catch_x /= catch_length;
    catch_z /= catch_length;
  }

  const float catch_up_cap =
      std::max(1.0F, march_speed * 0.6F) * (0.94F + variation * 0.12F);
  const float catch_up_speed = std::min(catch_up_cap, distance * 4.0F) * crowd_speed;
  float desired_x = slot_velocity_x + catch_x * catch_up_speed;
  float desired_z = slot_velocity_z + catch_z * catch_up_speed;

  const float speed_ceiling = max_speed;
  float desired_magnitude = std::hypot(desired_x, desired_z);
  if (desired_magnitude > speed_ceiling && desired_magnitude > 0.0001F) {
    const float scale = speed_ceiling / desired_magnitude;
    desired_x *= scale;
    desired_z *= scale;
    desired_magnitude = speed_ceiling;
  }
  const float travel_yaw =
      desired_magnitude > 0.25F
          ? std::atan2(desired_x, desired_z) * 180.0F / std::numbers::pi_v<float>
          : desired_facing;
  const float facing_target =
      !engaged && desired_magnitude > 0.25F ? travel_yaw : desired_facing;
  const bool responding = soldier.turn_response_remaining <= 0.0F;
  if (responding) {
    soldier.world_yaw =
        turn_yaw_toward(soldier.world_yaw,
                        facing_target,
                        (mounted ? 125.0F : 185.0F) * (0.85F + variation * 0.30F) * dt);
  }
  const float alignment = std::cos(signed_yaw_delta(soldier.world_yaw, travel_yaw) *
                                   std::numbers::pi_v<float> / 180.0F);
  const float mobility =
      engaged ? 1.0F : std::clamp((alignment - 0.15F) / 0.85F, 0.0F, 1.0F);
  if (responding) {
    desired_x *= mobility;
    desired_z *= mobility;
  } else {

    desired_x = soldier.world_velocity_x;
    desired_z = soldier.world_velocity_z;
  }
  const float velocity_dx = desired_x - soldier.world_velocity_x;
  const float velocity_dz = desired_z - soldier.world_velocity_z;
  const float velocity_change = std::hypot(velocity_dx, velocity_dz);
  const float acceleration_step =
      std::max(max_speed, desired_magnitude) / (mounted ? 0.45F : 0.28F) * dt;
  const float velocity_blend = velocity_change > 0.0001F
                                   ? std::min(1.0F, acceleration_step / velocity_change)
                                   : 1.0F;
  soldier.world_velocity_x += velocity_dx * velocity_blend;
  soldier.world_velocity_z += velocity_dz * velocity_blend;
  float step_x = soldier.world_velocity_x * dt;
  float step_z = soldier.world_velocity_z * dt;
  if (auto const* pathfinder = NavGrid::get_pathfinder();
      pathfinder != nullptr && std::hypot(step_x, step_z) > 0.0001F) {
    const QVector3D origin(soldier.world_x, actor.position.y, soldier.world_z);
    auto clear_step = [&](float x, float z) {
      return pathfinder->is_world_segment_walkable(
          origin, origin + QVector3D(x, 0.0F, z), passability);
    };
    if (!clear_step(step_x, step_z)) {
      if (clear_step(step_x, 0.0F)) {
        step_z = 0.0F;
      } else if (clear_step(0.0F, step_z)) {
        step_x = 0.0F;
      } else {
        step_x = 0.0F;
        step_z = 0.0F;
      }
      soldier.relocation_blocked = true;
    }
  }

  if (position_is_authored) {
    step_x = dx;
    step_z = dz;
  } else if (!soldier.relocation_blocked && foreign_neighbors.empty() &&
             std::hypot(soldier.crowd_offset_x, soldier.crowd_offset_z) <
                 k_crowd_offset_settled &&
             distance > 0.0001F && distance < k_slot_settle_distance) {

    step_x = dx;
    step_z = dz;
  }
  soldier.world_x += step_x;
  soldier.world_z += step_z;
  if (dt > 0.0F) {
    soldier.world_velocity_x = step_x / dt;
    soldier.world_velocity_z = step_z / dt;
    if (position_is_authored) {

      float const blend = std::min(1.0F, dt / k_authored_velocity_smoothing_seconds);
      soldier.world_velocity_x =
          previous->world_velocity_x +
          (soldier.world_velocity_x - previous->world_velocity_x) * blend;
      soldier.world_velocity_z =
          previous->world_velocity_z +
          (soldier.world_velocity_z - previous->world_velocity_z) * blend;
    }
    soldier.angular_speed =
        std::abs(signed_yaw_delta(previous->world_yaw, soldier.world_yaw)) / dt;
  }
  const auto local = world_to_local(actor, soldier.world_x, soldier.world_z);
  soldier.local_x = local.first;
  soldier.local_z = local.second;
  soldier.local_yaw = signed_yaw_delta(actor.rotation.y, soldier.world_yaw);
  const auto previous_local =
      world_to_local(actor, previous->world_x, previous->world_z);
  soldier.previous_local_x = previous_local.first;
  soldier.previous_local_z = previous_local.second;
  const auto [relocation_vx, relocation_vz] =
      world_vector_to_local(actor, soldier.world_velocity_x, soldier.world_velocity_z);
  soldier.relocation_velocity_x = relocation_vx;
  soldier.relocation_velocity_z = relocation_vz;
  const float remaining =
      std::hypot(destination.x() - soldier.world_x, destination.z() - soldier.world_z);
  const float facing_error =
      std::abs(signed_yaw_delta(soldier.world_yaw, desired_facing));
  soldier.reforming =
      remaining > 0.06F || facing_error > 5.0F || std::hypot(step_x, step_z) > 0.001F;
  if (std::abs(heading_change) < 0.05F && remaining < 0.08F && facing_error < 3.0F) {
    soldier.turning = false;
  }
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
  thread_local std::unordered_map<Engine::Core::EntityID, std::size_t> layout_index;
  thread_local std::deque<FormationCombat::FormationLayout> layout_pool;
  layout_index.clear();
  std::size_t layouts_used = 0U;
  auto layout_for =
      [&layouts_used](
          Engine::Core::Entity& entity) -> const FormationCombat::FormationLayout& {
    auto const [entry, inserted] =
        layout_index.try_emplace(entity.get_id(), layouts_used);
    if (inserted) {
      if (layouts_used == layout_pool.size()) {
        layout_pool.emplace_back();
      }
      FormationCombat::resolve_layout_into(entity, layout_pool[layouts_used]);
      ++layouts_used;
    }
    return layout_pool[entry->second];
  };

  thread_local ForeignSoldierGrid foreign_grid;
  thread_local std::vector<ForeignSoldier> foreign_scratch;
  foreign_grid.clear();
  for (auto [entity_ref, entity_unit] :
       world.entity_view<Engine::Core::UnitComponent>()) {
    auto const* published = world.try_get<Engine::Core::FormationPresentationComponent>(
        entity_ref.get_id());
    if (published == nullptr || !published->melee_ordered) {
      continue;
    }
    auto const* published_attack =
        world.try_get<Engine::Core::AttackComponent>(entity_ref.get_id());
    auto const* published_contact =
        world.try_get<Engine::Core::FormationContactComponent>(entity_ref.get_id());
    bool const fighting =
        (published_attack != nullptr && published_attack->in_melee_lock) ||
        (published_contact != nullptr &&
         std::any_of(published_contact->fronts.begin(),
                     published_contact->fronts.end(),
                     [](auto const& front) { return front.in_contact; }));
    if (!fighting) {
      continue;
    }
    for (auto const& soldier : published->soldiers) {
      if (soldier.alive && soldier.world_motion_valid) {
        foreign_grid.insert({entity_ref.get_id(),
                             soldier.slot_index,
                             soldier.world_x,
                             soldier.world_z});
      }
    }
  }

  for (auto [entity_ref, entity_unit] :
       world.entity_view<Engine::Core::UnitComponent>()) {
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

    auto const* attack = world.try_get<Engine::Core::AttackComponent>(entity->get_id());
    auto const* target_ref =
        world.try_get<Engine::Core::AttackTargetComponent>(entity->get_id());
    auto const* contact =
        world.try_get<Engine::Core::FormationContactComponent>(entity->get_id());
    auto const* traversal =
        world.try_get<Engine::Core::UnitTraversalLayoutStateComponent>(
            entity->get_id());
    Engine::Core::EntityID const outgoing_target =
        target_ref != nullptr ? target_ref->target_id : 0U;
    bool const outgoing_melee = is_melee_mode(attack) && outgoing_target != 0U;
    bool const incoming_contact =
        contact != nullptr && std::any_of(contact->fronts.begin(),
                                          contact->fronts.end(),
                                          [](auto const& front) {
                                            return !front.outgoing && front.in_contact;
                                          });
    bool const melee_ordered = outgoing_melee || incoming_contact;

    bool const in_melee_contact =
        (attack != nullptr && attack->in_melee_lock) ||
        (contact != nullptr &&
         std::any_of(contact->fronts.begin(),
                     contact->fronts.end(),
                     [](auto const& front) { return front.in_contact; }));

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
        world.try_get<Engine::Core::TransformComponent>(entity->get_id());
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

        QVector3D const world_shift =
            structure_facade.outward_normal * structure_render_shift;
        auto const [shift_local_x, shift_local_z] =
            world_vector_to_local(*actor_transform, world_shift.x(), world_shift.z());
        structure_shift_local_x = shift_local_x;
        structure_shift_local_z = shift_local_z;
      }
    }

    float nearest_facade_anchor = std::numeric_limits<float>::infinity();
    if (display_opponent != nullptr && actor_transform != nullptr &&
        is_building(display_opponent)) {
      for (auto const& slot : layout.live_slots) {
        float anchor_x = slot.local_x;
        float anchor_z = slot.local_z;
        if (traversal != nullptr) {
          if (auto const* moved = traversal->slot_for(slot.index); moved != nullptr) {
            anchor_x = moved->current_local_x;
            anchor_z = moved->current_local_z;
          }
        }
        nearest_facade_anchor = std::min(
            nearest_facade_anchor,
            closest_structure_surface(
                *display_opponent, local_to_world(*actor_transform, anchor_x, anchor_z))
                .distance);
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

    auto* reform = world.try_get<Engine::Core::SquadReformComponent>(entity->get_id());
    if (reform != nullptr) {
      reform->remaining_seconds -= std::max(0.0F, delta_time);
      if (reform->remaining_seconds <= 0.0F || actor_transform == nullptr) {
        reform->soldiers.clear();
      }
    }
    float squad_speed = 0.0F;
    if (auto const* movement =
            world.try_get<Engine::Core::MovementComponent>(entity->get_id())) {
      squad_speed = std::hypot(movement->get_vx(), movement->get_vz());
    }

    auto& directives = presentation->soldiers;

    const auto previous_soldiers = directives;
    auto const* movement =
        world.try_get<Engine::Core::MovementComponent>(entity->get_id());
    const auto passability = movement != nullptr && movement->get_can_enter_forest()
                                 ? Pathfinding::Passability::Light
                                 : Pathfinding::Passability::Heavy;
    const bool mounted = Game::Units::is_cavalry(entity_unit.spawn_type);
    std::size_t const previous_directive_count = directives.size();
    bool soldiers_changed = previous_directive_count != layout.all_slots.size();
    directives.resize(layout.all_slots.size());
    float const frame_sign =
        traversal != nullptr && traversal->about_faced ? -1.0F : 1.0F;
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
      directive.local_x = frame_sign * (live_slot != nullptr ? live_slot->local_x
                                                             : original_slot.local_x);
      directive.local_z = frame_sign * (live_slot != nullptr ? live_slot->local_z
                                                             : original_slot.local_z);
      auto const* traversal_slot =
          traversal != nullptr ? traversal->slot_for(original_slot.index) : nullptr;
      if (live_slot != nullptr && traversal != nullptr) {
        if (traversal_slot != nullptr) {
          directive.row = traversal_slot->row;
          directive.col = traversal_slot->col;
          directive.previous_local_x = traversal_slot->previous_local_x;
          directive.previous_local_z = traversal_slot->previous_local_z;
          directive.local_x = traversal_slot->current_local_x;
          directive.local_z = traversal_slot->current_local_z;
          directive.relocation_velocity_x = traversal_slot->velocity_x;
          directive.relocation_velocity_z = traversal_slot->velocity_z;
          directive.relocation_blocked = traversal_slot->blocked;
        }
      }
      if (live_slot != nullptr && structure_render_shift > 0.0F) {

        directive.local_x += structure_shift_local_x;
        directive.local_z += structure_shift_local_z;
      }

      float const anchor_local_x = directive.local_x;
      float const anchor_local_z = directive.local_z;
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
        bool const same_target = previous != nullptr &&
                                 previous->opponent_id == directive.opponent_id &&
                                 previous->target_slot == directive.target_slot;
        directive.target_held_seconds =
            same_target
                ? std::min(previous->target_held_seconds + std::max(0.0F, delta_time),
                           k_target_hold_seconds + 1.0F)
                : 0.0F;
        directive.engagement_surface_gap =
            assignment.pair->surface_gap +
            (retained.root_distance - assignment.pair->root_distance);

        bool const opponent_is_structure = opponent != nullptr && is_building(opponent);
        StructureSurfaceContact facade_surface{};
        bool structure_facade_rank = false;
        if (opponent_is_structure && actor_transform != nullptr) {
          facade_surface = closest_structure_surface(
              *opponent,
              local_to_world(*actor_transform, directive.local_x, directive.local_z));
          structure_facade_rank = opponent != display_opponent ||
                                  !std::isfinite(nearest_facade_anchor) ||
                                  facade_surface.distance <=
                                      nearest_facade_anchor + structure_render_shift +
                                          layout.spacing * 0.55F;
        }

        bool const opponent_within_reach =
            opponent_is_structure
                ? structure_facade_rank
                : directive.engagement_surface_gap <= layout.spacing * 0.65F;
        directive.combat_role =
            brawls_as_a_crowd(world, entity->get_id())
                ? crowd_brawl_role(original_slot.index)
            : opponent_within_reach
                ? combat_role_for(layout.seed, original_slot.index, true)
                : Engine::Core::FormationSoldierCombatRole::Guard;
        directive.action = action_for_role(directive.combat_role);

        auto const* target_slot = find_live_slot(opponent_layout, retained.slot);
        auto const* opponent_transform =
            opponent != nullptr
                ? world.try_get<Engine::Core::TransformComponent>(opponent->get_id())
                : nullptr;
        if (actor_transform != nullptr && opponent_transform != nullptr) {
          float const target_x = structure_facade_rank ? facade_surface.point.x()
                                 : target_slot != nullptr
                                     ? target_slot->world_x
                                     : opponent_transform->position.x;
          float const target_z = structure_facade_rank ? facade_surface.point.z()
                                 : target_slot != nullptr
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
          directive.local_yaw = Game::Systems::turn_yaw_toward(
              prior_yaw,
              desired_yaw,
              k_contact_turn_degrees * std::max(0.0F, delta_time));

          constexpr float k_weapon_contact_distance = 0.72F;
          float const pull_distance =
              structure_facade_rank
                  ? std::clamp(facade_surface.distance -
                                   structure_attack_profile(entity).contact_clearance,
                               0.0F,
                               layout.spacing * 1.6F)
                  : std::clamp(contact_vector.distance - k_weapon_contact_distance,
                               0.0F,
                               layout.spacing * 0.20F);
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

          float const lateral = (hash_unit_float(soldier_seed, 0x4f1bbcdcU) - 0.5F) *
                                std::min(0.08F, layout.spacing * 0.08F);
          float const depth = (hash_unit_float(soldier_seed, 0x94d049bbU) - 0.5F) *
                              std::min(0.06F, layout.spacing * 0.06F);
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
            brawls_as_a_crowd(world, entity->get_id())
                ? crowd_brawl_role(original_slot.index)
                : combat_role_for(layout.seed, original_slot.index, false);
        directive.action = action_for_role(directive.combat_role);
        bool const faces_an_animal =
            display_opponent != nullptr &&
            world.has<Engine::Core::WildlifeComponent>(display_opponent->get_id());
        if ((brawls_as_a_crowd(world, entity->get_id()) || faces_an_animal) &&
            !attacks_structure && actor_transform != nullptr &&
            display_opponent != nullptr) {
          if (auto const* opponent_transform =
                  world.try_get<Engine::Core::TransformComponent>(
                      display_opponent->get_id())) {
            auto const toward = local_contact_vector(*actor_transform,
                                                     directive.local_x,
                                                     directive.local_z,
                                                     opponent_transform->position.x,
                                                     opponent_transform->position.z);
            if (toward.distance > 0.0001F) {
              directive.local_yaw = toward.yaw;
            }
          }
        }
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
            directive.local_yaw = Game::Systems::turn_yaw_toward(
                prior_yaw,
                contact_vector.yaw,
                k_contact_turn_degrees * std::max(0.0F, delta_time));

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
          directive.local_yaw = Game::Systems::turn_yaw_toward(
              previous->local_yaw,
              directive.local_yaw,
              k_disengage_turn_degrees * std::max(0.0F, delta_time));
        }
      }

      if (!directive.alive && previous != nullptr &&
          (previous->alive || previous->contact_offset_x != 0.0F ||
           previous->contact_offset_z != 0.0F)) {

        directive.contact_offset_x = previous->contact_offset_x;
        directive.contact_offset_z = previous->contact_offset_z;
        directive.local_x = anchor_local_x + previous->contact_offset_x;
        directive.local_z = anchor_local_z + previous->contact_offset_z;
        directive.local_yaw = previous->local_yaw;

        directive.crowd_offset_x = previous->crowd_offset_x;
        directive.crowd_offset_z = previous->crowd_offset_z;
        if (actor_transform != nullptr) {
          auto const [crowd_local_x, crowd_local_z] = world_vector_to_local(
              *actor_transform, previous->crowd_offset_x, previous->crowd_offset_z);
          directive.local_x += crowd_local_x;
          directive.local_z += crowd_local_z;
        }
      }
      if (directive.alive) {
        float const desired_offset_x = directive.local_x - anchor_local_x;
        float const desired_offset_z = directive.local_z - anchor_local_z;
        bool const continues = previous != nullptr && previous->alive;
        float const prior_offset_x = continues ? previous->contact_offset_x : 0.0F;
        float const prior_offset_z = continues ? previous->contact_offset_z : 0.0F;
        float const step_x = desired_offset_x - prior_offset_x;
        float const step_z = desired_offset_z - prior_offset_z;
        float const step = std::hypot(step_x, step_z);
        float const allowed = k_engage_close_speed * std::max(0.0F, delta_time);
        float offset_x = desired_offset_x;
        float offset_z = desired_offset_z;
        if (step > allowed && step > 0.0001F) {
          offset_x = prior_offset_x + step_x / step * allowed;
          offset_z = prior_offset_z + step_z / step * allowed;
        }
        directive.contact_offset_x = offset_x;
        directive.contact_offset_z = offset_z;
        directive.local_x = anchor_local_x + offset_x;
        directive.local_z = anchor_local_z + offset_z;
      }
      if (reform != nullptr && directive.alive && actor_transform != nullptr) {
        directive.reforming = walk_to_new_slot(
            *reform, *actor_transform, squad_speed, delta_time, previous, directive);
      }

      if (traversal_slot == nullptr && previous != nullptr && directive.alive) {
        float const step_time = std::max(0.0F, delta_time);
        directive.previous_local_x = previous->local_x;
        directive.previous_local_z = previous->local_z;
        directive.relocation_velocity_x =
            step_time > 0.0F
                ? (directive.local_x - directive.previous_local_x) / step_time
                : 0.0F;
        directive.relocation_velocity_z =
            step_time > 0.0F
                ? (directive.local_z - directive.previous_local_z) / step_time
                : 0.0F;
      } else if (traversal_slot == nullptr) {
        directive.previous_local_x = directive.local_x;
        directive.previous_local_z = directive.local_z;
      }
      if (directive.alive && attacks_structure && actor_transform != nullptr &&
          structure_facade.outward_normal.lengthSquared() > 0.000001F) {

        auto const rendered =
            local_to_world(*actor_transform, directive.local_x, directive.local_z);
        float const facade_gap = QVector3D::dotProduct(
            rendered - structure_facade.point, structure_facade.outward_normal);
        if (facade_gap < 0.0F) {
          QVector3D const correction = structure_facade.outward_normal * (-facade_gap);
          auto const [correction_x, correction_z] =
              world_vector_to_local(*actor_transform, correction.x(), correction.z());
          directive.local_x += correction_x;
          directive.local_z += correction_z;
        }
      }
      if (directive.alive && actor_transform != nullptr &&
          layout.all_slots.size() > 1U) {

        if (!melee_ordered) {
          directive.local_yaw =
              live_slot != nullptr ? live_slot->local_yaw : original_slot.local_yaw;
        }
        foreign_scratch.clear();
        if (in_melee_contact && previous != nullptr && previous->world_motion_valid) {
          float const personal_space = std::max(0.28F, layout.spacing * 0.72F);
          float const gather_radius =
              mounted ? std::max(k_mounted_crowd_width, personal_space) /
                            k_mounted_crowd_length_ratio
                      : personal_space;
          foreign_grid.gather(entity->get_id(),
                              original_slot.index,
                              previous->world_x,
                              previous->world_z,
                              gather_radius,
                              foreign_scratch);
        }
        float const contact_step =
            previous != nullptr
                ? std::hypot(directive.contact_offset_x - previous->contact_offset_x,
                             directive.contact_offset_z - previous->contact_offset_z)
                : 0.0F;
        float const crowd_step_budget =
            k_engage_close_speed * std::max(0.0F, delta_time) - contact_step;
        walk_formation_slot(*actor_transform,
                            *presentation,
                            previous,
                            previous_soldiers,
                            foreign_scratch,
                            crowd_step_budget,
                            squad_speed,
                            entity_unit.speed,
                            layout.spacing,
                            layout.rows,
                            layout.seed,
                            mounted,
                            melee_ordered,
                            reform != nullptr,
                            traversal_slot != nullptr,
                            passability,
                            delta_time,
                            directive);
      }
      soldiers_changed =
          soldiers_changed || previous == nullptr || *previous != directive;
      directives[original_slot.index] = directive;
    }

    if (reform != nullptr) {
      std::erase_if(reform->soldiers, [&layout](auto const& entry) {
        return find_live_slot(layout, entry.slot_index) == nullptr;
      });
      if (reform->soldiers.empty()) {
        world.remove<Engine::Core::SquadReformComponent>(entity->get_id());
      }
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
    if (actor_transform != nullptr) {
      presentation->motion_root_x = actor_transform->position.x;
      presentation->motion_root_z = actor_transform->position.z;
      presentation->motion_root_yaw = actor_transform->rotation.y;
      presentation->motion_root_valid = true;
    }
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
