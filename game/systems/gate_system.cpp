#include "gate_system.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "../core/component.h"
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
  Engine::Core::Entity* entity{nullptr};
  GateComponent* gate{nullptr};
  float center_x{0.0F};
  float center_z{0.0F};
  float occupancy_half_x{0.0F};
  float occupancy_half_z{0.0F};
  int owner_id{0};
  bool wants_open{false};
  bool occupied{false};
};

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

  auto gate_entities = world->get_entities_with<GateComponent>();
  if (gate_entities.empty()) {
    GateService::clear_blockers();
    return;
  }

  std::vector<GateRecord> gates;
  gates.reserve(gate_entities.size());

  for (auto* entity : gate_entities) {
    if (entity == nullptr || entity->has_component<PendingRemovalComponent>()) {
      continue;
    }

    auto* gate = entity->get_component<GateComponent>();
    const auto* transform = entity->get_component<TransformComponent>();
    const auto* unit = entity->get_component<UnitComponent>();
    if (gate == nullptr || transform == nullptr || unit == nullptr ||
        unit->health <= 0) {
      continue;
    }

    const auto passage = GateService::passage_extent(transform->rotation.y);
    gates.push_back(GateRecord{.entity = entity,
                               .gate = gate,
                               .center_x = transform->position.x,
                               .center_z = transform->position.z,
                               .occupancy_half_x = passage.half_x + k_occupancy_margin,
                               .occupancy_half_z = passage.half_z + k_occupancy_margin,
                               .owner_id = unit->owner_id});
  }

  if (gates.empty()) {
    GateService::clear_blockers();
    return;
  }

  for (auto* entity : world->get_entities_with<UnitComponent>()) {
    if (entity == nullptr || entity->has_component<PendingRemovalComponent>()) {
      continue;
    }

    const auto* unit = entity->get_component<UnitComponent>();
    const auto* transform = entity->get_component<TransformComponent>();
    if (unit == nullptr || transform == nullptr || unit->health <= 0 ||
        !Game::Units::is_troop_spawn(unit->spawn_type)) {
      continue;
    }

    for (auto& record : gates) {

      if (!GateService::serves_owner(record.owner_id, unit->owner_id)) {
        continue;
      }

      const float dx = transform->position.x - record.center_x;
      const float dz = transform->position.z - record.center_z;
      const float radius = record.gate->trigger_radius;
      if ((dx * dx) + (dz * dz) <= radius * radius) {
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
      if (gate.state == GateComponent::State::Opening) {
        Engine::Core::EventManager::instance().publish(
            Engine::Core::AudioCueEvent("build.gate_open"));
      } else if (gate.state == GateComponent::State::Closing) {
        Engine::Core::EventManager::instance().publish(
            Engine::Core::AudioCueEvent("build.gate_close"));
      }
    }
  }

  GateService::refresh_blockers(*world);
}

} // namespace Game::Systems
