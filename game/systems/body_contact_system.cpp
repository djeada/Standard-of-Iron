#include "body_contact_system.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <vector>

#include "../core/ambient_session.h"
#include "../core/component_economy.h"
#include "../core/component_gameplay.h"
#include "../core/entity.h"
#include "../core/system_context.h"
#include "../core/world.h"
#include "../map/terrain_service.h"
#include "../wildlife/wildlife_config.h"
#include "body_profile.h"
#include "building_collision_registry.h"
#include "command_service.h"
#include "nav_grid.h"
#include "walkability.h"

namespace Game::Systems {

namespace {

struct ContactBody {
  Engine::Core::TransformComponent* transform{nullptr};
  Engine::Core::MovementFactsComponent* facts{nullptr};
  float radius{0.0F};
  BodyProfile profile;
  bool movable{false};
  bool locked_in_melee{false};

  bool yields_when_idle{false};
  float separation_remaining{0.0F};
  Engine::Core::EntityID melee_intent{0};

  bool has_travel{false};
  float travel_x{0.0F};
  float travel_z{0.0F};

  bool in_one_lane_passage{false};
};

auto melee_intent_of(const Engine::Core::World& world,
                     Engine::Core::EntityID id) -> Engine::Core::EntityID {
  if (const auto* wildlife = world.try_get<Engine::Core::WildlifeComponent>(id);
      wildlife != nullptr && wildlife->behavior == Game::Wildlife::Behavior::Stalk) {
    return wildlife->focus_id;
  }
  const auto* attack = world.try_get<Engine::Core::AttackComponent>(id);
  if (attack == nullptr ||
      attack->current_mode != Engine::Core::AttackComponent::CombatMode::Melee) {
    return 0;
  }
  if (attack->in_melee_lock && attack->melee_lock_target_id != 0) {
    return attack->melee_lock_target_id;
  }
  const auto* target = world.try_get<Engine::Core::AttackTargetComponent>(id);
  return target != nullptr ? target->target_id : 0;
}

auto body_stands_its_ground(const Engine::Core::World& world,
                            Engine::Core::EntityID id) -> bool {
  if (world.has<Engine::Core::BuildingComponent>(id) ||
      world.has<Engine::Core::WildlifeComponent>(id)) {
    return true;
  }
  if (const auto* attack = world.try_get<Engine::Core::AttackComponent>(id);
      attack != nullptr && attack->in_melee_lock) {
    return true;
  }
  const auto* hold = world.try_get<Engine::Core::HoldModeComponent>(id);
  return hold != nullptr && hold->active;
}

auto body_yields_when_idle(const Engine::Core::World& world,
                           Engine::Core::EntityID id) -> bool {
  if (body_stands_its_ground(world, id) ||
      world.has<Engine::Core::BuilderProductionComponent>(id)) {
    return false;
  }
  const auto* movement = world.try_get<Engine::Core::MovementComponent>(id);
  return movement != nullptr && !movement->get_has_target();
}

auto body_is_movable(const Engine::Core::World& world,
                     Engine::Core::EntityID id,
                     const Engine::Core::MovementFactsComponent* facts) -> bool {
  if (facts == nullptr || !facts->desired.valid || body_stands_its_ground(world, id)) {
    return false;
  }
  return world.has<Engine::Core::MovementComponent>(id);
}

void slide_along_travel(const ContactBody& body,
                        float& dx,
                        float& dz,
                        bool forward_only) {
  if (!body.has_travel) {
    return;
  }
  float const along = (dx * body.travel_x) + (dz * body.travel_z);
  if (body.in_one_lane_passage || forward_only) {

    float const forward = std::max(along, 0.0F);
    dx = body.travel_x * forward;
    dz = body.travel_z * forward;
    return;
  }
  if (along >= 0.0F) {
    return;
  }

  float const perpendicular_x = -body.travel_z;
  float const perpendicular_z = body.travel_x;
  float lateral = (dx * perpendicular_x) + (dz * perpendicular_z);
  float const side = lateral >= 0.0F ? 1.0F : -1.0F;
  lateral += side * -along;

  dx = perpendicular_x * lateral;
  dz = perpendicular_z * lateral;
}

auto try_push(ContactBody& body, float dx, float dz, bool forward_only) -> bool {
  slide_along_travel(body, dx, dz, forward_only);
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
  if (body.facts != nullptr) {
    body.facts->steering.contact_push_x += dx;
    body.facts->steering.contact_push_z += dz;
  }
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
  const auto& services = Game::Session::services_for(world);
  const auto& terrain = *services.terrain;
  const auto& buildings = *services.building_collision;

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
    if (body.facts != nullptr) {

      body.facts->steering.contact_push_x = 0.0F;
      body.facts->steering.contact_push_z = 0.0F;
      body.facts->steering.body_overlap = 0.0F;
    }
    body.radius = CommandService::get_unit_radii(world, entry.id).core;
    body.profile = body_profile_for(*entity);
    body.movable = body_is_movable(world, entry.id, body.facts);
    if (const auto* attack = world.try_get<Engine::Core::AttackComponent>(entry.id)) {
      body.locked_in_melee = attack->in_melee_lock;
    }
    body.yields_when_idle = !body.movable && body_yields_when_idle(world, entry.id);
    if (body.facts != nullptr && body.facts->desired.valid) {
      float const speed =
          std::hypot(body.facts->desired.velocity_x, body.facts->desired.velocity_z);
      if (speed > 1.0e-4F) {
        body.has_travel = true;
        body.travel_x = body.facts->desired.velocity_x / speed;
        body.travel_z = body.facts->desired.velocity_z / speed;
      }
    }
    body.separation_remaining =
        std::min(k_separation_speed * delta_time, k_max_separation_step);
    {
      auto const cell =
          NavGrid::world_to_grid(transform->position.x, transform->position.z);
      body.in_one_lane_passage =
          terrain.is_on_bridge(transform->position.x, transform->position.z) ||
          terrain.is_hill_entrance(cell.x, cell.y) ||
          buildings.point_in_navigation_passage(transform->position.x,
                                                transform->position.z);
    }
    body.melee_intent = melee_intent_of(world, entry.id);
    widest_radius = std::max(widest_radius, body.radius);
  }

  const Engine::Core::WorldSpatialIndex::Entry* const first = entries.data();

  const float stale_margin =
      k_stale_position_margin + (k_stale_position_speed_allowance * delta_time);

  struct Pair {
    std::size_t me;
    std::size_t them;
    float overlap;
  };
  std::vector<Pair> pairs;
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
          const auto other_slot = static_cast<std::size_t>(&other - first);
          ContactBody& them = bodies[other_slot];
          if (them.transform == nullptr || (!me.movable && !them.movable)) {
            return;
          }
          const float combined = me.radius + them.radius;
          if (combined <= 1.0e-4F) {
            return;
          }
          const float distance =
              std::hypot(me.transform->position.x - them.transform->position.x,
                         me.transform->position.z - them.transform->position.z);
          if (distance >= combined) {
            return;
          }
          pairs.push_back({slot, other_slot, combined - distance});
        });
  }
  std::stable_sort(pairs.begin(), pairs.end(), [](const Pair& lhs, const Pair& rhs) {
    return lhs.overlap > rhs.overlap;
  });

  for (const auto& pair : pairs) {
    ContactBody& me = bodies[pair.me];
    ContactBody& them = bodies[pair.them];
    const Engine::Core::EntityID me_id = entries[pair.me].id;
    const Engine::Core::EntityID them_id = entries[pair.them].id;

    bool const me_pushable = me.movable || (them.movable && me.yields_when_idle);
    bool const them_pushable = them.movable || (me.movable && them.yields_when_idle);

    const float combined = me.radius + them.radius;
    float px = me.transform->position.x - them.transform->position.x;
    float pz = me.transform->position.z - them.transform->position.z;
    float distance = std::hypot(px, pz);
    if (distance >= combined) {
      continue;
    }
    if (me.melee_intent == them_id || them.melee_intent == me_id) {
      continue;
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

    bool const combat_press =
        me.locked_in_melee || them.locked_in_melee ||
        (me.melee_intent != 0 && them.melee_intent != 0) ||
        (entries[pair.them].owner_id != entries[pair.me].owner_id &&
         (me.melee_intent != 0 || them.melee_intent != 0));
    m_diagnostics.deepest_overlap = std::max(m_diagnostics.deepest_overlap, overlap);
    ++m_diagnostics.pairs_resolved;
    if (me.facts != nullptr && !combat_press) {
      me.facts->steering.body_overlap =
          std::max(me.facts->steering.body_overlap, overlap);
    }
    if (them.facts != nullptr && !combat_press) {
      them.facts->steering.body_overlap =
          std::max(them.facts->steering.body_overlap, overlap);
    }

    constexpr float k_idle_share = 0.35F;
    bool const friends = entries[pair.them].owner_id == entries[pair.me].owner_id;
    float my_share = 0.0F;
    float their_share = 0.0F;
    bool me_forward_only = false;
    bool them_forward_only = false;
    if (me.movable && them.movable) {
      my_share = 0.5F;
      their_share = 0.5F;
    } else if (me.movable) {
      their_share = them_pushable ? (friends ? 1.0F : k_idle_share) : 0.0F;
      my_share = 1.0F - their_share;
      me_forward_only = friends;
    } else if (them.movable) {
      my_share = me_pushable ? (friends ? 1.0F : k_idle_share) : 0.0F;
      their_share = 1.0F - my_share;
      them_forward_only = friends;
    }
    if (me.in_one_lane_passage || them.in_one_lane_passage) {

      float const me_ahead_of_them =
          me.has_travel ? (px * me.travel_x) + (pz * me.travel_z) : 0.0F;
      if (me_ahead_of_them > 0.0F) {
        my_share = 0.0F;
        their_share = them.movable ? 1.0F : 0.0F;
      } else if (me_ahead_of_them < 0.0F) {
        their_share = 0.0F;
        my_share = me.movable ? 1.0F : 0.0F;
      }
    }

    if (my_share > 0.0F &&
        !try_push(
            me, nx * overlap * my_share, nz * overlap * my_share, me_forward_only)) {
      ++m_diagnostics.pushes_rejected;
    }
    if (their_share > 0.0F && !try_push(them,
                                        -nx * overlap * their_share,
                                        -nz * overlap * their_share,
                                        them_forward_only)) {
      ++m_diagnostics.pushes_rejected;
    }
  }
}

auto BodyContactSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(Reads<UnitComponent,
                                     BuildingComponent,
                                     AttackComponent,
                                     AttackTargetComponent,
                                     WildlifeComponent,
                                     HoldModeComponent,
                                     MovementComponent,
                                     PendingRemovalComponent>{},
                               Writes<TransformComponent, MovementFactsComponent>{});
}

} // namespace Game::Systems
