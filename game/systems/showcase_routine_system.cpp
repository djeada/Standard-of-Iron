#include "showcase_routine_system.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../core/component.h"
#include "../core/world.h"
#include "animation/showcase_pose_manifest.h"

using Animation::PoseVec3;
#include "arrow_system.h"

namespace Game::Systems {

namespace {

[[nodiscard]] auto
move_duration(const Engine::Core::ShowcaseRoutineComponent::Step& step) -> float {
  float authored = step.duration;
  if (authored <= 0.0F) {
    authored = Animation::humanoid_showcase_move_duration(
        static_cast<Animation::HumanoidShowcaseMove>(step.move));
  }
  return std::max(0.05F, authored);
}

[[nodiscard]] auto
step_length(const Engine::Core::ShowcaseRoutineComponent::Step& step) -> float {
  return move_duration(step) + std::max(0.0F, step.hold_after);
}

void apply_travel(Engine::Core::ShowcaseRoutineComponent& routine,
                  Engine::Core::TransformComponent& transform,
                  Animation::HumanoidShowcaseMove move,
                  float phase) {
  float const model_scale = std::max(0.01F, transform.scale.x);
  auto const authored = Animation::humanoid_showcase_root_travel(move, phase);
  PoseVec3 const travel{authored.x * model_scale, 0.0F, authored.z * model_scale};
  float const delta_x = travel.x - routine.applied_travel_x;
  float const delta_z = travel.z - routine.applied_travel_z;
  routine.applied_travel_x = travel.x;
  routine.applied_travel_z = travel.z;
  transform.position.x +=
      (delta_x * routine.facing_cos) + (delta_z * routine.facing_sin);
  transform.position.z +=
      (delta_z * routine.facing_cos) - (delta_x * routine.facing_sin);
}

void release_throw(Engine::Core::World& world,
                   Engine::Core::ShowcaseRoutineComponent& routine,
                   const Engine::Core::TransformComponent& transform) {
  auto* arrows = world.get_system<Game::Systems::ArrowSystem>();
  if (arrows == nullptr) {
    return;
  }
  constexpr float k_hand_height = 1.85F;
  constexpr float k_hand_reach = 0.45F;
  QVector3D const start(transform.position.x + (k_hand_reach * routine.facing_sin),
                        transform.position.y + k_hand_height,
                        transform.position.z + (k_hand_reach * routine.facing_cos));
  QVector3D const end(
      routine.throw_target_x, transform.position.y + 1.15F, routine.throw_target_z);
  arrows->spawn_arrow(start,
                      end,
                      QVector3D(0.62F, 0.52F, 0.38F),
                      24.0F,
                      Game::Systems::ArrowVisualStyle::Javelin);
}

void set_idle(Engine::Core::ShowcaseRoutineComponent& routine) {
  routine.active = false;
  routine.current_move = 0;
  routine.phase = 0.0F;
}

} // namespace

void ShowcaseRoutineSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr || delta_time <= 0.0F) {
    return;
  }

  for (auto* entity :
       world->get_entities_with<Engine::Core::ShowcaseRoutineComponent>()) {
    auto* routine = entity->get_component<Engine::Core::ShowcaseRoutineComponent>();
    if (routine == nullptr || routine->steps.empty() || routine->finished) {
      if (routine != nullptr) {
        set_idle(*routine);
      }
      continue;
    }

    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (!routine->active && routine->index == 0 && routine->elapsed <= 0.0F &&
        transform != nullptr) {
      float const yaw = transform->rotation.y * (std::numbers::pi_v<float> / 180.0F);
      routine->facing_sin = std::sin(yaw);
      routine->facing_cos = std::cos(yaw);
    }

    routine->elapsed += delta_time;
    if (routine->start_delay > 0.0F) {
      if (routine->elapsed < routine->start_delay) {
        set_idle(*routine);
        continue;
      }
      routine->elapsed -= routine->start_delay;
      routine->start_delay = 0.0F;
    }

    while (routine->elapsed >= step_length(routine->steps[routine->index])) {
      auto const& done = routine->steps[routine->index];
      if (transform != nullptr) {
        apply_travel(*routine,
                     *transform,
                     static_cast<Animation::HumanoidShowcaseMove>(done.move),
                     1.0F);
      }
      routine->applied_travel_x = 0.0F;
      routine->applied_travel_z = 0.0F;
      routine->elapsed -= step_length(done);
      ++routine->index;
      if (routine->index >= routine->steps.size()) {

        routine->index = std::min(routine->loop_from, routine->steps.size() - 1U);
        if (!routine->loop) {
          routine->finished = true;
          break;
        }
      }
    }

    if (routine->finished) {
      set_idle(*routine);
      continue;
    }

    auto const& step = routine->steps[routine->index];
    auto const move = static_cast<Animation::HumanoidShowcaseMove>(step.move);
    float const phase = std::clamp(routine->elapsed / move_duration(step), 0.0F, 1.0F);
    if (transform != nullptr) {
      apply_travel(*routine, *transform, move, phase);
    }

    float const release = Animation::humanoid_showcase_release_phase(move);
    if (move == Animation::HumanoidShowcaseMove::SpearThrow &&
        routine->has_throw_target && transform != nullptr && release > 0.0F) {
      auto* renderable = entity->get_component<Engine::Core::RenderableComponent>();
      if (phase < release * 0.5F) {
        routine->throw_armed = true;
        if (renderable != nullptr && !routine->armed_renderer_id.empty()) {
          renderable->renderer_id = routine->armed_renderer_id;
        }
      } else if (routine->throw_armed && phase >= release) {
        routine->throw_armed = false;
        release_throw(*world, *routine, *transform);
        if (renderable != nullptr && !routine->released_renderer_id.empty()) {
          renderable->renderer_id = routine->released_renderer_id;
        }
      }
    }

    routine->active = move != Animation::HumanoidShowcaseMove::None;
    routine->current_move = step.move;
    routine->phase = phase;
  }
}

} // namespace Game::Systems
