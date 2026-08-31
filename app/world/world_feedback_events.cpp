#include "app/world/world_feedback_events.h"

#include <algorithm>

#include "game/core/component.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"

namespace App::Core {

namespace {

auto feedback_kind_of(Engine::Core::WorldFeedbackKind kind) -> FeedbackKind {
  switch (kind) {
  case Engine::Core::WorldFeedbackKind::Reserve:
    return FeedbackKind::Reserve;
  case Engine::Core::WorldFeedbackKind::Heal:
    return FeedbackKind::Heal;
  case Engine::Core::WorldFeedbackKind::Resource:
    break;
  }
  return FeedbackKind::Resource;
}

auto label_height_of(const Engine::Core::Entity& entity,
                     const Engine::Core::TransformComponent& transform) -> float {
  const bool is_building = entity.has_component<Engine::Core::BuildingComponent>();
  return transform.position.y +
         (is_building ? std::max(2.4F, transform.scale.y * 0.9F) : 1.8F);
}

} // namespace

auto combat_feedback_tick(Engine::Core::World& world,
                          const Engine::Core::CombatHitEvent& event,
                          FeedbackStyle style) -> std::optional<WorldFeedbackTick> {
  const auto* target = world.get_entity(event.target_id);
  if (target == nullptr) {
    return std::nullopt;
  }
  const auto* transform = target->get_component<Engine::Core::TransformComponent>();
  const auto* unit = target->get_component<Engine::Core::UnitComponent>();
  if (transform == nullptr || unit == nullptr) {
    return std::nullopt;
  }

  WorldFeedbackTick hit;
  hit.anchor = event.target_id;
  hit.kind = FeedbackKind::Damage;
  hit.style = style;
  hit.x = transform->position.x;
  hit.y = label_height_of(*target, *transform);
  hit.z = transform->position.z;
  hit.amount = event.damage;
  hit.severity = unit->max_health > 0
                     ? std::clamp(static_cast<float>(event.damage) /
                                      static_cast<float>(unit->max_health),
                                  0.0F,
                                  1.5F)
                     : 0.0F;
  hit.killing_blow = event.is_killing_blow;
  return hit;
}

auto world_feedback_tick(Engine::Core::World& world,
                         const Engine::Core::WorldFeedbackEvent& event)
    -> std::optional<WorldFeedbackTick> {
  if (event.amount == 0) {
    return std::nullopt;
  }

  WorldFeedbackTick tick;
  tick.anchor = event.anchor_id;
  tick.kind = feedback_kind_of(event.kind);
  tick.style = FeedbackStyle::Tick;
  tick.amount = event.amount;
  tick.severity = event.severity;
  tick.resource = event.resource;
  tick.paired_resource = event.paired_resource;
  tick.paired_amount = event.paired_amount;
  tick.outgoing = event.amount < 0;
  tick.incoming = event.amount > 0;

  if (event.anchor_id != Engine::Core::NULL_ENTITY) {
    if (const auto* anchor = world.get_entity(event.anchor_id)) {
      if (const auto* transform =
              anchor->get_component<Engine::Core::TransformComponent>()) {
        tick.x = transform->position.x;
        tick.y = label_height_of(*anchor, *transform);
        tick.z = transform->position.z;
        return tick;
      }
    }
  }

  if (!event.has_position) {
    return std::nullopt;
  }
  tick.anchor = Engine::Core::NULL_ENTITY;
  tick.x = event.x;
  tick.y = event.y + 1.8F;
  tick.z = event.z;
  return tick;
}

} // namespace App::Core
