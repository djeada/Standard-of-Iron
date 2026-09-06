#include "body_contact_system.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <vector>

#include "../core/component_gameplay.h"
#include "../core/entity.h"
#include "../core/system_context.h"
#include "../core/world.h"
#include "command_service.h"
#include "walkability.h"

namespace Game::Systems {

namespace {

struct ContactBody {
  Engine::Core::TransformComponent* transform{nullptr};
  Engine::Core::MovementFactsComponent* facts{nullptr};
  float radius{0.0F};
  BodyProfile profile;
  bool movable{false};
  float separation_remaining{0.0F};
  Engine::Core::EntityID melee_intent{0};
};

auto melee_intent_of(const Engine::Core::Entity& entity) -> Engine::Core::EntityID {
  const auto* attack = entity.get_component<Engine::Core::AttackComponent>();
  if (attack == nullptr ||
      attack->current_mode != Engine::Core::AttackComponent::CombatMode::Melee) {
    return 0;
  }
  if (attack->in_melee_lock && attack->melee_lock_target_id != 0) {
    return attack->melee_lock_target_id;
  }
  const auto* target = entity.get_component<Engine::Core::AttackTargetComponent>();
  return target != nullptr ? target->target_id : 0;
}

auto body_is_movable(const Engine::Core::Entity& entity,
                     const Engine::Core::MovementFactsComponent* facts) -> bool {
  if (facts == nullptr || !facts->desired.valid) {
    return false;
  }
  if (entity.has_component<Engine::Core::BuildingComponent>()) {
    return false;
  }
  if (const auto* attack = entity.get_component<Engine::Core::AttackComponent>();
      attack != nullptr && attack->in_melee_lock) {
    return false;
  }
  if (const auto* hold = entity.get_component<Engine::Core::HoldModeComponent>();
      hold != nullptr && hold->active) {
    return false;
  }
  return entity.get_component<Engine::Core::MovementComponent>() != nullptr;
}

auto profile_for(const Engine::Core::Entity& entity) -> BodyProfile {
  BodyProfile profile;
  if (const auto* movement = entity.get_component<Engine::Core::MovementComponent>()) {
    profile.radius = movement->get_navigation_clearance();
    profile.passability = movement->get_can_enter_forest()
                              ? Pathfinding::Passability::Light
                              : Pathfinding::Passability::Heavy;
  }
  return profile;
}

auto try_push(ContactBody& body, float dx, float dz) -> bool {
  float const distance = std::hypot(dx, dz);
  float const step = std::min(distance, body.separation_remaining);
  if (step <= 1.0e-6F || distance <= 1.0e-6F) {
    return true;
  }
  dx *= step / distance;
  dz *= step / distance;
  QVector3D const destination(
      body.transform->position.x + dx, 0.0F, body.transform->position.z + dz);
  if (!Walkability::can_stand(destination, body.profile)) {
    return false;
  }
  body.transform->position.x = destination.x();
  body.transform->position.z = destination.z();
  body.separation_remaining -= step;
  return true;
}

} // namespace

void BodyContactSystem::run(Engine::Core::SystemContext& context) {
  const float delta_time = context.delta_time();
  if (delta_time <= 0.0F) {
    return;
  }

  m_diagnostics = {};

  auto& world = context.world();
  auto& index = world.spatial_index();
  index.refresh(world);
  const auto& entries = index.entries();
  if (entries.size() < 2U) {
    return;
  }

  std::vector<ContactBody> bodies(entries.size());
  float widest_radius = 0.0F;
  for (std::size_t slot = 0; slot < entries.size(); ++slot) {
    const auto& entry = entries[slot];
    if (!entry.is(Engine::Core::WorldSpatialIndex::k_alive) ||
        entry.is(Engine::Core::WorldSpatialIndex::k_building) ||
        entry.is(Engine::Core::WorldSpatialIndex::k_pending_removal)) {
      continue;
    }
    auto* entity = world.get_entity(entry.id);
    if (entity == nullptr) {
      continue;
    }
    auto* transform = world.try_get<Engine::Core::TransformComponent>(entity->get_id());
    if (transform == nullptr) {
      continue;
    }

    ContactBody& body = bodies[slot];
    body.transform = transform;
    body.facts = world.try_get<Engine::Core::MovementFactsComponent>(entity->get_id());
    body.radius = CommandService::get_unit_radii(world, entry.id).core;
    body.profile = profile_for(*entity);
    body.movable = body_is_movable(*entity, body.facts);
    body.separation_remaining =
        std::min(k_separation_speed * delta_time, k_max_separation_step);
    body.melee_intent = melee_intent_of(*entity);
    widest_radius = std::max(widest_radius, body.radius);
  }

  const Engine::Core::WorldSpatialIndex::Entry* const first = entries.data();

  const float stale_margin =
      k_stale_position_margin + (k_stale_position_speed_allowance * delta_time);

  for (std::size_t slot = 0; slot < entries.size(); ++slot) {
    ContactBody& me = bodies[slot];
    if (me.transform == nullptr) {
      continue;
    }
    const auto me_id = entries[slot].id;
    const float query_radius = me.radius + widest_radius + stale_margin;

    index.for_each_in_radius(
        me.transform->position.x,
        me.transform->position.z,
        query_radius,
        [&](const auto& other) {
          if (other.id <= me_id) {
            return;
          }
          ContactBody& them = bodies[static_cast<std::size_t>(&other - first)];
          if (them.transform == nullptr) {
            return;
          }
          if (!me.movable && !them.movable) {
            return;
          }

          const float combined = me.radius + them.radius;
          if (combined <= 1.0e-4F) {
            return;
          }
          float px = me.transform->position.x - them.transform->position.x;
          float pz = me.transform->position.z - them.transform->position.z;
          float distance = std::hypot(px, pz);
          if (distance >= combined) {
            return;
          }
          const Engine::Core::EntityID them_id = other.id;
          if (me.melee_intent == them_id || them.melee_intent == me_id) {
            return;
          }

          float nx = 0.0F;
          float nz = 0.0F;
          if (distance > 1.0e-5F) {
            nx = px / distance;
            nz = pz / distance;
          } else {

            nx = 1.0F;
            distance = 0.0F;
          }

          const float overlap = combined - distance;
          m_diagnostics.deepest_overlap =
              std::max(m_diagnostics.deepest_overlap, overlap);
          ++m_diagnostics.pairs_resolved;
          if (me.facts != nullptr) {
            me.facts->steering.body_overlap =
                std::max(me.facts->steering.body_overlap, overlap);
          }
          if (them.facts != nullptr) {
            them.facts->steering.body_overlap =
                std::max(them.facts->steering.body_overlap, overlap);
          }

          const float my_share = me.movable ? (them.movable ? 0.5F : 1.0F) : 0.0F;
          const float their_share = them.movable ? (me.movable ? 0.5F : 1.0F) : 0.0F;

          if (my_share > 0.0F &&
              !try_push(me, nx * overlap * my_share, nz * overlap * my_share)) {
            ++m_diagnostics.pushes_rejected;
          }
          if (their_share > 0.0F && !try_push(them,
                                              -nx * overlap * their_share,
                                              -nz * overlap * their_share)) {
            ++m_diagnostics.pushes_rejected;
          }
        });
  }
}

auto BodyContactSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(Reads<UnitComponent,
                                     BuildingComponent,
                                     AttackComponent,
                                     AttackTargetComponent,
                                     HoldModeComponent,
                                     MovementComponent,
                                     PendingRemovalComponent>{},
                               Writes<TransformComponent, MovementFactsComponent>{});
}

} // namespace Game::Systems
