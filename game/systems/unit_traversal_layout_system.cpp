#include "unit_traversal_layout_system.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <utility>

#include "../core/component_gameplay.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../formation/traversal_layout_policy.h"
#include "../formation/unit_layout.h"
#include "../util/planar_math.h"
#include "formation_combat_geometry.h"
#include "nav_grid.h"
#include "pathfinding.h"

namespace Game::Systems {
namespace {

constexpr float k_probe_step_cells = 0.2F;
constexpr float k_width_epsilon = 0.05F;
constexpr float k_exit_width_margin = 0.12F;
constexpr float k_enter_dwell_seconds = 0.12F;
constexpr float k_minimum_mode_dwell_seconds = 0.50F;
constexpr float k_tail_clear_seconds = 0.35F;
constexpr float k_transition_seconds = 0.65F;
constexpr float k_narrow_rate = 8.0F;
constexpr float k_reform_rate = 3.5F;
constexpr float k_infantry_relocation_speed = 1.8F;
constexpr float k_infantry_relocation_acceleration = 7.2F;
constexpr float k_mounted_relocation_speed = 2.25F;
constexpr float k_mounted_relocation_acceleration = 9.0F;
constexpr float k_heavy_relocation_speed = 1.2F;
constexpr float k_heavy_relocation_acceleration = 4.8F;
constexpr float k_entry_alignment_cosine = 0.94F;

constexpr float k_minimum_squeeze_scale = 0.12F;

constexpr float k_soldier_probe_reach = 0.88F;

constexpr float k_minimum_presented_separation = 0.15F;

constexpr std::uint64_t k_moving_width_measure_ticks = 4;
constexpr std::uint64_t k_resting_width_measure_ticks = 16;

constexpr int k_slot_recovery_bisections = 8;
constexpr float k_ground_recovery_speed = 4.5F;
constexpr float k_ground_recovery_acceleration = 18.0F;
constexpr float k_ground_clamp_speed = 14.0F;

struct WidthMeasurement {
  float available_half_width{0.0F};
  float desired_half_width{0.0F};
  bool constrained{false};
};

struct RelocationLimits {
  float speed{0.0F};
  float acceleration{0.0F};
};

auto relocation_limits(Game::Units::SpawnType type) -> RelocationLimits {
  using Type = Game::Units::SpawnType;
  switch (type) {
  case Type::MountedKnight:
  case Type::HorseArcher:
  case Type::HorseSpearman:
    return {k_mounted_relocation_speed, k_mounted_relocation_acceleration};
  case Type::Elephant:
  case Type::Catapult:
  case Type::Ballista:
    return {k_heavy_relocation_speed, k_heavy_relocation_acceleration};
  default:
    return {k_infantry_relocation_speed, k_infantry_relocation_acceleration};
  }
}

auto smoothstep(float value) noexcept -> float {
  float const t = std::clamp(value, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

auto mode_for_files(std::uint32_t files,
                    std::uint32_t normal_files) -> Engine::Core::TraversalLayoutMode {
  using Mode = Engine::Core::TraversalLayoutMode;
  if (files >= normal_files) {
    return Mode::Normal;
  }
  if (files == 1U) {
    return Mode::SingleFile;
  }
  if (files <= 2U) {
    return Mode::MarchingOrder;
  }
  return Mode::NarrowRanks;
}

void rebuild_stable_mapping(const FormationCombat::FormationLayout& layout,
                            Engine::Core::UnitTraversalLayoutStateComponent& state) {
  if (state.stable_slot_mapping.size() == layout.all_slots.size() &&
      std::all_of(
          layout.all_slots.begin(), layout.all_slots.end(), [&state](auto const& slot) {
            return std::find(state.stable_slot_mapping.begin(),
                             state.stable_slot_mapping.end(),
                             slot.index) != state.stable_slot_mapping.end();
          })) {
    return;
  }

  auto ordered = layout.all_slots;
  std::stable_sort(
      ordered.begin(), ordered.end(), [](auto const& left, auto const& right) {
        if (left.local_z != right.local_z) {
          return left.local_z > right.local_z;
        }
        if (left.local_x != right.local_x) {
          return left.local_x < right.local_x;
        }
        return left.index < right.index;
      });
  state.stable_slot_mapping.clear();
  state.stable_slot_mapping.reserve(ordered.size());
  for (auto const& slot : ordered) {
    state.stable_slot_mapping.push_back(slot.index);
  }
}

auto normal_file_count(const FormationCombat::FormationLayout& layout)
    -> std::uint32_t {
  std::uint32_t files = 1U;
  for (auto const& slot : layout.all_slots) {
    auto const in_rank = static_cast<std::uint32_t>(std::count_if(
        layout.all_slots.begin(),
        layout.all_slots.end(),
        [&slot](auto const& candidate) { return candidate.row == slot.row; }));
    files = std::max(files, in_rank);
  }
  return files;
}

auto measure_width(const Engine::Core::Entity& entity,
                   const Engine::Core::TransformComponent& transform,
                   const Engine::Core::MovementComponent& movement,
                   const Engine::Core::MovementFactsComponent& facts,
                   const FormationCombat::FormationLayout& layout) -> WidthMeasurement {
  WidthMeasurement result;
  result.desired_half_width = FormationCombat::formation_navigation_clearance(entity);
  result.available_half_width = result.desired_half_width;
  auto* pathfinder = NavGrid::get_pathfinder();
  if (pathfinder == nullptr || result.desired_half_width <= k_width_epsilon) {
    return result;
  }

  pathfinder->update_navigation_grid();
  auto const passability = movement.get_can_enter_forest()
                               ? Pathfinding::Passability::Light
                               : Pathfinding::Passability::Heavy;
  float tangent_x = facts.desired.tangent_x;
  float tangent_z = facts.desired.tangent_z;
  float tangent_length = Game::Systems::planar_length(tangent_x, tangent_z);
  if (!facts.desired.valid || tangent_length < 0.001F) {
    tangent_x = facts.motor.accepted_vx;
    tangent_z = facts.motor.accepted_vz;
    tangent_length = Game::Systems::planar_length(tangent_x, tangent_z);
  }
  if (tangent_length < 0.001F) {
    float const yaw = transform.rotation.y * std::numbers::pi_v<float> / 180.0F;
    tangent_x = std::sin(yaw);
    tangent_z = std::cos(yaw);
    tangent_length = 1.0F;
  }
  QVector3D const forward(tangent_x / tangent_length, 0.0F, tangent_z / tangent_length);
  QVector3D const root(transform.position.x, 0.0F, transform.position.z);

  float longitudinal_half_extent = layout.body_radius;
  for (auto const& slot : layout.all_slots) {
    longitudinal_half_extent =
        std::max(longitudinal_half_extent, std::abs(slot.local_z) + layout.body_radius);
  }
  float const accepted_speed =
      Game::Systems::planar_length(facts.motor.accepted_vx, facts.motor.accepted_vz);

  float const maximum_column_half_depth =
      0.5F *
      static_cast<float>(layout.all_slots.size() > 1U ? layout.all_slots.size() - 1U
                                                      : 0U) *
      Game::Formation::TraversalPolicy::compact_spacing(layout.body_radius,
                                                        layout.spacing);
  float const route_speed =
      std::max(accepted_speed, facts.desired.valid ? facts.desired.speed_limit : 0.0F);
  float const relocation_warning_distance =
      route_speed * maximum_column_half_depth / k_infantry_relocation_speed;
  float const lookahead =
      std::max(longitudinal_half_extent + maximum_column_half_depth +
                   relocation_warning_distance,
               route_speed * k_transition_seconds);
  float const cell_size = std::max(0.1F, pathfinder->grid_cell_size());
  float const lateral_step = cell_size * k_probe_step_cells;
  float const longitudinal_step = cell_size * 0.5F;

  auto available_on_side =
      [&](QVector3D const& center, QVector3D const& lateral, float side) {
        float previous = 0.0F;
        for (float distance = lateral_step;
             distance <= result.desired_half_width + lateral_step;
             distance += lateral_step) {
          float const clamped = std::min(distance, result.desired_half_width);
          if (!pathfinder->is_world_position_walkable(
                  center + lateral * (clamped * side), passability)) {
            return previous;
          }
          previous = clamped;
          if (clamped >= result.desired_half_width) {
            break;
          }
        }
        return result.desired_half_width;
      };

  float const envelope_start = -longitudinal_half_extent;
  float const envelope_end = longitudinal_half_extent + lookahead;
  auto const& path = movement.get_path();
  bool const has_route = movement.has_waypoints();

  std::size_t walk_index = movement.get_path_index();
  QVector3D walk_cursor = root;
  QVector3D walk_direction = forward;
  float walk_consumed = 0.0F;

  auto route_sample = [&](float distance) {
    if (distance <= 0.0F || !has_route) {
      return std::pair(root + forward * distance, forward);
    }
    float remaining = distance - walk_consumed;
    while (walk_index < path.size()) {
      QVector3D const waypoint(path[walk_index].first, 0.0F, path[walk_index].second);
      QVector3D const segment = waypoint - walk_cursor;
      float const length = segment.length();
      if (length < 0.001F) {
        walk_cursor = waypoint;
        ++walk_index;
        continue;
      }
      walk_direction = segment / length;
      if (remaining <= length) {
        return std::pair(walk_cursor + walk_direction * remaining, walk_direction);
      }
      remaining -= length;
      walk_consumed += length;
      walk_cursor = waypoint;
      ++walk_index;
    }
    return std::pair(walk_cursor + walk_direction * remaining, walk_direction);
  };
  int const samples = std::max(
      1,
      static_cast<int>(std::ceil((envelope_end - envelope_start) / longitudinal_step)));
  for (int sample = 0; sample <= samples; ++sample) {
    float const t = static_cast<float>(sample) / static_cast<float>(samples);
    float const offset = envelope_start + (envelope_end - envelope_start) * t;
    auto const [center, sample_forward] = route_sample(offset);
    if (!pathfinder->is_world_position_walkable(center, passability)) {
      continue;
    }
    QVector3D const sample_lateral(sample_forward.z(), 0.0F, -sample_forward.x());
    float const left = available_on_side(center, sample_lateral, -1.0F);
    float const right = available_on_side(center, sample_lateral, 1.0F);
    if (left + k_width_epsilon >= result.desired_half_width ||
        right + k_width_epsilon >= result.desired_half_width) {
      continue;
    }
    result.constrained = true;
    result.available_half_width = std::min({result.available_half_width, left, right});
  }
  return result;
}

void publish_facts(const Engine::Core::UnitTraversalLayoutStateComponent& state,
                   Engine::Core::TraversalLayoutFacts& facts) {
  facts.mode = state.mode;
  facts.target_mode = state.target_mode;
  facts.portal_id = state.portal_id;
  facts.current_files = state.current_files;
  facts.target_files = state.target_files;
  facts.corridor_half_width = state.available_half_width;
  facts.desired_half_width = state.desired_half_width;
  facts.soldier_body_radius = state.soldier_body_radius;
  facts.transition_progress = state.transition_curve;
  facts.mode_dwell_seconds = state.mode_dwell_seconds;
}

void update_slot_states(const Engine::Core::TransformComponent& transform,
                        const Engine::Core::MovementComponent& movement,
                        const Engine::Core::MovementFactsComponent& facts,
                        const Engine::Core::UnitComponent& unit,
                        const FormationCombat::FormationLayout& layout,
                        bool reset_transition,
                        float step,
                        Engine::Core::UnitTraversalLayoutStateComponent& state) {
  std::vector<Engine::Core::UnitTraversalSlotState> next;
  next.reserve(layout.all_slots.size());

  float const corridor_half_width = state.available_half_width -
                                    (layout.body_radius * k_soldier_probe_reach) -
                                    k_width_epsilon;
  for (auto const& slot : layout.all_slots) {
    Engine::Core::UnitTraversalSlotState target;
    if (auto const* previous = state.slot_for(slot.index)) {
      target = *previous;
    } else {
      target.slot_index = slot.index;
      target.start_local_x = slot.local_x;
      target.start_local_z = slot.local_z;
      target.previous_local_x = slot.local_x;
      target.previous_local_z = slot.local_z;
      target.current_local_x = slot.local_x;
      target.current_local_z = slot.local_z;
    }
    target.row = slot.row;
    target.col = slot.col;
    target.alive = std::any_of(
        layout.live_slots.begin(),
        layout.live_slots.end(),
        [&slot](auto const& candidate) { return candidate.index == slot.index; });
    target.target_local_x = slot.local_x;
    target.target_local_z = slot.local_z;
    if (state.active) {

      target.target_local_x *= state.lateral_scale;
    }
    if (reset_transition) {
      target.start_local_x = target.current_local_x;
      target.start_local_z = target.current_local_z;
    }
    next.push_back(target);
  }

  auto* pathfinder = NavGrid::get_pathfinder();
  auto const passability = movement.get_can_enter_forest()
                               ? Pathfinding::Passability::Light
                               : Pathfinding::Passability::Heavy;
  float const yaw = transform.rotation.y * std::numbers::pi_v<float> / 180.0F;
  float const sin_yaw = std::sin(yaw);
  float const cos_yaw = std::cos(yaw);
  float const clearance = layout.body_radius * 0.88F;
  auto walkability_score =
      [&](float local_x, float local_z, float root_offset_x, float root_offset_z) {
        if (pathfinder == nullptr) {
          return 9;
        }
        float const world_x = transform.position.x + root_offset_x + cos_yaw * local_x +
                              sin_yaw * local_z;
        float const world_z = transform.position.z + root_offset_z - sin_yaw * local_x +
                              cos_yaw * local_z;
        constexpr std::array<std::pair<float, float>, 9> k_probes{{
            {0.0F, 0.0F},
            {1.0F, 0.0F},
            {-1.0F, 0.0F},
            {0.0F, 1.0F},
            {0.0F, -1.0F},
            {0.707F, 0.707F},
            {-0.707F, 0.707F},
            {0.707F, -0.707F},
            {-0.707F, -0.707F},
        }};
        return static_cast<int>(
            std::count_if(k_probes.begin(), k_probes.end(), [&](auto const& probe) {
              return pathfinder->is_world_position_walkable(
                  QVector3D(world_x + probe.first * clearance,
                            0.0F,
                            world_z + probe.second * clearance),
                  passability);
            }));
      };

  auto const limits = relocation_limits(unit.spawn_type);

  if (state.active && corridor_half_width > 0.0F) {
    float widest = 0.0F;
    for (auto const& slot : next) {
      if (slot.alive) {
        widest = std::max(widest, std::abs(slot.target_local_x));
      }
    }
    float const floor_correction =
        state.file_spacing > 0.01F
            ? std::min(1.0F, k_minimum_presented_separation / state.file_spacing)
            : k_minimum_squeeze_scale;
    if (widest > corridor_half_width) {
      float const correction = std::max(corridor_half_width / widest, floor_correction);
      for (auto& slot : next) {
        slot.target_local_x *= correction;
      }
    }

    if (widest > 0.001F) {
      float const live_limit = std::max(corridor_half_width, widest * floor_correction);

      float const max_step = limits.speed * step;
      for (auto& slot : next) {
        float const share = std::abs(slot.target_local_x) / widest;
        float const allowance = live_limit * share;
        if (std::abs(slot.current_local_x) <= allowance) {
          continue;
        }
        float const bounded = std::copysign(allowance, slot.current_local_x);
        slot.current_local_x +=
            std::clamp(bounded - slot.current_local_x, -max_step, max_step);
      }
    }
  }

  auto slot_ground_is_open = [&](float local_x, float local_z) {
    if (pathfinder == nullptr) {
      return true;
    }
    float const world_x =
        transform.position.x + (cos_yaw * local_x) + (sin_yaw * local_z);
    float const world_z =
        transform.position.z - (sin_yaw * local_x) + (cos_yaw * local_z);
    auto const cell = pathfinder->world_to_grid(world_x, world_z);
    return pathfinder->is_terrain_walkable(cell.x, cell.y);
  };

  auto onto_walkable_ground = [&](float& local_x, float& local_z) {
    if (slot_ground_is_open(local_x, local_z)) {
      return false;
    }
    float low = 0.0F;
    float high = 1.0F;
    for (int bisection = 0; bisection < k_slot_recovery_bisections; ++bisection) {
      float const middle = (low + high) * 0.5F;
      if (slot_ground_is_open(local_x * middle, local_z * middle)) {
        low = middle;
      } else {
        high = middle;
      }
    }
    float const corrected_x = local_x * low;
    float const corrected_z = local_z * low;
    float const correction =
        Game::Systems::planar_length(corrected_x - local_x, corrected_z - local_z);
    float const budget = k_ground_clamp_speed * step;
    if (correction > budget && correction > 0.0001F) {
      float const share = budget / correction;
      local_x += (corrected_x - local_x) * share;
      local_z += (corrected_z - local_z) * share;
      return true;
    }
    local_x = corrected_x;
    local_z = corrected_z;
    return true;
  };

  std::vector<std::uint8_t> recovering_to_ground(next.size(), 0U);
  for (std::size_t index = 0; index < next.size(); ++index) {
    auto& slot = next[index];
    if (!slot.alive) {
      continue;
    }
    recovering_to_ground[index] = static_cast<std::uint8_t>(
        onto_walkable_ground(slot.target_local_x, slot.target_local_z));
  }

  float const minimum_separation =
      std::max(Game::Formation::TraversalPolicy::k_minimum_soldier_separation,
               layout.body_radius * 2.0F * 1.04F);
  state.blocked_slot_count = 0U;
  for (std::size_t slot_index = 0; slot_index < next.size(); ++slot_index) {
    auto& slot = next[slot_index];
    RelocationLimits const step_limits =
        recovering_to_ground[slot_index] != 0U
            ? RelocationLimits{k_ground_recovery_speed, k_ground_recovery_acceleration}
            : limits;
    slot.previous_local_x = slot.current_local_x;
    slot.previous_local_z = slot.current_local_z;

    float const waypoint_x = slot.target_local_x;
    float const waypoint_z = slot.target_local_z;
    float const error_x = waypoint_x - slot.current_local_x;
    float const error_z = waypoint_z - slot.current_local_z;
    float const distance = Game::Systems::planar_length(error_x, error_z);
    float desired_vx = 0.0F;
    float desired_vz = 0.0F;
    if (distance > 0.0001F) {
      float const stopping_speed =
          std::sqrt(2.0F * step_limits.acceleration * distance);
      float const speed = std::min(step_limits.speed, stopping_speed);
      desired_vx = error_x / distance * speed;
      desired_vz = error_z / distance * speed;
    }
    float delta_vx = desired_vx - slot.velocity_x;
    float delta_vz = desired_vz - slot.velocity_z;
    float const delta_speed = Game::Systems::planar_length(delta_vx, delta_vz);
    float const max_delta = step_limits.acceleration * step;
    if (delta_speed > max_delta && delta_speed > 0.0001F) {
      float const scale = max_delta / delta_speed;
      delta_vx *= scale;
      delta_vz *= scale;
    }
    float const next_vx = slot.velocity_x + delta_vx;
    float const next_vz = slot.velocity_z + delta_vz;
    float const proposed_x = slot.current_local_x + next_vx * step;
    float const proposed_z = slot.current_local_z + next_vz * step;

    auto keeps_separation = [&](float candidate_x, float candidate_z) {
      return std::all_of(next.begin(), next.end(), [&](auto const& other) {
        if (other.slot_index == slot.slot_index || !slot.alive || !other.alive) {
          return true;
        }
        float const previous_distance =
            Game::Systems::planar_length(slot.current_local_x - other.current_local_x,
                                         slot.current_local_z - other.current_local_z);
        float const candidate_distance = Game::Systems::planar_length(
            candidate_x - other.current_local_x, candidate_z - other.current_local_z);
        return candidate_distance + 0.00001F >= minimum_separation ||
               candidate_distance + 0.000001F >= previous_distance;
      });
    };

    int current_score = -1;
    auto stays_on_walkable_ground = [&](float candidate_x, float candidate_z) {
      if (!slot.alive) {
        return true;
      }
      if (current_score < 0) {
        current_score =
            walkability_score(slot.current_local_x, slot.current_local_z, 0.0F, 0.0F);
      }
      int const candidate_score =
          walkability_score(candidate_x, candidate_z, 0.0F, 0.0F);
      return candidate_score >= 9 || candidate_score >= current_score;
    };

    auto allowed = [&](float candidate_x, float candidate_z) {
      return keeps_separation(candidate_x, candidate_z) &&
             stays_on_walkable_ground(candidate_x, candidate_z);
    };

    float fraction = allowed(proposed_x, proposed_z) ? 1.0F : 0.0F;
    if (fraction == 0.0F) {
      float low = 0.0F;
      float high = 1.0F;
      for (int iteration = 0; iteration < 8; ++iteration) {
        float const candidate = (low + high) * 0.5F;
        float const candidate_x = slot.current_local_x + next_vx * step * candidate;
        float const candidate_z = slot.current_local_z + next_vz * step * candidate;
        if (allowed(candidate_x, candidate_z)) {
          low = candidate;
        } else {
          high = candidate;
        }
      }
      fraction = low;
    }
    slot.blocked = fraction < 0.999F;
    state.blocked_slot_count += slot.blocked ? 1U : 0U;
    slot.current_local_x += next_vx * step * fraction;
    slot.current_local_z += next_vz * step * fraction;
    slot.velocity_x = next_vx * fraction;
    slot.velocity_z = next_vz * fraction;

    if (slot.alive &&
        onto_walkable_ground(slot.current_local_x, slot.current_local_z)) {
      slot.velocity_x = 0.0F;
      slot.velocity_z = 0.0F;
    }
  }

  state.slot_states = std::move(next);
  ++state.slot_states_revision;
  float max_remaining_ratio = 0.0F;
  state.transition_total_distance = 0.0F;
  state.transition_remaining_distance = 0.0F;
  for (auto const& slot : state.slot_states) {
    float const total =
        Game::Systems::planar_length(slot.target_local_x - slot.start_local_x,
                                     slot.target_local_z - slot.start_local_z);
    float const remaining =
        Game::Systems::planar_length(slot.target_local_x - slot.current_local_x,
                                     slot.target_local_z - slot.current_local_z);
    state.transition_total_distance += total;
    state.transition_remaining_distance += remaining;
    if (total > 0.001F) {
      max_remaining_ratio = std::max(max_remaining_ratio, remaining / total);
    }
  }
  state.transition_progress = std::clamp(1.0F - max_remaining_ratio, 0.0F, 1.0F);
  state.transition_curve = smoothstep(state.transition_progress);

  state.root_motion_blocked = false;
}

} // namespace

void UnitTraversalLayoutSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  float const step = std::max(0.0F, delta_time);
  std::uint64_t const tick = world->tick_id();
  world->for_each_entity([step, tick](Engine::Core::Entity& entity) {
    auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
    auto const* transform = entity.get_component<Engine::Core::TransformComponent>();
    auto const* movement = entity.get_component<Engine::Core::MovementComponent>();
    auto* facts = entity.get_component<Engine::Core::MovementFactsComponent>();
    if (unit == nullptr || transform == nullptr || movement == nullptr ||
        facts == nullptr) {
      return;
    }
    auto* state = Engine::Core::get_or_add_component<
        Engine::Core::UnitTraversalLayoutStateComponent>(&entity);
    if (state == nullptr) {
      return;
    }

    auto const layout = FormationCombat::resolve_layout(entity);
    state->route_id = movement->get_route_id();
    std::uint16_t layout_id = 0xFFFFU;
    if (auto const* normal =
            entity.get_component<Engine::Core::UnitLayoutStateComponent>()) {
      layout_id = normal->layout_id;
    }

    bool const layout_shape_changed =
        state->normal_layout_id != layout_id ||
        state->stable_slot_mapping.size() != layout.all_slots.size();
    state->normal_layout_id = layout_id;

    if (layout_shape_changed) {

      rebuild_stable_mapping(layout, *state);
      state->normal_files = normal_file_count(layout);
    }
    state->soldier_body_radius = layout.body_radius;
    float const body_diameter = layout.body_radius * 2.0F;
    state->file_spacing = Game::Formation::TraversalPolicy::compact_spacing(
        layout.body_radius, layout.spacing);
    state->rank_spacing = state->file_spacing;
    float layout_half_width = 0.0F;
    for (auto const& slot : layout.all_slots) {
      layout_half_width = std::max(layout_half_width, std::abs(slot.local_x));
    }
    if (layout_shape_changed) {
      state->minimum_lateral_scale =
          Game::Formation::TraversalPolicy::k_formation_spacing_scale;
      float authored_spacing = std::numeric_limits<float>::infinity();
      for (std::size_t first = 0; first < layout.all_slots.size(); ++first) {
        for (std::size_t second = first + 1; second < layout.all_slots.size();
             ++second) {
          if (layout.all_slots[first].row != layout.all_slots[second].row) {
            continue;
          }
          float const separation = std::abs(layout.all_slots[first].local_x -
                                            layout.all_slots[second].local_x);
          if (separation > 0.01F) {
            authored_spacing = std::min(authored_spacing, separation);
          }
        }
      }
      if (std::isfinite(authored_spacing)) {

        state->minimum_lateral_scale = std::clamp(
            body_diameter * Game::Formation::TraversalPolicy::k_body_spacing_scale /
                authored_spacing,
            k_minimum_squeeze_scale,
            1.0F);
      }
    }

    bool const moving = movement->get_has_target() || movement->has_waypoints() ||
                        Game::Systems::planar_length(facts->motor.accepted_vx,
                                                     facts->motor.accepted_vz) > 0.05F;
    float const route_tangent_length = Game::Systems::planar_length(
        facts->desired.tangent_x, facts->desired.tangent_z);
    float const yaw = transform->rotation.y * std::numbers::pi_v<float> / 180.0F;
    float const route_alignment = route_tangent_length > 0.001F
                                      ? (std::sin(yaw) * facts->desired.tangent_x +
                                         std::cos(yaw) * facts->desired.tangent_z) /
                                            route_tangent_length
                                      : 1.0F;
    bool const entry_frame_aligned = route_alignment >= k_entry_alignment_cosine;
    std::uint64_t const measure_interval =
        moving ? k_moving_width_measure_ticks : k_resting_width_measure_ticks;
    bool const measure_now =
        state->desired_half_width <= k_width_epsilon ||
        ((tick + static_cast<std::uint64_t>(entity.get_id())) % measure_interval) == 0U;

    WidthMeasurement width;
    if (measure_now) {
      width = measure_width(entity, *transform, *movement, *facts, layout);
      state->available_half_width = width.available_half_width;
      state->desired_half_width = width.desired_half_width;
    } else {

      width.available_half_width = state->available_half_width;
      width.desired_half_width = state->desired_half_width;
      width.constrained =
          width.available_half_width + k_width_epsilon < width.desired_half_width;
    }

    std::uint32_t const target_files = state->normal_files;

    state->mode_dwell_seconds += step;
    bool const wants_traversal =
        moving && width.constrained && (state->active || entry_frame_aligned) &&
        width.available_half_width + k_width_epsilon < width.desired_half_width;
    if (wants_traversal) {
      state->entry_progress =
          std::min(1.0F, state->entry_progress + step / k_enter_dwell_seconds);
      state->exit_progress = 0.0F;
      state->tail_clear_seconds = 0.0F;
    } else {
      state->entry_progress = 0.0F;
      bool const exit_width_clear =
          !width.constrained &&
          width.available_half_width + k_exit_width_margin >= width.desired_half_width;
      state->tail_clear_seconds =
          exit_width_clear ? state->tail_clear_seconds + step : 0.0F;
      state->exit_progress =
          std::clamp(state->tail_clear_seconds / k_tail_clear_seconds, 0.0F, 1.0F);
    }

    bool const was_active = state->active;
    std::uint32_t accepted_files = state->target_files;
    if (!state->active && wants_traversal && state->entry_progress >= 1.0F) {
      accepted_files = target_files;
      state->active = true;
      state->portal_id = static_cast<std::uint32_t>(
          (state->route_id ^ (state->route_id >> 32U) ^ facts->route.route_revision) |
          1U);
    } else if (state->active && wants_traversal) {
      accepted_files = target_files;
      if (target_files != state->target_files &&
          state->mode_dwell_seconds < k_minimum_mode_dwell_seconds) {
        accepted_files = state->target_files;
      } else if (target_files > state->target_files) {
        float const required_half_width =
            0.5F * (body_diameter +
                    static_cast<float>(target_files - 1U) * state->file_spacing);
        if (width.available_half_width < required_half_width + k_exit_width_margin) {
          accepted_files = state->target_files;
        }
      }
    } else if (state->active && !wants_traversal &&
               state->mode_dwell_seconds >= k_minimum_mode_dwell_seconds &&
               state->exit_progress >= 1.0F) {
      accepted_files = state->normal_files;
      state->active = false;
    } else if (!state->active) {
      accepted_files = state->normal_files;
    }

    auto const accepted_mode = mode_for_files(accepted_files, state->normal_files);
    bool reset_slot_transition = was_active != state->active;
    if (accepted_files != state->target_files || accepted_mode != state->target_mode) {
      state->target_files = accepted_files;
      state->target_mode = accepted_mode;
      state->transition_progress = 0.0F;
      state->transition_curve = 0.0F;
      state->transition_seconds = k_transition_seconds;
      state->mode_dwell_seconds = 0.0F;
      reset_slot_transition = true;
    }

    float target_scale = 1.0F;
    if (state->active) {
      if (layout_half_width > k_width_epsilon) {
        float const usable_half_width = width.available_half_width -
                                        (layout.body_radius * k_soldier_probe_reach) -
                                        k_width_epsilon;
        target_scale = std::clamp(
            usable_half_width / layout_half_width, k_minimum_squeeze_scale, 1.0F);
      } else if (width.desired_half_width > k_width_epsilon) {

        target_scale = std::clamp(width.available_half_width / width.desired_half_width,
                                  k_minimum_squeeze_scale,
                                  1.0F);
      }
    }
    state->target_lateral_scale = target_scale;
    float const rate =
        target_scale < state->lateral_scale ? k_narrow_rate : k_reform_rate;
    float const max_scale_step = rate * step;
    state->lateral_scale += std::clamp(
        target_scale - state->lateral_scale, -max_scale_step, max_scale_step);
    update_slot_states(*transform,
                       *movement,
                       *facts,
                       *unit,
                       layout,
                       reset_slot_transition,
                       step,
                       *state);
    if (state->transition_progress >= 0.999F) {
      state->transition_progress = 1.0F;
      state->transition_curve = 1.0F;
      state->current_files = state->target_files;
      state->mode = state->target_mode;
      if (!state->active) {
        state->portal_id = 0U;
      }
    }
    publish_facts(*state, facts->traversal);
  });
}

auto UnitTraversalLayoutSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(
      Reads<UnitComponent,
            TransformComponent,
            MovementComponent,
            UnitLayoutStateComponent>{},
      Writes<MovementFactsComponent, UnitTraversalLayoutStateComponent>{});
}

} // namespace Game::Systems
