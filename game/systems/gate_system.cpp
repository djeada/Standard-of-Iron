#include "gate_system.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "../core/component_gameplay.h"
#include "../core/entity.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "gate_service.h"

namespace Game::Systems {

namespace {

using Engine::Core::GateComponent;
using Engine::Core::PendingRemovalComponent;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;

constexpr float k_occupancy_margin = 0.6F;

struct GateRecord {
  GateComponent* gate{nullptr};
  float center_x{0.0F};
  float center_z{0.0F};
  float rotation_y{0.0F};
  float occupancy_half_x{0.0F};
  float occupancy_half_z{0.0F};
  int owner_id{0};
  bool wants_open{false};
  bool occupied{false};
};

auto movement_intends_to_cross(const GateRecord& gate,
                               const TransformComponent& transform,
                               const Engine::Core::MovementComponent& movement)
    -> bool {
  if (!movement.get_has_target()) {
    return false;
  }

  float const target_x = movement.get_has_requested_goal()
                             ? movement.get_requested_goal_x()
                             : movement.get_goal_x();
  float const target_z = movement.get_has_requested_goal()
                             ? movement.get_requested_goal_z()
                             : movement.get_goal_y();
  bool const spans_x = GateComponent::spans_x_axis(gate.rotation_y);
  float const from_across = spans_x ? transform.position.z - gate.center_z
                                    : transform.position.x - gate.center_x;
  float const to_across = spans_x ? target_z - gate.center_z : target_x - gate.center_x;
  if (from_across * to_across > 0.0F || std::abs(from_across - to_across) < 0.001F) {
    return false;
  }

  float const t = from_across / (from_across - to_across);
  float const crossing_along =
      spans_x ? transform.position.x + (target_x - transform.position.x) * t
              : transform.position.z + (target_z - transform.position.z) * t;
  float const gate_along = spans_x ? gate.center_x : gate.center_z;
  return std::abs(crossing_along - gate_along) <= GateComponent::k_passage_half_width;
}

auto derive_state(const GateComponent& gate, bool opening) -> GateComponent::State {
  if (gate.open_amount >= 1.0F) {
    return GateComponent::State::Open;
  }
  if (gate.open_amount <= 0.0F) {
    return GateComponent::State::Closed;
  }
  return opening ? GateComponent::State::Opening : GateComponent::State::Closing;
}

} // namespace

void GateSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  auto gate_view =
      world
          ->entity_view<GateComponent, const TransformComponent, const UnitComponent>();
  if (gate_view.empty()) {
    GateService::clear_blockers();
    return;
  }

  std::vector<GateRecord> gates;
  gates.reserve(gate_view.candidate_count());

  for (auto [gate_entity, gate_component, transform_component, unit_component] :
       gate_view) {
    Engine::Core::Entity* entity = &gate_entity;
    auto* gate = &gate_component;
    const auto* transform = &transform_component;
    const auto* unit = &unit_component;
    if (entity->has_component<PendingRemovalComponent>() || unit->health <= 0) {
      continue;
    }

    const auto passage = GateService::passage_extent(transform->rotation.y);
    const bool spans_x = GateComponent::spans_x_axis(transform->rotation.y);
    const float along_margin = spans_x ? k_occupancy_margin : 0.0F;
    const float across_margin = spans_x ? 0.0F : k_occupancy_margin;
    gates.push_back(GateRecord{.gate = gate,
                               .center_x = transform->position.x,
                               .center_z = transform->position.z,
                               .rotation_y = transform->rotation.y,
                               .occupancy_half_x = passage.half_x + along_margin,
                               .occupancy_half_z = passage.half_z + across_margin,
                               .owner_id = unit->owner_id});
  }

  if (gates.empty()) {
    GateService::clear_blockers();
    return;
  }

  for (auto [entity_id, unit_component, transform_component] :
       world->view<const UnitComponent, const TransformComponent>()) {
    if (world->has<PendingRemovalComponent>(entity_id)) {
      continue;
    }

    const auto* unit = &unit_component;
    const auto* transform = &transform_component;
    if (unit->health <= 0 || !Game::Units::is_troop_spawn(unit->spawn_type)) {
      continue;
    }

    for (auto& record : gates) {

      if (!GateService::serves_owner(*world, record.owner_id, unit->owner_id)) {
        continue;
      }

      const float dx = transform->position.x - record.center_x;
      const float dz = transform->position.z - record.center_z;
      const float radius = record.gate->trigger_radius;
      if ((dx * dx) + (dz * dz) <= radius * radius) {
        record.wants_open = true;
      }

      auto const* moving_entity = world->get_entity(entity_id);
      auto const* movement =
          moving_entity != nullptr
              ? moving_entity->get_component<Engine::Core::MovementComponent>()
              : nullptr;
      if (movement != nullptr &&
          movement_intends_to_cross(record, *transform, *movement)) {
        record.wants_open = true;
      }

      if (std::abs(dx) <= record.occupancy_half_x &&
          std::abs(dz) <= record.occupancy_half_z) {
        record.occupied = true;
      }
    }
  }

  for (auto& record : gates) {
    auto& gate = *record.gate;

    bool target_open = false;
    switch (gate.manual_mode) {
    case GateComponent::ManualMode::ForcedOpen:
      target_open = true;
      break;
    case GateComponent::ManualMode::ForcedClosed:

      target_open = record.occupied;
      break;
    case GateComponent::ManualMode::Automatic:
      if (record.wants_open || record.occupied) {
        gate.hold_timer = gate.hold_open_seconds;
      } else {
        gate.hold_timer = std::max(0.0F, gate.hold_timer - delta_time);
      }
      target_open = record.wants_open || record.occupied || gate.hold_timer > 0.0F;
      break;
    }

    if (gate.manual_mode != GateComponent::ManualMode::Automatic) {
      gate.hold_timer = 0.0F;
    }

    const float speed = target_open ? gate.open_speed : gate.close_speed;
    const float step = std::max(speed, 0.01F) * delta_time;
    const float previous = gate.open_amount;
    gate.open_amount = target_open ? std::min(1.0F, gate.open_amount + step)
                                   : std::max(0.0F, gate.open_amount - step);
    const GateComponent::State previous_state = gate.state;
    gate.state = derive_state(gate, gate.open_amount >= previous);

    if (gate.state != previous_state) {
      const char* const cue_id =
          gate.state == GateComponent::State::Opening   ? "build.gate_open"
          : gate.state == GateComponent::State::Closing ? "build.gate_close"
                                                        : nullptr;
      if (cue_id != nullptr) {
        auto cue = Engine::Core::AudioCueEvent::for_owner(record.owner_id, cue_id);
        cue.at(record.center_x, 0.0F, record.center_z);
        Engine::Core::EventManager::instance().publish(cue);
      }
    }
  }

  GateService::refresh_blockers(*world);
}

auto GateSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(Reads<TransformComponent,
                                     UnitComponent,
                                     MovementComponent,
                                     PendingRemovalComponent>{},
                               Writes<GateComponent>{});
}

} // namespace Game::Systems
