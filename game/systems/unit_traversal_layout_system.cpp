#include "unit_traversal_layout_system.h"

#include <QVector3D>

#include <algorithm>
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
constexpr float k_tail_clear_seconds = 0.35F;
constexpr float k_transition_seconds = 0.65F;
constexpr float k_narrow_rate = 0.6F;
constexpr float k_reform_rate = 0.35F;
constexpr float k_entry_alignment_cosine = 0.94F;

constexpr float k_soldier_probe_reach = 0.88F;

constexpr float k_minimum_lookahead = 1.5F;
constexpr float k_maximum_lookahead = 40.0F;

constexpr std::uint64_t k_moving_width_measure_ticks = 4;
constexpr std::uint64_t k_resting_width_measure_ticks = 16;

struct WidthMeasurement {
  float available_half_width{0.0F};
  float footprint_half_width{0.0F};
  float constriction_distance{std::numeric_limits<float>::max()};
  bool constrained{false};
};

void rebuild_stable_mapping(const FormationCombat::FormationLayout& layout,
                            Engine::Core::UnitTraversalLayoutStateComponent& state) {
  state.stable_slot_mapping.clear();
  state.stable_slot_mapping.reserve(layout.all_slots.size());
  for (auto const& slot : layout.all_slots) {
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

auto measure_width(const Engine::Core::TransformComponent& transform,
                   const Engine::Core::MovementComponent& movement,
                   const Engine::Core::MovementFactsComponent& facts,
                   const FormationCombat::FormationLayout& layout,
                   float footprint_half_width,
                   float front_extent,
                   float rear_extent,
                   float probe_reach) -> WidthMeasurement {
  namespace Policy = Game::Formation::TraversalPolicy;
  WidthMeasurement result;
  result.footprint_half_width = footprint_half_width;
  float const probe_cap = result.footprint_half_width + Policy::k_exit_clearance;
  result.available_half_width = probe_cap;
  auto* pathfinder = NavGrid::get_pathfinder();
  if (pathfinder == nullptr || result.footprint_half_width <= k_width_epsilon) {
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

  float const lookahead =
      std::clamp(probe_reach, k_minimum_lookahead, k_maximum_lookahead);
  float const cell_size = std::max(0.1F, pathfinder->grid_cell_size());
  float const lateral_step = cell_size * k_probe_step_cells;
  float const longitudinal_step = cell_size * 0.5F;

  auto available_on_side =
      [&](QVector3D const& center, QVector3D const& lateral, float side) {
        float previous = 0.0F;
        for (float distance = lateral_step; distance <= probe_cap + lateral_step;
             distance += lateral_step) {
          float const clamped = std::min(distance, probe_cap);
          if (!pathfinder->is_world_position_walkable(
                  center + lateral * (clamped * side), passability)) {
            return previous;
          }
          previous = clamped;
          if (clamped >= probe_cap) {
            break;
          }
        }
        return probe_cap;
      };

  float const envelope_start = -rear_extent;
  float const envelope_end = front_extent + lookahead;
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
    if (left + k_width_epsilon >= probe_cap || right + k_width_epsilon >= probe_cap) {
      continue;
    }
    float const sample_width = std::min(left, right);
    if (sample_width < result.available_half_width) {
      result.available_half_width = sample_width;
      result.constriction_distance = offset;
    }
  }
  result.constrained = result.available_half_width <
                       result.footprint_half_width + Policy::k_enter_clearance;
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
  facts.normal_files = state.normal_files;
  facts.lateral_scale = state.lateral_scale;
  facts.about_faced = state.about_faced;
  facts.file_spacing = state.authored_file_spacing * state.lateral_scale;
}

void update_slot_states(const FormationCombat::FormationLayout& layout,
                        float step,
                        Engine::Core::UnitTraversalLayoutStateComponent& state) {
  float const frame_sign = state.about_faced ? -1.0F : 1.0F;
  std::vector<Engine::Core::UnitTraversalSlotState> next;
  next.reserve(layout.all_slots.size());
  for (auto const& slot : layout.all_slots) {
    Engine::Core::UnitTraversalSlotState placed;
    auto const* previous = state.slot_for(slot.index);
    placed.slot_index = slot.index;
    placed.row = slot.row;
    placed.col = slot.col;
    placed.alive =
        std::any_of(layout.live_slots.begin(),
                    layout.live_slots.end(),
                    [&slot](auto const& live) { return live.index == slot.index; });
    placed.current_local_x = frame_sign * slot.local_x * state.lateral_scale;
    placed.current_local_z = frame_sign * slot.local_z * state.lateral_scale;
    placed.previous_local_x =
        previous != nullptr ? previous->current_local_x : placed.current_local_x;
    placed.previous_local_z =
        previous != nullptr ? previous->current_local_z : placed.current_local_z;
    placed.start_local_x = placed.previous_local_x;
    placed.start_local_z = placed.previous_local_z;
    placed.target_local_x = frame_sign * slot.local_x * state.target_lateral_scale;
    placed.target_local_z = frame_sign * slot.local_z * state.target_lateral_scale;
    float const inverse_step = step > 0.0F ? 1.0F / step : 0.0F;
    placed.velocity_x =
        (placed.current_local_x - placed.previous_local_x) * inverse_step;
    placed.velocity_z =
        (placed.current_local_z - placed.previous_local_z) * inverse_step;
    next.push_back(placed);
  }
  state.slot_states = std::move(next);
  ++state.slot_states_revision;
  state.blocked_slot_count = 0U;
  state.transition_remaining_distance =
      std::abs(state.target_lateral_scale - state.lateral_scale);
  state.transition_total_distance = 1.0F - state.minimum_lateral_scale;
  state.transition_progress =
      1.0F - state.transition_remaining_distance /
                 std::max(0.001F, state.transition_total_distance);
  state.transition_curve = state.transition_progress;
}

} // namespace

void UnitTraversalLayoutSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  float const step = std::max(0.0F, delta_time);
  std::uint64_t const tick = world->tick_id();
  world->each<Engine::Core::MovementFactsComponent>(
      [world, step, tick](Engine::Core::EntityID id,
                          Engine::Core::MovementFactsComponent& facts_component) {
        auto* facts = &facts_component;
        auto const* unit = world->try_get<Engine::Core::UnitComponent>(id);
        auto const* transform = world->try_get<Engine::Core::TransformComponent>(id);
        auto const* movement = world->try_get<Engine::Core::MovementComponent>(id);
        auto* entity_ptr = world->get_entity(id);
        if (unit == nullptr || transform == nullptr || movement == nullptr ||
            entity_ptr == nullptr) {
          return;
        }
        auto& entity = *entity_ptr;
        auto* state = Engine::Core::get_or_add_component<
            Engine::Core::UnitTraversalLayoutStateComponent>(&entity);
        if (state == nullptr) {
          return;
        }

        auto const layout = FormationCombat::resolve_layout(entity);
        state->route_id = movement->get_route_id();
        std::uint16_t layout_id = 0xFFFFU;
        if (auto const* normal =
                world->try_get<Engine::Core::UnitLayoutStateComponent>(id)) {
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
        state->file_spacing = Game::Formation::TraversalPolicy::compact_spacing(
            layout.body_radius, layout.spacing);
        state->rank_spacing = state->file_spacing;
        state->authored_file_spacing = layout.spacing;
        state->minimum_lateral_scale = FormationCombat::minimum_formation_scale(layout);

        bool const moving =
            movement->get_has_target() || movement->has_waypoints() ||
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
        bool const measure_now = state->desired_half_width <= k_width_epsilon ||
                                 ((tick + static_cast<std::uint64_t>(entity.get_id())) %
                                  measure_interval) == 0U;

        namespace Policy = Game::Formation::TraversalPolicy;
        float const edge_margin =
            (layout.body_radius * k_soldier_probe_reach) + k_width_epsilon;
        float front_extent = layout.body_radius;
        float rear_extent = layout.body_radius;
        for (auto const& slot : layout.all_slots) {
          front_extent = std::max(front_extent, slot.local_z + layout.body_radius);
          rear_extent = std::max(rear_extent, layout.body_radius - slot.local_z);
        }
        float const footprint_half_width =
            FormationCombat::formation_lateral_half_extent(layout);
        float const probe_reach = std::max(
            k_minimum_lookahead, facts->desired.speed_limit * k_transition_seconds);

        WidthMeasurement width;
        if (measure_now) {
          width = measure_width(*transform,
                                *movement,
                                *facts,
                                layout,
                                footprint_half_width,
                                front_extent,
                                rear_extent,
                                probe_reach);
          state->available_half_width = width.available_half_width;
          state->desired_half_width = width.footprint_half_width;
          state->constriction_distance = width.constriction_distance;
        } else {

          width.available_half_width = state->available_half_width;
          width.footprint_half_width = state->desired_half_width;
          width.constriction_distance = state->constriction_distance;
          width.constrained = width.available_half_width <
                              width.footprint_half_width + Policy::k_enter_clearance;
        }

        state->mode_dwell_seconds += step;
        bool const wants_compression = width.constrained && entry_frame_aligned;
        state->tail_clear_seconds =
            wants_compression ? 0.0F : state->tail_clear_seconds + step;
        state->active =
            wants_compression ||
            (state->active && state->tail_clear_seconds < k_tail_clear_seconds);
        float const slot_half_width =
            std::max(k_width_epsilon, footprint_half_width - layout.body_radius);
        state->target_lateral_scale =
            state->active ? std::clamp((width.available_half_width - edge_margin) /
                                           slot_half_width,
                                       state->minimum_lateral_scale,
                                       1.0F)
                          : 1.0F;
        float const rate = state->target_lateral_scale < state->lateral_scale
                               ? k_narrow_rate
                               : k_reform_rate;
        state->lateral_scale +=
            std::clamp(state->target_lateral_scale - state->lateral_scale,
                       -rate * step,
                       rate * step);
        state->current_files = state->target_files = state->normal_files;
        state->mode = state->target_mode = Engine::Core::TraversalLayoutMode::Normal;
        state->portal_id = 0U;
        update_slot_states(layout, step, *state);
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
