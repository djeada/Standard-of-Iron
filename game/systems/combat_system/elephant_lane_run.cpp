#include "elephant_lane_run.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../../formation/army_formation_registry.h"
#include "../../formation/formation_frame.h"

namespace Game::Systems::Combat {

namespace {

using Game::Formation::ArmyFormation;
using Game::Formation::BattleBand;

constexpr float k_min_elephant_radius = 1.5F;
constexpr float k_lane_side_margin = 0.25F;
constexpr float k_lane_mouth_tolerance = 0.75F;
constexpr float k_lane_exit_margin = 4.0F;
constexpr float k_requeue_distance = 0.5F;

struct Frame {
  QVector3D origin;
  QVector3D forward;
  QVector3D lateral;

  [[nodiscard]] auto to_local(const QVector3D& world) const -> QVector3D {
    QVector3D const d(world.x() - origin.x(), 0.0F, world.z() - origin.z());
    return {QVector3D::dotProduct(d, lateral), 0.0F, QVector3D::dotProduct(d, forward)};
  }
  [[nodiscard]] auto to_world(float x, float z) const -> QVector3D {
    QVector3D const p = origin + lateral * x + forward * z;
    return {p.x(), 0.0F, p.z()};
  }
};

struct Block {
  float x{0.0F};
  float z{0.0F};
  float half_width{0.0F};
  float half_depth{0.0F};
  BattleBand band{BattleBand::None};
};

auto is_line_band(BattleBand band) -> bool {
  return band == BattleBand::Hastati || band == BattleBand::Principes ||
         band == BattleBand::Triarii;
}

auto formation_of(Engine::Core::World& world,
                  const Engine::Core::Entity& member) -> const ArmyFormation* {
  const auto* membership =
      member.get_component<Engine::Core::ArmyFormationMembershipComponent>();
  if (membership == nullptr || !membership->is_valid()) {
    return nullptr;
  }
  const auto* formation = Game::Formation::ArmyFormationRegistry::for_world(world).find(
      membership->group_id);
  if (formation == nullptr || !formation->has_member(member.get_id())) {
    return nullptr;
  }
  return formation;
}

auto elephant_radius(const Engine::Core::Entity& elephant) -> float {
  const auto* movement = elephant.get_component<Engine::Core::MovementComponent>();
  float const clearance =
      movement != nullptr ? movement->get_navigation_clearance() : 0.0F;
  return std::max(k_min_elephant_radius, clearance);
}

struct Lane {
  float x{0.0F};
  float half_width{0.0F};
};

auto clear_lanes(const std::vector<Block>& blocks, float radius) -> std::vector<Lane> {
  // The lanes are the gaps of the front-most line that run clear through
  // every line behind it.
  float front_mean = -std::numeric_limits<float>::max();
  BattleBand front_band = BattleBand::None;
  for (BattleBand band :
       {BattleBand::Hastati, BattleBand::Principes, BattleBand::Triarii}) {
    float sum = 0.0F;
    int count = 0;
    for (const auto& block : blocks) {
      if (block.band == band) {
        sum += block.z;
        ++count;
      }
    }
    if (count > 0 && sum / static_cast<float>(count) > front_mean) {
      front_mean = sum / static_cast<float>(count);
      front_band = band;
    }
  }
  std::vector<Block> front;
  for (const auto& block : blocks) {
    if (block.band == front_band) {
      front.push_back(block);
    }
  }
  std::sort(front.begin(), front.end(), [](const Block& a, const Block& b) {
    return a.x < b.x;
  });
  std::vector<Lane> lanes;
  for (std::size_t i = 1; i < front.size(); ++i) {
    float const lo = front[i - 1U].x + front[i - 1U].half_width;
    float const hi = front[i].x - front[i].half_width;
    if (hi - lo < 2.0F * (radius + k_lane_side_margin)) {
      continue;
    }
    float const centre = (lo + hi) * 0.5F;
    bool clear = true;
    for (const auto& block : blocks) {
      if (block.band == front_band) {
        continue;
      }
      if (std::abs(block.x - centre) < block.half_width + radius + k_lane_side_margin) {
        clear = false;
        break;
      }
    }
    if (clear) {
      lanes.push_back({centre, (hi - lo) * 0.5F});
    }
  }
  return lanes;
}

auto remember(Engine::Core::ElephantComponent& state,
              const QVector3D& goal) -> std::optional<QVector3D> {
  state.lane_goal_x = goal.x();
  state.lane_goal_z = goal.z();
  return goal;
}

} // namespace

auto elephant_lane_goal(Engine::Core::World& world,
                        Engine::Core::Entity& elephant,
                        const Engine::Core::Entity* target)
    -> std::optional<QVector3D> {
  auto* state = elephant.get_component<Engine::Core::ElephantComponent>();
  const auto* transform = elephant.get_component<Engine::Core::TransformComponent>();
  if (state == nullptr || transform == nullptr) {
    return std::nullopt;
  }
  if (state->lane_run_out_seconds > 0.0F) {
    return QVector3D(state->lane_goal_x, 0.0F, state->lane_goal_z);
  }
  const ArmyFormation* formation =
      target != nullptr ? formation_of(world, *target) : nullptr;
  if (formation == nullptr ||
      formation->intent != Game::Formation::ArmyFormationIntent::TriplexAcies) {
    state->lane_running = false;
    return std::nullopt;
  }

  Frame frame;
  frame.origin = QVector3D(formation->anchor.x(), 0.0F, formation->anchor.z());
  frame.forward = Game::Formation::planning::rotate_offset(QVector3D(0.0F, 0.0F, 1.0F),
                                                           formation->facing);
  frame.lateral = Game::Formation::planning::rotate_offset(QVector3D(1.0F, 0.0F, 0.0F),
                                                           formation->facing);

  std::vector<Block> blocks;
  float front = -std::numeric_limits<float>::max();
  float rear = std::numeric_limits<float>::max();
  for (const auto& slot : formation->slot_list) {
    if (!is_line_band(slot.band) || !slot.is_placeable()) {
      continue;
    }
    QVector3D const local = frame.to_local(slot.world_position);
    blocks.push_back(
        {local.x(), local.z(), slot.half_width, slot.half_depth, slot.band});
    front = std::max(front, local.z() + slot.half_depth);
    rear = std::min(rear, local.z() - slot.half_depth);
  }
  if (blocks.size() < 2U) {
    state->lane_running = false;
    return std::nullopt;
  }

  QVector3D const here =
      frame.to_local(QVector3D(transform->position.x, 0.0F, transform->position.z));
  if (here.z() < rear) {
    if (!state->lane_running) {
      return std::nullopt;
    }
    // Through the lines: keep running out the back before turning.
    state->lane_running = false;
    state->lane_run_out_seconds = k_lane_run_out_seconds;
    return remember(*state, frame.to_world(here.x(), rear - k_lane_run_out_distance));
  }
  if (here.z() > front + k_lane_lookahead) {
    state->lane_running = false;
    return std::nullopt;
  }

  float const radius = elephant_radius(elephant);
  auto const lanes = clear_lanes(blocks, radius);
  if (lanes.empty()) {
    state->lane_running = false;
    return std::nullopt;
  }
  // Stay in the lane already chosen; otherwise take the nearest one.
  float const remembered_x =
      state->lane_running
          ? frame.to_local(QVector3D(state->lane_goal_x, 0.0F, state->lane_goal_z)).x()
          : here.x();
  const Lane* best = nullptr;
  for (const auto& lane : lanes) {
    if (best == nullptr ||
        std::abs(lane.x - remembered_x) < std::abs(best->x - remembered_x)) {
      best = &lane;
    }
  }

  bool const inside = here.z() <= front + 0.5F;
  float const off_line = std::abs(here.x() - best->x);
  if (inside && off_line > best->half_width) {
    state->lane_running = false;
    return std::nullopt;
  }
  state->lane_running = true;
  if (!inside && off_line > k_lane_mouth_tolerance) {
    float const lead = std::clamp((here.z() - front) * 0.5F, 4.0F, 12.0F);
    return remember(*state, frame.to_world(best->x, front + lead));
  }
  return remember(*state, frame.to_world(best->x, rear - k_lane_exit_margin));
}

void steer_down_lane(Engine::Core::Entity& elephant,
                     const QVector3D& goal,
                     std::vector<CommandService::MoveIntent>& intents) {
  auto* movement =
      Engine::Core::get_or_add_component<Engine::Core::MovementComponent>(&elephant);
  if (movement == nullptr) {
    return;
  }
  movement->clear_structure_approach_target();
  if (const auto* transform =
          elephant.get_component<Engine::Core::TransformComponent>()) {
    QVector3D const here(transform->position.x, 0.0F, transform->position.z);
    if ((here - goal).lengthSquared() <= 1.0F) {
      return;
    }
  }
  if (movement->get_has_target() || movement->has_waypoints()) {
    QVector3D const planned =
        movement->get_has_requested_goal()
            ? QVector3D(movement->get_requested_goal_x(),
                        0.0F,
                        movement->get_requested_goal_z())
            : QVector3D(movement->get_goal_x(), 0.0F, movement->get_goal_y());
    if ((planned - goal).lengthSquared() <= k_requeue_distance * k_requeue_distance) {
      return;
    }
  }
  intents.push_back({elephant.get_id(), goal});
}

} // namespace Game::Systems::Combat
