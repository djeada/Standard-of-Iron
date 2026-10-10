#include "army_formation_manoeuvre.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>

#include "../core/ambient_session.h"
#include "../core/component_combat.h"
#include "../core/component_core.h"
#include "../core/component_gameplay.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../systems/owner_registry.h"
#include "formation_frame.h"

namespace Game::Formation::Manoeuvre {

namespace {

constexpr float k_wheel_degrees = 90.0F;
constexpr float k_redispatch_metres = 0.75F;
constexpr float k_give_ground_dead_band = 0.4F;
constexpr float k_min_lane_pitch = 0.5F;

struct WheelPivots {
  bool has_left{false};
  bool has_right{false};
  QVector3D left;
  QVector3D right;
};

struct Frame {
  WheelPivots pivots;
  float pitch{0.0F};
  bool has_rear{false};
  float rear{0.0F};
};

constexpr float k_screen_fallback_gap = 4.0F;

auto wheel_pivots(const ArmyFormation& formation) -> WheelPivots {
  WheelPivots pivots;
  for (const auto& slot : formation.slot_list) {
    if (slot.band != BattleBand::CrescentWing) {
      continue;
    }
    float const x = slot.local_offset.x();
    float const front = slot.local_offset.z() + slot.half_depth;
    if (x < 0.0F) {
      float const inner = x + slot.half_width;
      pivots.left = pivots.has_left ? QVector3D(std::max(pivots.left.x(), inner),
                                                0.0F,
                                                std::max(pivots.left.z(), front))
                                    : QVector3D(inner, 0.0F, front);
      pivots.has_left = true;
    } else {
      float const inner = x - slot.half_width;
      pivots.right = pivots.has_right ? QVector3D(std::min(pivots.right.x(), inner),
                                                  0.0F,
                                                  std::max(pivots.right.z(), front))
                                      : QVector3D(inner, 0.0F, front);
      pivots.has_right = true;
    }
  }
  return pivots;
}

auto frame_of(const ArmyFormation& formation) -> Frame {
  Frame frame;
  if (formation.intent == ArmyFormationIntent::ConvexCrescent) {
    frame.pivots = wheel_pivots(formation);
  }
  if (formation.intent == ArmyFormationIntent::TriplexAcies) {
    frame.pitch = lane_pitch(formation);
    for (const auto& slot : formation.slot_list) {
      if (slot.band == BattleBand::Hastati || slot.band == BattleBand::Principes ||
          slot.band == BattleBand::Triarii) {
        float const rear = slot.local_offset.z() - slot.half_depth;
        frame.rear = frame.has_rear ? std::min(frame.rear, rear) : rear;
        frame.has_rear = true;
      }
    }
  }
  return frame;
}

auto manoeuvre_with(const ArmyFormation& formation,
                    const FormationSlot& slot,
                    const Frame& frame) -> SlotManoeuvre {
  SlotManoeuvre out;
  auto const& state = formation.manoeuvre;
  switch (slot.band) {
  case BattleBand::CrescentCentre:
    out.offset = QVector3D(0.0F, 0.0F, -slot.yield_depth * state.centre_yield);
    out.facing = -2.0F * slot.local_facing * state.centre_yield;
    break;
  case BattleBand::CrescentWing: {
    float const turn = k_wheel_degrees * state.wing_wheel;
    if (turn <= 0.0F) {
      break;
    }
    bool const left = slot.local_offset.x() < 0.0F;
    if ((left && !frame.pivots.has_left) || (!left && !frame.pivots.has_right)) {
      break;
    }
    QVector3D const pivot = left ? frame.pivots.left : frame.pivots.right;
    QVector3D const from_pivot = slot.local_offset - pivot;
    float const signed_turn = left ? turn : -turn;
    QVector3D const swung = planning::rotate_offset(from_pivot, signed_turn);
    out.offset =
        QVector3D(swung.x() - from_pivot.x(), 0.0F, swung.z() - from_pivot.z());
    out.facing = signed_turn;
    break;
  }
  case BattleBand::Principes:
    out.offset = QVector3D(frame.pitch * 0.5F * state.lane_shift, 0.0F, 0.0F);
    break;
  case BattleBand::Screen:
    // The skirmishers fall back through the lanes, out of the elephants' way.
    if (formation.intent == ArmyFormationIntent::TriplexAcies && frame.has_rear) {
      float const behind = frame.rear - k_screen_fallback_gap - slot.half_depth;
      out.offset =
          QVector3D(0.0F, 0.0F, (behind - slot.local_offset.z()) * state.lane_shift);
    }
    break;
  default:
    break;
  }
  return out;
}

auto owner_of(Engine::Core::World& world, const ArmyFormation& formation) -> int {
  for (auto const member : formation.members) {
    if (const auto* unit = world.try_get<Engine::Core::UnitComponent>(member)) {
      return unit->owner_id;
    }
  }
  return 0;
}

auto ground(const Engine::Core::TransformComponent& transform) -> QVector3D {
  return {transform.position.x, 0.0F, transform.position.z};
}

void advance_crescent(Engine::Core::World& world,
                      ArmyFormation& formation,
                      float elapsed) {
  std::unordered_set<EntityID> centre;
  for (const auto& slot : formation.slot_list) {
    if (slot.band == BattleBand::CrescentCentre && slot.occupant != 0U &&
        formation.has_member(slot.occupant)) {
      centre.insert(slot.occupant);
    }
  }
  auto& state = formation.manoeuvre;
  if (centre.empty()) {
    state.yielding = false;
    return;
  }
  int engaged = 0;
  for (auto const member : centre) {
    const auto* attack = world.try_get<Engine::Core::AttackComponent>(member);
    if (attack != nullptr && attack->in_melee_lock) {
      ++engaged;
    }
  }
  auto const centre_count = static_cast<float>(centre.size());
  bool const pressed = static_cast<float>(engaged) >= k_pressure_share * centre_count;
  state.yielding = pressed && state.centre_yield < 1.0F;
  if (pressed) {
    int attackers = 0;
    for (auto const id : world.entities_with<Engine::Core::AttackComponent>()) {
      const auto* attack = world.try_get<Engine::Core::AttackComponent>(id);
      if (attack != nullptr && attack->in_melee_lock &&
          centre.count(attack->melee_lock_target_id) != 0U && centre.count(id) == 0U) {
        ++attackers;
      }
    }
    // Outnumbered, the centre gives ground faster; holding its own, slower.
    float const odds = static_cast<float>(attackers) / centre_count;
    float const pace = std::clamp(0.5F + odds, 0.5F, 2.0F);
    state.centre_yield =
        std::min(1.0F, state.centre_yield + pace * elapsed / k_yield_seconds);
  }
  if (state.centre_yield >= k_wheel_trigger) {
    state.wheel_ordered = true;
  }
  if (state.wheel_ordered) {
    state.wing_wheel = std::min(1.0F, state.wing_wheel + elapsed / k_wheel_seconds);
  }
}

auto enemy_elephant_near(Engine::Core::World& world,
                         const ArmyFormation& formation) -> bool {
  int const owner = owner_of(world, formation);
  auto const* owners = Game::Session::services_for(world).owners;
  if (owners == nullptr) {
    return false;
  }
  float const reach_sq = k_lane_alarm_distance * k_lane_alarm_distance;
  for (auto const id : world.entities_with<Engine::Core::ElephantComponent>()) {
    const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
    const auto* transform = world.try_get<Engine::Core::TransformComponent>(id);
    if (unit == nullptr || transform == nullptr || unit->health <= 0 ||
        !owners->are_enemies(owner, unit->owner_id)) {
      continue;
    }
    QVector3D const at = ground(*transform);
    for (const auto& slot : formation.slot_list) {
      if (slot.band != BattleBand::Hastati && slot.band != BattleBand::Principes &&
          slot.band != BattleBand::Triarii) {
        continue;
      }
      QVector3D const delta(
          slot.world_position.x() - at.x(), 0.0F, slot.world_position.z() - at.z());
      if (delta.lengthSquared() <= reach_sq) {
        return true;
      }
    }
  }
  return false;
}

void advance_triplex(Engine::Core::World& world,
                     ArmyFormation& formation,
                     float elapsed) {
  auto& state = formation.manoeuvre;
  if (!state.lanes_opened && enemy_elephant_near(world, formation)) {
    state.lanes_opened = true;
  }
  if (state.lanes_opened) {
    state.lane_shift =
        std::min(1.0F, state.lane_shift + elapsed / k_lane_shift_seconds);
  }
}

auto apply_offsets(ArmyFormation& formation) -> float {
  auto const frame = frame_of(formation);
  float moved = 0.0F;
  for (auto& slot : formation.slot_list) {
    auto const next = manoeuvre_with(formation, slot, frame);
    QVector3D const delta_local = next.offset - slot.manoeuvre_offset;
    float const delta_facing = next.facing - slot.manoeuvre_facing;
    if (delta_local.lengthSquared() < 1.0e-8F && std::abs(delta_facing) < 1.0e-4F) {
      continue;
    }
    QVector3D const delta_world =
        planning::rotate_offset(delta_local, formation.facing);
    slot.world_position += QVector3D(delta_world.x(), 0.0F, delta_world.z());
    slot.facing += delta_facing;
    slot.manoeuvre_offset = next.offset;
    slot.manoeuvre_facing = next.facing;
    moved = std::max(moved, delta_local.length());
  }
  return moved;
}

} // namespace

auto lane_pitch(const ArmyFormation& formation) -> float {
  std::vector<float> xs;
  for (const auto& slot : formation.slot_list) {
    if (slot.band == BattleBand::Hastati) {
      xs.push_back(slot.local_offset.x());
    }
  }
  if (xs.size() < 2U) {
    return 0.0F;
  }
  std::sort(xs.begin(), xs.end());
  float pitch = 0.0F;
  for (std::size_t i = 1; i < xs.size(); ++i) {
    float const step = xs[i] - xs[i - 1U];
    if (step > k_min_lane_pitch && (pitch <= 0.0F || step < pitch)) {
      pitch = step;
    }
  }
  return pitch;
}

auto slot_manoeuvre(const ArmyFormation& formation,
                    const FormationSlot& slot) -> SlotManoeuvre {
  return manoeuvre_with(formation, slot, frame_of(formation));
}

void update(Engine::Core::World& world, ArmyFormation& formation, float elapsed) {
  if (!is_battle_order_intent(formation.intent) || formation.morph.active ||
      elapsed <= 0.0F) {
    return;
  }
  switch (formation.intent) {
  case ArmyFormationIntent::ConvexCrescent:
    advance_crescent(world, formation, elapsed);
    break;
  case ArmyFormationIntent::TriplexAcies:
    advance_triplex(world, formation, elapsed);
    break;
  default:
    return;
  }
  auto& state = formation.manoeuvre;
  state.drift_since_dispatch += apply_offsets(formation);
  if (state.drift_since_dispatch >= k_redispatch_metres) {
    state.drift_since_dispatch = 0.0F;
    formation.moves_pending = true;
  }
}

void reapply(ArmyFormation& formation) {
  if (!is_battle_order_intent(formation.intent)) {
    return;
  }
  for (auto& slot : formation.slot_list) {
    slot.manoeuvre_offset = QVector3D();
    slot.manoeuvre_facing = 0.0F;
  }
  static_cast<void>(apply_offsets(formation));
}

auto holds_for_manoeuvre(const ArmyFormation& formation, EntityID entity) -> bool {
  if (formation.intent != ArmyFormationIntent::ConvexCrescent ||
      formation.manoeuvre.wheel_ordered) {
    return false;
  }
  const auto* slot = formation.find_slot_for(entity);
  return slot != nullptr && slot->band == BattleBand::CrescentWing;
}

auto give_ground_velocity(const ArmyFormation& formation,
                          const Engine::Core::Entity& entity) -> QVector3D {
  if (formation.intent != ArmyFormationIntent::ConvexCrescent ||
      !formation.manoeuvre.yielding || formation.morph.active) {
    return {};
  }
  const auto* slot = formation.find_slot_for(entity.get_id());
  if (slot == nullptr || slot->band != BattleBand::CrescentCentre) {
    return {};
  }
  const auto* registry = entity.registry();
  const auto* transform =
      registry != nullptr
          ? registry->try_get<Engine::Core::TransformComponent>(entity.get_id())
          : nullptr;
  if (transform == nullptr) {
    return {};
  }
  QVector3D const forward =
      planning::rotate_offset(QVector3D(0.0F, 0.0F, 1.0F), formation.facing);
  QVector3D const to_slot = slot->world_position - ground(*transform);
  float const ahead = -QVector3D::dotProduct(QVector3D(to_slot.x(), 0.0F, to_slot.z()),
                                             QVector3D(forward.x(), 0.0F, forward.z()));
  if (ahead <= k_give_ground_dead_band) {
    return {};
  }
  float const speed = std::min(k_give_ground_speed, ahead);
  return QVector3D(-forward.x() * speed, 0.0F, -forward.z() * speed);
}

} // namespace Game::Formation::Manoeuvre
