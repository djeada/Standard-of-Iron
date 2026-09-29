#include "wildlife_nature_actions.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../audio/cue_ids.h"
#include "../core/component.h"
#include "../core/entity.h"
#include "../core/event_manager.h"
#include "../core/world.h"
#include "../systems/combat_system/combat_utils.h"
#include "../systems/movement/command_service.h"
#include "../systems/navigation/nav_grid.h"
#include "../systems/navigation/walkability.h"
#include "wildlife_spawner.h"

namespace Game::Wildlife {

namespace {

constexpr float k_animal_navigation_radius = 0.0F;
constexpr float k_move_reissue_epsilon = 0.75F;

void issue_move(Engine::Core::World& world,
                Engine::Core::EntityID entity_id,
                float world_x,
                float world_z) {

  Game::Systems::BodyProfile profile;
  profile.radius = k_animal_navigation_radius;
  QVector3D const requested(world_x, 0.0F, world_z);
  auto const standable =
      Game::Systems::Walkability::nearest_standable(requested, profile, 24.0F);

  QVector3D destination =
      Game::Systems::NavGrid::snap_to_walkable_ground(standable.value_or(requested));
  if (standable.has_value()) {
    destination.setX(standable->x());
    destination.setZ(standable->z());
  }

  auto* entity = world.get_entity(entity_id);
  if (entity != nullptr) {
    const auto* movement = entity->get_component<Engine::Core::MovementComponent>();
    if (movement != nullptr && movement->get_has_target()) {
      float const dx = movement->get_goal_x() - destination.x();
      float const dz = movement->get_goal_y() - destination.z();
      if ((dx * dx) + (dz * dz) <= k_move_reissue_epsilon * k_move_reissue_epsilon) {
        return;
      }
    }
  }

  Game::Systems::CommandService::MoveOptions options;
  options.kind = Game::Systems::MoveOrderKind::ScriptedMove;
  Game::Systems::CommandService::move_unit(world, entity_id, destination, options);
}

} // namespace

WildlifeNatureActions::WildlifeNatureActions(Engine::Core::World& world,
                                             WildlifeCensus& census,
                                             WildlifePredation& predation,
                                             WildlifeStats& stats)
    : m_world(world)
    , m_census(census)
    , m_predation(predation)
    , m_stats(stats) {
}

void WildlifeNatureActions::move_to(const NatureContext& ctx,
                                    float world_x,
                                    float world_z) {
  issue_move(m_world, ctx.entity->get_id(), world_x, world_z);
}

auto WildlifeNatureActions::bypass_around_obstacle(const NatureContext& ctx,
                                                   float prey_x,
                                                   float prey_z,
                                                   float standoff)
    -> std::optional<std::pair<float, float>> {
  QVector3D const from(ctx.x, 0.0F, ctx.z);
  QVector3D const prey(prey_x, 0.0F, prey_z);
  if (!Game::Systems::Combat::structure_separates_positions(from, prey)) {
    return std::nullopt;
  }

  auto const around =
      Game::Systems::Combat::melee_bypass_destination(from, prey, standoff, 0.0F);
  if (!around.has_value()) {
    return std::nullopt;
  }
  return std::make_pair(around->x(), around->z());
}

void WildlifeNatureActions::halt(const NatureContext& ctx) {
  if (ctx.movement != nullptr) {
    ctx.movement->stop();
  }
}

void WildlifeNatureActions::face_toward(const NatureContext& ctx,
                                        float world_x,
                                        float world_z) {
  auto* transform = ctx.entity->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }
  float const dx = world_x - transform->position.x;
  float const dz = world_z - transform->position.z;
  if ((dx * dx) + (dz * dz) < 1e-4F) {
    return;
  }
  transform->desired_yaw = std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
  transform->has_desired_yaw = true;
}

void WildlifeNatureActions::set_travel_speed(const NatureContext& ctx, bool urgent) {
  auto* unit = ctx.entity->get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr) {
    return;
  }
  unit->speed = urgent ? ctx.config->flee_speed : ctx.config->move_speed;
}

void WildlifeNatureActions::alert_herd(std::uint16_t group_id, float duration) {
  m_census.alert_group(group_id, duration);
}

void WildlifeNatureActions::mark_hostile(const NatureContext& ctx,
                                         Engine::Core::EntityID foe_id,
                                         bool rally_pack) {
  auto& wildlife = *ctx.wildlife;
  wildlife.hostile_timer = Game::Wildlife::k_hostility_duration;
  wildlife.aggressor_id = foe_id;
  if (rally_pack) {
    m_census.rally_pack(
        wildlife.group_id, foe_id, Game::Wildlife::k_hostility_duration);
  }
}

void WildlifeNatureActions::note(const NatureContext& ctx, NatureEvent event) {
  const char* cue_id = nullptr;
  switch (event) {
  case NatureEvent::Flee:
    m_stats.flee_events += 1U;
    if (ctx.wildlife != nullptr &&
        ctx.wildlife->species == Game::Wildlife::Species::Sheep) {
      cue_id = Game::Audio::Cue::k_wildlife_sheep_alarm;
    }
    break;
  case NatureEvent::Hunt:
    m_stats.hunt_events += 1U;
    cue_id = Game::Audio::Cue::k_wildlife_wolf_hunt;
    break;
  }
  if (cue_id == nullptr) {
    return;
  }
  Engine::Core::AudioCueEvent cue(cue_id);
  const auto* transform =
      (ctx.world != nullptr && ctx.entity != nullptr)
          ? ctx.world->try_get<Engine::Core::TransformComponent>(ctx.entity->get_id())
          : nullptr;
  cue.at(ctx.x, transform != nullptr ? transform->position.y : 0.0F, ctx.z);
  Engine::Core::EventManager::instance().publish(cue);
}

auto WildlifeNatureActions::pick_open_point(std::uint32_t& rng,
                                            float origin_x,
                                            float origin_z,
                                            float min_radius,
                                            float max_radius,
                                            float& out_x,
                                            float& out_z) -> bool {
  return Game::Wildlife::pick_open_point(
      rng, origin_x, origin_z, min_radius, max_radius, out_x, out_z);
}

auto WildlifeNatureActions::nearest_prey(const NatureContext& ctx,
                                         float radius) -> PreyRef {
  const AnimalRef* found = m_census.nearest_prey(
      ctx.x, ctx.z, radius, ctx.entity->get_id(), ctx.config->aggression);
  if (found == nullptr) {
    return {};
  }
  return resolve_prey(m_world, found->id, ctx.x, ctx.z);
}

auto WildlifeNatureActions::nearest_quarry(const NatureContext& ctx,
                                           float radius) -> PreyRef {
  const QuarryRef* found = m_census.nearest_quarry(
      ctx.x, ctx.z, radius, ctx.entity->get_id(), ctx.config->aggression);
  if (found == nullptr) {
    return {};
  }
  return resolve_prey(m_world, found->id, ctx.x, ctx.z);
}

auto WildlifeNatureActions::locate(const NatureContext& ctx,
                                   Engine::Core::EntityID entity_id) -> PreyRef {
  return resolve_prey(m_world, entity_id, ctx.x, ctx.z);
}

auto WildlifeNatureActions::claim_pack_slot(const NatureContext& ctx,
                                            const PreyRef& prey) -> PackSlot {
  return m_census.pack_slot_for(prey.id, ctx.entity->get_id());
}

auto WildlifeNatureActions::nearest_pack_hunter(float world_x,
                                                float world_z,
                                                float radius) -> ThreatQuery {
  return m_census.nearest_pack_hunter(world_x, world_z, radius);
}

auto WildlifeNatureActions::bite(const NatureContext& ctx,
                                 const PreyRef& prey) -> bool {
  return m_predation.begin_bite(*ctx.entity, *ctx.wildlife, prey, ctx.x, ctx.z);
}

} // namespace Game::Wildlife
