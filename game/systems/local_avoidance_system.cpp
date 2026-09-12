#include "local_avoidance_system.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "../core/ambient_session.h"
#include "../core/component.h"
#include "../core/entity.h"
#include "../core/system_context.h"
#include "../core/world.h"
#include "../map/terrain_service.h"
#include "../util/planar_math.h"
#include "building_collision_registry.h"
#include "command_service.h"
#include "nav_grid.h"
#include "walkability.h"

namespace Game::Systems {

namespace {

struct Body {
  float core_radius{0.5F};
  float navigation_clearance{0.0F};
  Engine::Core::EntityID engaged_target{0};
  std::uint32_t formation_id{0};
  bool under_way{false};
  bool stopped_by_terrain{false};
  bool can_enter_forest{true};
  bool has_travel{false};
  float travel_x{0.0F};
  float travel_z{0.0F};
  float pace{0.0F};
};

} // namespace

void LocalAvoidanceSystem::run(Engine::Core::SystemContext& context) {
  const float delta_time = context.delta_time();
  if (delta_time <= 0.0F) {
    return;
  }

  m_diagnostics = {};

  auto& world = context.world();
  auto& index = world.spatial_index();
  index.refresh(world);
  const auto& entries = index.entries();
  if (entries.empty()) {
    return;
  }

  const auto& services = Game::Session::services_for(world);
  const auto& terrain = *services.terrain;
  const auto& buildings = *services.building_collision;

  std::vector<Body> bodies(entries.size());
  float widest_core = 0.0F;
  std::uint32_t units_processed = 0;
  for (std::size_t slot = 0; slot < entries.size(); ++slot) {
    const auto& entry = entries[slot];
    if (!entry.is(Engine::Core::WorldSpatialIndex::k_alive) ||
        entry.is(Engine::Core::WorldSpatialIndex::k_building) ||
        entry.is(Engine::Core::WorldSpatialIndex::k_pending_removal)) {
      continue;
    }
    ++units_processed;

    Body& body = bodies[slot];
    body.core_radius = CommandService::get_unit_radii(world, entry.id).core;
    widest_core = std::max(widest_core, body.core_radius);

    if (const auto* movement =
            context.try_get<Engine::Core::MovementComponent>(entry.id)) {
      body.navigation_clearance = movement->get_navigation_clearance();
      body.can_enter_forest = movement->get_can_enter_forest();
    }
    if (const auto* facts =
            context.try_get<Engine::Core::MovementFactsComponent>(entry.id)) {
      body.under_way = facts->desired.valid;
      body.stopped_by_terrain = facts->progress.blocked_steps > 0U;
      body.pace = facts->last_accepted_speed;
      float const speed = Game::Systems::planar_length(facts->desired.velocity_x,
                                                       facts->desired.velocity_z);
      if (facts->desired.valid && speed > 1.0e-4F) {
        body.has_travel = true;
        body.travel_x = facts->desired.velocity_x / speed;
        body.travel_z = facts->desired.velocity_z / speed;
      }
    }
    if (const auto* formation =
            context.try_get<Engine::Core::FormationModeComponent>(entry.id)) {
      body.formation_id = formation->formation_id;
    }
    if (const auto* attack = context.try_get<Engine::Core::AttackComponent>(entry.id);
        attack != nullptr && attack->in_melee_lock) {
      body.engaged_target = attack->melee_lock_target_id;
    } else if (const auto* target =
                   context.try_get<Engine::Core::AttackTargetComponent>(entry.id)) {
      body.engaged_target = target->target_id;
    } else if (const auto* wildlife =
                   context.try_get<Engine::Core::WildlifeComponent>(entry.id);
               wildlife != nullptr &&
               wildlife->behavior == Game::Wildlife::Behavior::Stalk) {
      body.engaged_target = wildlife->focus_id;
    }
  }

  const Engine::Core::WorldSpatialIndex::Entry* const first = entries.data();
  std::uint32_t neighbors_examined = 0;
  std::uint32_t units_steered = 0;

  for (std::size_t slot = 0; slot < entries.size(); ++slot) {
    const auto& entry = entries[slot];
    const Body& me = bodies[slot];

    auto* facts = context.try_get<Engine::Core::MovementFactsComponent>(entry.id);
    if (facts == nullptr) {
      continue;
    }
    if (!me.under_way || !facts->desired.valid) {
      facts->steering.valid = false;
      continue;
    }

    const float desired_vx = facts->desired.velocity_x;
    const float desired_vz = facts->desired.velocity_z;
    const float desired_speed = Game::Systems::planar_length(desired_vx, desired_vz);

    float const contact_push_x = facts->steering.contact_push_x;
    float const contact_push_z = facts->steering.contact_push_z;
    float const body_overlap = facts->steering.body_overlap;
    facts->steering = {};
    facts->steering.contact_push_x = contact_push_x;
    facts->steering.contact_push_z = contact_push_z;
    facts->steering.body_overlap = body_overlap;
    facts->steering.valid = true;
    facts->steering.velocity_x = desired_vx;
    facts->steering.velocity_z = desired_vz;
    facts->steering.result = Engine::Core::SteeringResult::Unconstrained;
    if (desired_speed <= 1.0e-4F) {
      continue;
    }

    const float tx = desired_vx / desired_speed;
    const float tz = desired_vz / desired_speed;
    const float right_x = tz;
    const float right_z = -tx;

    const float lookahead = desired_speed * k_lookahead_seconds;
    const float query_radius =
        me.core_radius + widest_core + k_separation_radius + lookahead;

    float speed_cap = desired_speed;
    float lean_x = 0.0F;
    float lean_z = 0.0F;
    std::uint32_t neighbor_count = 0;
    bool rank_ahead = false;

    index.for_each_in_radius(entry.x, entry.z, query_radius, [&](const auto& other) {
      ++neighbors_examined;
      if (other.id == entry.id) {
        return;
      }
      if (!other.is(Engine::Core::WorldSpatialIndex::k_alive) ||
          other.is(Engine::Core::WorldSpatialIndex::k_building) ||
          other.is(Engine::Core::WorldSpatialIndex::k_pending_removal)) {
        return;
      }
      if (other.owner_id != entry.owner_id) {
        return;
      }
      if (other.id == me.engaged_target) {
        return;
      }
      const auto neighbor_slot = static_cast<std::size_t>(&other - first);
      const Body& them = bodies[neighbor_slot];
      if (entry.id == them.engaged_target) {
        return;
      }

      const float px = other.x - entry.x;
      const float pz = other.z - entry.z;
      const float along = (px * tx) + (pz * tz);
      if (along <= 0.0F) {
        return;
      }
      const float lane = me.core_radius + them.core_radius + k_separation_radius;
      const float cross = (tx * pz) - (tz * px);
      if (std::abs(cross) >= lane) {
        return;
      }
      const float reach = lane + lookahead;
      if (along >= reach) {
        return;
      }
      ++neighbor_count;

      const float gap =
          along - std::sqrt(std::max(0.0F, (lane * lane) - (cross * cross)));
      const float heading_agreement =
          them.has_travel ? (them.travel_x * tx) + (them.travel_z * tz) : 0.0F;

      bool const mutual =
          them.has_travel && ((-px * them.travel_x) + (-pz * them.travel_z)) > 0.0F;
      bool const i_go_first = mutual && entry.id < other.id;
      if (them.under_way && heading_agreement > 0.0F && !i_go_first) {

        float closing_horizon = k_follow_close_seconds;
        if (them.pace > 1.0e-3F) {
          float const lateral_speed =
              them.pace *
              std::sqrt(std::max(0.0F, 1.0F - heading_agreement * heading_agreement));
          if (lateral_speed > 1.0e-3F) {
            float const leaves_in = (lane - std::abs(cross)) / lateral_speed;
            closing_horizon = std::clamp(leaves_in, delta_time, k_follow_close_seconds);
          }
        }
        const float closing_allowance =
            std::max(0.0F, gap - k_follow_gap) / closing_horizon;
        float cap = (them.pace * heading_agreement) + closing_allowance;
        if (!them.stopped_by_terrain) {
          cap = std::max(cap, desired_speed * k_min_speed_fraction);
        }
        speed_cap = std::min(speed_cap, cap);
      }

      if (me.formation_id != 0U && them.formation_id == me.formation_id) {
        rank_ahead = true;
      }

      const float closeness =
          std::clamp((reach - along) / std::max(1.0e-4F, reach - lane), 0.0F, 1.0F);
      const float lateral_falloff =
          1.0F - std::clamp(std::abs(cross) / lane, 0.0F, 1.0F);
      const float weight = closeness * lateral_falloff;

      const float head_on_band = lane * k_head_on_lane_fraction;
      float side = 1.0F;
      if (cross < 0.0F) {
        side = std::clamp(
            1.0F - (2.0F * std::abs(cross) / std::max(1.0e-4F, head_on_band)),
            -1.0F,
            1.0F);
      }
      lean_x += right_x * weight * side;
      lean_z += right_z * weight * side;
    });

    facts->steering.neighbor_count = neighbor_count;
    if (neighbor_count == 0U) {
      continue;
    }

    if (const float leaned = Game::Systems::planar_length(lean_x, lean_z);
        leaned > k_lean_gain) {
      const float trim = k_lean_gain / leaned;
      lean_x *= trim;
      lean_z *= trim;
    }

    Point const own_cell = NavGrid::world_to_grid(entry.x, entry.z);
    bool has_room = !rank_ahead && !terrain.is_on_bridge(entry.x, entry.z) &&
                    !terrain.is_hill_entrance(own_cell.x, own_cell.y) &&
                    !buildings.point_in_navigation_passage(entry.x, entry.z);

    float lean_magnitude = Game::Systems::planar_length(lean_x, lean_z);
    if (has_room && lean_magnitude > 1.0e-4F) {
      BodyProfile profile;
      profile.radius = me.navigation_clearance;
      profile.passability = me.can_enter_forest ? Pathfinding::Passability::Light
                                                : Pathfinding::Passability::Heavy;
      const float probe_distance = std::max(0.75F, me.core_radius + 0.25F);
      QVector3D const probe(entry.x + (lean_x / lean_magnitude * probe_distance),
                            0.0F,
                            entry.z + (lean_z / lean_magnitude * probe_distance));
      if (!Walkability::can_stand(probe, profile)) {
        has_room = false;
      }
    }
    if (!has_room) {
      lean_x = 0.0F;
      lean_z = 0.0F;
      lean_magnitude = 0.0F;
    }

    const float speed_scale = std::clamp(speed_cap / desired_speed, 0.0F, 1.0F);
    float steered_vx = (desired_vx * speed_scale) + (lean_x * desired_speed);
    float steered_vz = (desired_vz * speed_scale) + (lean_z * desired_speed);

    if (const float steered_speed =
            Game::Systems::planar_length(steered_vx, steered_vz);
        steered_speed > desired_speed && steered_speed > 1.0e-4F) {
      const float trim = desired_speed / steered_speed;
      steered_vx *= trim;
      steered_vz *= trim;
    }

    facts->steering.velocity_x = steered_vx;
    facts->steering.velocity_z = steered_vz;
    facts->steering.correction_x = steered_vx - desired_vx;
    facts->steering.correction_z = steered_vz - desired_vz;

    if (Game::Systems::planar_length(facts->steering.correction_x,
                                     facts->steering.correction_z) > 1.0e-4F) {
      ++units_steered;
      facts->steering.result =
          speed_scale < k_yield_speed_fraction
              ? Engine::Core::SteeringResult::Yielded
              : (lean_magnitude > 1.0e-4F ? Engine::Core::SteeringResult::Deviated
                                          : Engine::Core::SteeringResult::Slowed);
    }
  }

  m_diagnostics.units_processed = units_processed;
  m_diagnostics.units_steered = units_steered;
  if (units_processed > 0U) {
    m_diagnostics.average_neighbors_checked = neighbors_examined / units_processed;
  }
}

auto LocalAvoidanceSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(Reads<UnitComponent,
                                     TransformComponent,
                                     AttackComponent,
                                     AttackTargetComponent,
                                     WildlifeComponent,
                                     MovementComponent,
                                     FormationModeComponent,
                                     BuildingComponent,
                                     PendingRemovalComponent>{},
                               Writes<MovementFactsComponent>{});
}

} // namespace Game::Systems
