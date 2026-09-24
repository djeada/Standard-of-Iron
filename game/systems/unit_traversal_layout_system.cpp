#include "unit_traversal_layout_system.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>

#include "../core/component_gameplay.h"
#include "../core/entity.h"
#include "../core/world.h"
#include "../formation/traversal_layout_policy.h"
#include "../formation/unit_layout.h"
#include "formation_combat_geometry.h"
#include "nav_grid.h"
#include "pathfinding.h"

namespace Game::Systems {
namespace {

constexpr float k_spacing_rate = 0.35F;

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

auto spacing_target(const Engine::Core::TransformComponent& transform,
                    const Engine::Core::MovementComponent& movement,
                    const FormationCombat::FormationLayout& layout,
                    float minimum_scale) -> float {
  auto const* pathfinder = NavGrid::get_pathfinder();
  if (pathfinder == nullptr) {
    return 1.0F;
  }
  auto const passability = movement.get_can_enter_forest()
                               ? Pathfinding::Passability::Light
                               : Pathfinding::Passability::Heavy;
  float const yaw = transform.rotation.y * std::numbers::pi_v<float> / 180.0F;
  float const reach = FormationCombat::formation_lateral_half_extent(layout);
  QVector3D const root(transform.position.x, 0.0F, transform.position.z);
  QVector3D const flank(std::cos(yaw) * reach, 0.0F, -std::sin(yaw) * reach);
  bool const bounded =
      !pathfinder->is_world_segment_walkable(root, root - flank, passability) &&
      !pathfinder->is_world_segment_walkable(root, root + flank, passability);
  return bounded ? minimum_scale : 1.0F;
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
  world->each<Engine::Core::MovementFactsComponent>(
      [world, step](Engine::Core::EntityID id,
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

        thread_local FormationCombat::FormationLayout layout;
        FormationCombat::resolve_layout_into(entity, layout);
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
        state->target_lateral_scale =
            spacing_target(*transform, *movement, layout, state->minimum_lateral_scale);
        state->lateral_scale =
            std::clamp(state->lateral_scale + std::clamp(state->target_lateral_scale -
                                                             state->lateral_scale,
                                                         -k_spacing_rate * step,
                                                         k_spacing_rate * step),
                       state->minimum_lateral_scale,
                       1.0F);
        state->active = state->lateral_scale < 1.0F;
        state->desired_half_width =
            FormationCombat::formation_lateral_half_extent(layout);
        state->available_half_width =
            layout.body_radius + (state->desired_half_width - layout.body_radius) *
                                     state->target_lateral_scale;
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
