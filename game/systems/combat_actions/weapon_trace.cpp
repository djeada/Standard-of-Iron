#include "weapon_trace.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "../../core/component_combat.h"
#include "../../core/simulation_timing.h"
#include "../../core/world.h"
#include "../combat_rules.h"
#include "../combat_system/target_rules.h"
#include "../rpg_combat_system/rpg_targeting.h"
#include "weapon_trace_sampling.h"

namespace Game::Systems::CombatActions {

namespace {

struct LocalTargetSample {
  Engine::Core::Entity* entity{nullptr};
  std::uint16_t soldier_slot{
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot};
  float forward{0.0F};
  float right{0.0F};
  float distance{0.0F};
  float radius{0.0F};
  QVector3D world_position{0.0F, 0.0F, 0.0F};
};

struct SegmentDistance {
  float distance{std::numeric_limits<float>::infinity()};
  QVector3D point{0.0F, 0.0F, 0.0F};
};

[[nodiscard]] auto distance_to_segment_xz(const QVector3D& point,
                                          const QVector3D& start,
                                          const QVector3D& end) -> SegmentDistance {
  float const ax = start.x();
  float const az = start.z();
  float const bx = end.x();
  float const bz = end.z();
  float const px = point.x();
  float const pz = point.z();
  float const abx = bx - ax;
  float const abz = bz - az;
  float const ab_len_sq = abx * abx + abz * abz;

  float t = 0.0F;
  if (ab_len_sq > 1.0e-6F) {
    t = ((px - ax) * abx + (pz - az) * abz) / ab_len_sq;
    t = std::clamp(t, 0.0F, 1.0F);
  }

  QVector3D const closest = start + (end - start) * t;
  float const dx = px - closest.x();
  float const dz = pz - closest.z();
  return {.distance = std::sqrt(dx * dx + dz * dz), .point = closest};
}

[[nodiscard]] auto best_segment_distance_xz(const WeaponTraceSegment& segment,
                                            const QVector3D& point) -> SegmentDistance {
  SegmentDistance best;
  auto consider = [&](const QVector3D& start, const QVector3D& end) {
    auto const distance = distance_to_segment_xz(point, start, end);
    if (distance.distance < best.distance) {
      best = distance;
    }
  };

  consider(segment.current_base, segment.current_tip);
  consider(segment.previous_tip, segment.current_tip);
  consider(segment.previous_base, segment.current_base);
  consider(segment.previous_base, segment.previous_tip);
  return best;
}

[[nodiscard]] auto make_local_sample(
    const AttackerFrame& frame,
    const Game::Systems::RpgCombat::SoldierTarget& target) -> LocalTargetSample {
  LocalTargetSample sample;
  sample.entity = target.entity;
  sample.soldier_slot = target.soldier_slot;

  if (!frame.valid || target.entity == nullptr) {
    sample.entity = nullptr;
    return sample;
  }

  sample.world_position = target.position;
  QVector3D const to_target = sample.world_position - frame.origin;
  sample.forward = QVector3D::dotProduct(to_target, frame.forward);
  sample.right = QVector3D::dotProduct(to_target, frame.right);
  sample.distance =
      std::sqrt(sample.forward * sample.forward + sample.right * sample.right);
  sample.radius = target.body_radius;
  return sample;
}

struct WeaponContactShape {
  float lateral_limit{0.0F};
  float min_forward{0.0F};
};

[[nodiscard]] auto contact_shape(const CombatActionDefinition& definition,
                                 const Engine::Core::MeleeIntent& intent,
                                 float target_radius) -> WeaponContactShape {
  float const vertical = std::clamp(std::abs(intent.strike_dir_y), 0.0F, 1.0F);
  float const thrust = intent.thrust_amount;

  float const cut_lateral = definition.hit_shape.reach * 0.70F;
  float const overhead_lateral = std::max(0.55F, definition.hit_shape.radius);
  float const sweep_lateral = std::lerp(cut_lateral, overhead_lateral, vertical);

  return {.lateral_limit =
              std::lerp(sweep_lateral, definition.hit_shape.radius, thrust) +
              target_radius,
          .min_forward = std::lerp(std::lerp(0.05F, 0.10F, vertical), 0.25F, thrust)};
}

[[nodiscard]] auto
weapon_contact_score(const LocalTargetSample& sample,
                     const CombatActionDefinition& definition,
                     const Engine::Core::MeleeIntent& intent) -> float {
  if (sample.entity == nullptr || !std::isfinite(sample.forward) ||
      !std::isfinite(sample.right) || !std::isfinite(sample.distance) ||
      sample.forward <= 0.0F) {
    return std::numeric_limits<float>::infinity();
  }

  float const reach = definition.hit_shape.reach + sample.radius;
  if (sample.distance > reach) {
    return std::numeric_limits<float>::infinity();
  }

  auto const shape = contact_shape(definition, intent, sample.radius);
  if (sample.forward < shape.min_forward ||
      std::abs(sample.right) > shape.lateral_limit) {
    return std::numeric_limits<float>::infinity();
  }

  return sample.distance + std::abs(sample.right) * 0.15F;
}

inline constexpr float k_max_trace_sample_span = 0.025F;
inline constexpr float k_directed_hint_score_bias = 0.25F;
inline constexpr float k_aimed_hint_score_bias = 0.04F;

[[nodiscard]] auto hint_score_bias(const Engine::Core::Entity& attacker) -> float {
  return Game::Systems::CombatRules::is_player_driven(&attacker)
             ? k_aimed_hint_score_bias
             : k_directed_hint_score_bias;
}

[[nodiscard]] auto is_melee_weapon(const CombatActionDefinition& definition) -> bool {
  return definition.weapon_family == WeaponFamily::Sword ||
         definition.weapon_family == WeaponFamily::Spear;
}

struct ContactQuery {
  Engine::Core::Entity& attacker;
  const Engine::Core::UnitComponent& attacker_unit;
  const PresentedAttackerFrame& presented;
  Engine::Core::EntityID target_hint_id;
  std::span<const Engine::Core::EntityID> ignored_target_ids;
  std::span<const WeaponTraceIgnoredTarget> ignored_target_slots;
};

[[nodiscard]] auto candidate_is_eligible(const ContactQuery& query,
                                         Engine::Core::Entity* candidate) -> bool {
  if (candidate == nullptr || candidate == &query.attacker ||
      !Game::Systems::Combat::may_attack(
          &query.attacker_unit,
          candidate,
          {.intent = Game::Systems::Combat::EngagementIntent::Ordered,
           .allow_buildings = false})) {
    return false;
  }
  return std::find(query.ignored_target_ids.begin(),
                   query.ignored_target_ids.end(),
                   candidate->get_id()) == query.ignored_target_ids.end();
}

[[nodiscard]] auto slot_is_ignored(const ContactQuery& query,
                                   Engine::Core::EntityID id,
                                   std::uint16_t slot) -> bool {
  return std::any_of(query.ignored_target_slots.begin(),
                     query.ignored_target_slots.end(),
                     [&](auto const& ignored) {
                       return ignored.entity_id == id && ignored.soldier_slot == slot;
                     });
}

[[nodiscard]] auto sample_is_in_front(const LocalTargetSample& sample) -> bool {
  return sample.entity != nullptr && std::isfinite(sample.forward) &&
         std::isfinite(sample.right) && std::isfinite(sample.distance) &&
         sample.forward > 0.0F;
}

void record_contact(WeaponTraceContact& contact,
                    Engine::Core::EntityID target_id,
                    const LocalTargetSample& sample,
                    const QVector3D& contact_point) {
  contact.target_id = target_id;
  contact.target_soldier_slot = sample.soldier_slot;
  contact.distance = sample.distance;
  contact.local_forward = sample.forward;
  contact.local_right = sample.right;
  contact.contact_point = contact_point;
}

template <typename Scorer>
void scan_for_best_contact(Engine::Core::World& world,
                           const ContactQuery& query,
                           WeaponTraceContact& contact,
                           Scorer&& score_soldier) {
  float best_score = std::numeric_limits<float>::infinity();
  for (auto [candidate, candidate_unit] :
       world.entity_view<Engine::Core::UnitComponent>()) {
    (void)candidate_unit;
    if (!candidate_is_eligible(query, &candidate)) {
      continue;
    }
    float const hint_bias = candidate.get_id() == query.target_hint_id
                                ? hint_score_bias(query.attacker)
                                : 0.0F;

    for (auto const& soldier :
         Game::Systems::RpgCombat::live_soldier_targets(candidate)) {
      if (slot_is_ignored(query, candidate.get_id(), soldier.soldier_slot)) {
        continue;
      }
      auto const sample = make_local_sample(query.presented.frame, soldier);
      if (!sample_is_in_front(sample)) {
        continue;
      }
      score_soldier(candidate, soldier, sample, hint_bias, best_score, contact);
    }
  }
}

[[nodiscard]] auto swept_tip_speed(Engine::Core::Entity& attacker,
                                   const WeaponTraceSegment& segment,
                                   WeaponTraceTimeSpan time_span) -> float {
  auto const* running_action =
      attacker.get_component<Engine::Core::RpgCommanderActionComponent>();
  float const action_seconds = running_action != nullptr
                                   ? std::max(0.001F, running_action->action_duration)
                                   : 1.0F;
  float const slice_seconds = std::max(
      0.001F,
      (time_span.current_normalized_time - time_span.previous_normalized_time) *
          action_seconds);
  QVector3D const tip_travel = segment.current_tip - segment.previous_tip;
  return tip_travel.length() / slice_seconds;
}

[[nodiscard]] auto
find_contact_in_slices(Engine::Core::World& world,
                       Engine::Core::Entity& attacker,
                       const CombatActionDefinition& definition,
                       WeaponTraceTimeSpan time_span,
                       Engine::Core::EntityID target_hint_id,
                       std::span<const Engine::Core::EntityID> ignored_target_ids,
                       std::span<const WeaponTraceIgnoredTarget> ignored_target_slots)
    -> WeaponTraceContact {
  float const trace_span =
      time_span.current_normalized_time - time_span.previous_normalized_time;
  int const sample_count =
      std::max(1, static_cast<int>(std::ceil(trace_span / k_max_trace_sample_span)));
  for (int sample = 0; sample < sample_count; ++sample) {
    float const sample_start =
        time_span.previous_normalized_time +
        trace_span * (static_cast<float>(sample) / static_cast<float>(sample_count));
    float const sample_end = time_span.previous_normalized_time +
                             trace_span * (static_cast<float>(sample + 1) /
                                           static_cast<float>(sample_count));
    auto contact = find_weapon_trace_contact(world,
                                             attacker,
                                             definition,
                                             {.previous_normalized_time = sample_start,
                                              .current_normalized_time = sample_end},
                                             target_hint_id,
                                             ignored_target_ids,
                                             ignored_target_slots);
    if (contact.target_id != 0) {
      return contact;
    }
  }
  return {};
}

void scan_swept_contact(Engine::Core::World& world,
                        const ContactQuery& query,
                        const WeaponTraceSegment& segment,
                        float tip_speed,
                        WeaponTraceContact& contact) {
  scan_for_best_contact(
      world,
      query,
      contact,
      [&](Engine::Core::Entity& candidate,
          const Game::Systems::RpgCombat::SoldierTarget& soldier,
          const LocalTargetSample& sample,
          float hint_bias,
          float& best_score,
          WeaponTraceContact& best_contact) {
        auto const distance = best_segment_distance_xz(segment, sample.world_position);
        float const hit_radius = segment.radius + sample.radius;
        if (!std::isfinite(distance.distance) || distance.distance > hit_radius) {
          return;
        }
        float const score = distance.distance + sample.distance * 0.03F - hint_bias;
        if (score >= best_score) {
          return;
        }
        best_score = score;
        record_contact(
            best_contact,
            candidate.get_id(),
            sample,
            Game::Systems::RpgCombat::hurt_body_contact_point(soldier, distance.point));
        best_contact.contact_speed = tip_speed;
      });
}

} // namespace

auto sample_authored_weapon_trace_segment(Engine::Core::Entity& attacker,
                                          const CombatActionDefinition& definition,
                                          WeaponTraceTimeSpan time_span)
    -> WeaponTraceSegment {
  return sample_segment_in_frame(attacker_frame(attacker),
                                 definition,
                                 live_intent_of(attacker, definition),
                                 time_span);
}

auto find_weapon_trace_contact(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    const CombatActionDefinition& definition,
    WeaponTraceTimeSpan time_span,
    Engine::Core::EntityID target_hint_id,
    std::span<const Engine::Core::EntityID> ignored_target_ids,
    std::span<const WeaponTraceIgnoredTarget> ignored_target_slots)
    -> WeaponTraceContact {
  Engine::Core::Timing::ScopedAccumulator const scope(
      Engine::Core::Timing::commander_weapon_trace());
  float const trace_span =
      time_span.current_normalized_time - time_span.previous_normalized_time;
  if (trace_span > k_max_trace_sample_span) {
    return find_contact_in_slices(world,
                                  attacker,
                                  definition,
                                  time_span,
                                  target_hint_id,
                                  ignored_target_ids,
                                  ignored_target_slots);
  }

  auto const presented_attacker = presented_attacker_frame(attacker, target_hint_id);
  auto const segment = sample_segment_in_frame(presented_attacker.frame,
                                               definition,
                                               live_intent_of(attacker, definition),
                                               time_span);
  if (!segment.valid) {
    return find_weapon_trace_contact(world,
                                     attacker,
                                     definition,
                                     target_hint_id,
                                     ignored_target_ids,
                                     ignored_target_slots);
  }

  WeaponTraceContact contact;
  contact.attacker_id = attacker.get_id();
  contact.attacker_soldier_slot = presented_attacker.soldier_slot;
  auto const* attacker_unit = attacker.get_component<Engine::Core::UnitComponent>();
  if (attacker_unit == nullptr) {
    return contact;
  }

  ContactQuery const query{attacker,
                           *attacker_unit,
                           presented_attacker,
                           target_hint_id,
                           ignored_target_ids,
                           ignored_target_slots};
  scan_swept_contact(
      world, query, segment, swept_tip_speed(attacker, segment, time_span), contact);
  return contact;
}

auto find_weapon_trace_contact(
    Engine::Core::World& world,
    Engine::Core::Entity& attacker,
    const CombatActionDefinition& definition,
    Engine::Core::EntityID target_hint_id,
    std::span<const Engine::Core::EntityID> ignored_target_ids,
    std::span<const WeaponTraceIgnoredTarget> ignored_target_slots)
    -> WeaponTraceContact {
  Engine::Core::Timing::ScopedAccumulator const scope(
      Engine::Core::Timing::commander_weapon_trace());
  WeaponTraceContact contact;
  contact.attacker_id = attacker.get_id();

  auto const* attacker_unit = attacker.get_component<Engine::Core::UnitComponent>();
  if (attacker_unit == nullptr || !is_melee_weapon(definition)) {
    return contact;
  }
  auto const presented_attacker = presented_attacker_frame(attacker, target_hint_id);
  contact.attacker_soldier_slot = presented_attacker.soldier_slot;
  auto const intent = live_intent_of(attacker, definition);

  ContactQuery const query{attacker,
                           *attacker_unit,
                           presented_attacker,
                           target_hint_id,
                           ignored_target_ids,
                           ignored_target_slots};
  scan_for_best_contact(
      world,
      query,
      contact,
      [&](Engine::Core::Entity& candidate,
          const Game::Systems::RpgCombat::SoldierTarget& soldier,
          const LocalTargetSample& sample,
          float hint_bias,
          float& best_score,
          WeaponTraceContact& best_contact) {
        float const score =
            weapon_contact_score(sample, definition, intent) - hint_bias;
        if (!std::isfinite(score) || score >= best_score) {
          return;
        }
        best_score = score;
        record_contact(best_contact,
                       candidate.get_id(),
                       sample,
                       Game::Systems::RpgCombat::hurt_body_contact_point(
                           soldier, presented_attacker.frame.origin));
      });
  return contact;
}

} // namespace Game::Systems::CombatActions
