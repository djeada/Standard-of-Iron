#include <QDebug>
#include <QVector2D>
#include <QVector3D>
#include <QtMath>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "app/orders/movement_utils.h"
#include "arena_scenario.h"
#include "arena_viewport.h"
#include "arena_viewport_internal.h"
#include "game/core/world.h"
#include "game/map/terrain.h"
#include "game/map/terrain_service.h"
#include "game/session/selection_service.h"
#include "game/session/session_context.h"
#include "game/systems/ai_system.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/raft_system.h"
#include "game/systems/rockfall_system.h"
#include "game/systems/undead_awakening_system.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"
#include "game/units/unit.h"
#include "game/wildlife/bird_flock.h"
#include "game/wildlife/wildlife_system.h"
#include "render/ground/ambient_fog_renderer.h"
#include "render/ground/rain_renderer.h"
#include "render/ground/terrain_scatter_manager.h"
#include "render/profiling/combat_animation_diagnostics.h"
#include "render/profiling/frame_continuity_analyzer.h"
#include "unit_spawn_options.h"

using namespace arena_viewport_internal;

namespace {

constexpr float k_unit_spawn_clearance = 3.25F;
constexpr float k_building_spawn_clearance = 5.0F;
constexpr float k_spawn_search_step_factor = 1.1F;
constexpr int k_max_spawn_search_ring = 20;

auto grid_position_from_world(const Game::Map::TerrainField& field,
                              const QVector3D& world_position) -> QVector2D {
  float const half_width = static_cast<float>(field.width) * 0.5F - 0.5F;
  float const half_height = static_cast<float>(field.height) * 0.5F - 0.5F;
  float const grid_x = world_position.x() / field.tile_size + half_width;
  float const grid_z = world_position.z() / field.tile_size + half_height;
  return {std::clamp(grid_x, 0.0F, static_cast<float>(field.width - 1)),
          std::clamp(grid_z, 0.0F, static_cast<float>(field.height - 1))};
}

auto mix_seed(std::uint32_t seed, std::uint32_t value) -> std::uint32_t {
  seed ^= value + 0x9E3779B9U + (seed << 6U) + (seed >> 2U);
  return seed;
}

auto unit_float(std::uint32_t seed, std::uint32_t stream) -> float {
  std::uint32_t hashed = mix_seed(seed, stream);
  hashed ^= hashed >> 16U;
  hashed *= 0x7FEB352DU;
  hashed ^= hashed >> 15U;
  hashed *= 0x846CA68BU;
  hashed ^= hashed >> 16U;
  return static_cast<float>(hashed & 0xFFFFFFU) / static_cast<float>(0x1000000U);
}

auto signed_unit(std::uint32_t seed, std::uint32_t stream) -> float {
  return (unit_float(seed, stream) * 2.0F) - 1.0F;
}

auto variation_seed(const QString& prop_type,
                    const QVector3D& origin,
                    int count) -> std::uint32_t {
  std::uint32_t seed = static_cast<std::uint32_t>(qHash(prop_type));
  seed = mix_seed(seed, static_cast<std::uint32_t>(std::lround(origin.x() * 16.0F)));
  seed = mix_seed(seed, static_cast<std::uint32_t>(std::lround(origin.z() * 16.0F)));
  return mix_seed(seed, static_cast<std::uint32_t>(count));
}

auto world_prop_type_from_string(const QString& prop_type)
    -> Game::Map::WorldProp::Type {
  QString const normalized = prop_type.trimmed().toLower();
  if (normalized == QStringLiteral("firecamp") ||
      normalized == QStringLiteral("fire_camp")) {
    return Game::Map::WorldProp::Type::FireCamp;
  }
  if (normalized == QStringLiteral("tent")) {
    return Game::Map::WorldProp::Type::Tent;
  }
  if (normalized == QStringLiteral("supply_cart")) {
    return Game::Map::WorldProp::Type::SupplyCart;
  }
  if (normalized == QStringLiteral("weapon_rack")) {
    return Game::Map::WorldProp::Type::WeaponRack;
  }
  if (normalized == QStringLiteral("ruins")) {
    return Game::Map::WorldProp::Type::Ruins;
  }
  if (normalized == QStringLiteral("magic_shrine")) {
    return Game::Map::WorldProp::Type::MagicShrine;
  }
  if (normalized == QStringLiteral("dead_tree")) {
    return Game::Map::WorldProp::Type::DeadTree;
  }
  if (normalized == QStringLiteral("boulder")) {
    return Game::Map::WorldProp::Type::Boulder;
  }
  if (normalized == QStringLiteral("pine_tree") ||
      normalized == QStringLiteral("pine")) {
    return Game::Map::WorldProp::Type::PineTree;
  }
  if (normalized == QStringLiteral("olive_tree") ||
      normalized == QStringLiteral("olive")) {
    return Game::Map::WorldProp::Type::OliveTree;
  }
  if (normalized == QStringLiteral("cypress_tree") ||
      normalized == QStringLiteral("cypress")) {
    return Game::Map::WorldProp::Type::CypressTree;
  }
  if (normalized == QStringLiteral("palm_tree") ||
      normalized == QStringLiteral("palm")) {
    return Game::Map::WorldProp::Type::PalmTree;
  }
  if (normalized == QStringLiteral("plant")) {
    return Game::Map::WorldProp::Type::Plant;
  }
  if (normalized == QStringLiteral("iron_ore")) {
    return Game::Map::WorldProp::Type::IronOre;
  }
  if (normalized == QStringLiteral("abandoned_home")) {
    return Game::Map::WorldProp::Type::AbandonedHome;
  }
  if (normalized == QStringLiteral("statue")) {
    return Game::Map::WorldProp::Type::Statue;
  }
  if (normalized == QStringLiteral("cursed_gold_vein") ||
      normalized == QStringLiteral("gold_vein")) {
    return Game::Map::WorldProp::Type::CursedGoldVein;
  }

  qWarning() << "Arena: unknown world prop type" << prop_type
             << "- falling back to a fire camp";
  return Game::Map::WorldProp::Type::FireCamp;
}

auto entity_spawn_clearance(const Engine::Core::Entity& entity) -> float {
  return entity.has_component<Engine::Core::BuildingComponent>()
             ? k_building_spawn_clearance
             : k_unit_spawn_clearance;
}

} // namespace

auto ArenaViewport::resolve_spawn_anchor_world() const -> QVector3D {
  if (m_spawn_anchor_world_valid) {
    return App::Utils::snap_to_walkable_ground(m_spawn_anchor_world);
  }

  return App::Utils::snap_to_walkable_ground(
      m_session.terrain().resolve_surface_world_position(0.0F, 0.0F, 0.0F, 0.0F));
}

auto ArenaViewport::is_spawn_position_available(const QVector3D& position,
                                                float clearance) const -> bool {
  if (m_session.terrain().is_forbidden_world(position.x(), position.z())) {
    return false;
  }

  if (m_world == nullptr) {
    return true;
  }

  for (const auto& unit : m_units) {
    if (unit == nullptr) {
      continue;
    }

    auto* entity = m_world->get_entity(unit->id());
    auto* transform = entity != nullptr
                          ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    auto* unit_component = entity != nullptr
                               ? entity->get_component<Engine::Core::UnitComponent>()
                               : nullptr;
    if (entity == nullptr || transform == nullptr || unit_component == nullptr ||
        unit_component->health <= 0 ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    float const dx = transform->position.x - position.x();
    float const dz = transform->position.z - position.z();
    float const min_distance = std::max(clearance, entity_spawn_clearance(*entity));
    if ((dx * dx + dz * dz) < (min_distance * min_distance)) {
      return false;
    }
  }

  return true;
}

auto ArenaViewport::find_available_spawn_position(const QVector3D& anchor,
                                                  float clearance) const -> QVector3D {
  auto resolve_candidate = [this, anchor](float world_x, float world_z) {
    return App::Utils::snap_to_walkable_ground(
        m_session.terrain().resolve_surface_world_position(
            world_x, world_z, 0.0F, anchor.y()));
  };

  QVector3D const snapped_anchor = resolve_candidate(anchor.x(), anchor.z());
  if (is_spawn_position_available(snapped_anchor, clearance)) {
    return snapped_anchor;
  }

  float const step = std::max(1.0F, clearance * k_spawn_search_step_factor);
  for (int ring = 1; ring <= k_max_spawn_search_ring; ++ring) {
    for (int grid_z = -ring; grid_z <= ring; ++grid_z) {
      for (int grid_x = -ring; grid_x <= ring; ++grid_x) {
        if (std::max(std::abs(grid_x), std::abs(grid_z)) != ring) {
          continue;
        }

        QVector3D const candidate =
            resolve_candidate(anchor.x() + static_cast<float>(grid_x) * step,
                              anchor.z() + static_cast<float>(grid_z) * step);
        if (is_spawn_position_available(candidate, clearance)) {
          return candidate;
        }
      }
    }
  }

  return snapped_anchor;
}

void ArenaViewport::set_spawn_owner(int owner_id) {
  if (owner_id == k_enemy_owner_id) {
    m_spawn_owner_id = k_enemy_owner_id;
    return;
  }
  m_spawn_owner_id = k_local_owner_id;
}

void ArenaViewport::set_spawn_nation(const QString& nation_id) {
  Game::Systems::NationID parsed{};
  if (!Game::Systems::try_parse_nation_id(nation_id, parsed)) {
    return;
  }
  m_spawn_nation_id = parsed;
  sync_spawn_selection_defaults();
}

void ArenaViewport::set_spawn_unit_type(const QString& unit_type) {
  if (auto special = Arena::UnitSpawnOptions::parse_special_unit_option(unit_type);
      special.has_value()) {
    m_spawn_unit_type = special->troop_type;
    return;
  }

  Game::Units::TroopType parsed{};
  if (!Game::Units::try_parse_troop_type(unit_type, parsed)) {
    return;
  }
  m_spawn_unit_type = parsed;
}

void ArenaViewport::set_spawn_individuals_per_unit(int count) {
  m_spawn_individuals_per_unit_override = std::max(0, count);
  auto* selection = selection_system();
  if (selection != nullptr && !selection->get_selected_units().empty()) {
    apply_visual_overrides_to_selection();
  }
}

void ArenaViewport::set_spawn_rider_visible(bool visible) {
  m_spawn_rider_visible = visible;
  auto* selection = selection_system();
  if (selection != nullptr && !selection->get_selected_units().empty()) {
    apply_visual_overrides_to_selection();
  }
}

void ArenaViewport::spawn_unit() {
  spawn_units(1);
}

void ArenaViewport::spawn_units(int count) {
  int const clamped_count = std::clamp(count, 1, 128);
  std::vector<Engine::Core::EntityID> spawned_ids;
  spawned_ids.reserve(static_cast<size_t>(clamped_count));

  for (int i = 0; i < clamped_count; ++i) {
    Engine::Core::EntityID const entity_id =
        spawn_single_unit(m_spawn_owner_id, m_spawn_nation_id, m_spawn_unit_type);
    if (entity_id != 0U) {
      spawned_ids.push_back(entity_id);
    }
  }

  if (spawned_ids.empty()) {
    return;
  }

  select_spawned_entities(spawned_ids);
  update();
}

void ArenaViewport::spawn_opposing_batch(int count) {
  int const clamped_count = std::clamp(count, 1, 128);
  int const owner_id =
      m_spawn_owner_id == k_enemy_owner_id ? k_local_owner_id : k_enemy_owner_id;
  std::vector<Engine::Core::EntityID> spawned_ids;
  spawned_ids.reserve(static_cast<size_t>(clamped_count));

  for (int i = 0; i < clamped_count; ++i) {
    Engine::Core::EntityID const entity_id =
        spawn_single_unit(owner_id, m_spawn_nation_id, m_spawn_unit_type);
    if (entity_id != 0U) {
      spawned_ids.push_back(entity_id);
    }
  }

  if (spawned_ids.empty()) {
    return;
  }

  select_spawned_entities(spawned_ids);
  update();
}

void ArenaViewport::spawn_mirror_match(int count) {
  int const clamped_count = std::clamp(count, 1, 128);
  std::vector<Engine::Core::EntityID> spawned_ids;
  spawned_ids.reserve(static_cast<size_t>(clamped_count * 2));

  for (int const owner_id : {k_local_owner_id, k_enemy_owner_id}) {
    for (int i = 0; i < clamped_count; ++i) {
      Engine::Core::EntityID const entity_id =
          spawn_single_unit(owner_id, m_spawn_nation_id, m_spawn_unit_type);
      if (entity_id != 0U) {
        spawned_ids.push_back(entity_id);
      }
    }
  }

  if (spawned_ids.empty()) {
    return;
  }

  select_spawned_entities(spawned_ids);
  update();
}

void ArenaViewport::apply_visual_overrides_to_selection() {
  sanitize_selection();
  auto* selection = selection_system();
  if (selection == nullptr || m_world == nullptr) {
    return;
  }

  for (auto entity_id : selection->get_selected_units()) {
    auto* entity = m_world->get_entity(entity_id);
    auto* unit_component = entity != nullptr
                               ? entity->get_component<Engine::Core::UnitComponent>()
                               : nullptr;
    if (unit_component == nullptr || unit_component->health <= 0 ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }
    unit_component->render_individuals_per_unit_override =
        m_spawn_individuals_per_unit_override;
    unit_component->render_rider = m_spawn_rider_visible;
  }

  update();
}

void ArenaViewport::select_spawned_entities(
    const std::vector<Engine::Core::EntityID>& ids) {
  auto* selection = selection_system();
  if (selection == nullptr) {
    return;
  }

  selection->clear_selection();
  for (auto entity_id : ids) {
    selection->select_unit(entity_id);
  }
}

auto ArenaViewport::spawn_single_unit() -> Engine::Core::EntityID {
  return spawn_single_unit(m_spawn_owner_id, m_spawn_nation_id, m_spawn_unit_type);
}

auto ArenaViewport::resolve_spawn_unit_type(Game::Systems::NationID nation_id,
                                            Game::Units::TroopType preferred) const
    -> Game::Units::TroopType {

  (void)nation_id;
  return preferred;
}

auto ArenaViewport::spawn_single_unit(int owner_id,
                                      Game::Systems::NationID nation_id,
                                      Game::Units::TroopType unit_type)
    -> Engine::Core::EntityID {
  QVector3D const spawn_position = find_available_spawn_position(
      resolve_spawn_anchor_world(), k_unit_spawn_clearance);
  return spawn_single_unit(
      owner_id, nation_id, unit_type, spawn_position, owner_id == k_enemy_owner_id);
}

auto ArenaViewport::spawn_single_unit(int owner_id,
                                      Game::Systems::NationID nation_id,
                                      Game::Units::TroopType unit_type,
                                      const QVector3D& spawn_position,
                                      bool ai_controlled,
                                      bool keep_troop_speed) -> Engine::Core::EntityID {
  if (m_unit_factory == nullptr || m_world == nullptr) {
    return 0U;
  }

  Game::Units::TroopType const resolved_unit_type =
      resolve_spawn_unit_type(nation_id, unit_type);

  Game::Units::SpawnParams params;
  params.position = spawn_position;
  params.player_id = owner_id;
  params.spawn_type = Game::Units::spawn_typeFromTroopType(resolved_unit_type);
  params.ai_controlled = ai_controlled;
  params.nation_id = nation_id;

  auto unit = m_unit_factory->create(resolved_unit_type, *m_world, params);
  if (unit == nullptr) {
    return 0U;
  }

  auto* entity = m_world->get_entity(unit->id());
  auto* transform = entity != nullptr
                        ? entity->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  auto* unit_component = entity != nullptr
                             ? entity->get_component<Engine::Core::UnitComponent>()
                             : nullptr;
  if (transform != nullptr && owner_id == k_enemy_owner_id) {
    transform->rotation.y = 180.0F;
    transform->desired_yaw = 180.0F;
    transform->has_desired_yaw = true;
  }
  if (unit_component != nullptr) {
    if (!keep_troop_speed) {
      unit_component->speed = m_default_unit_speed;
    }
    unit_component->render_individuals_per_unit_override =
        m_spawn_individuals_per_unit_override;
    unit_component->render_rider = m_spawn_rider_visible;
  }

  Engine::Core::EntityID const entity_id = unit->id();
  m_units.push_back(std::move(unit));
  return entity_id;
}

auto ArenaViewport::find_unit_handle(Engine::Core::EntityID entity_id) const
    -> Game::Units::Unit* {
  auto it = std::find_if(m_units.begin(),
                         m_units.end(),
                         [entity_id](const std::unique_ptr<Game::Units::Unit>& unit) {
                           return unit != nullptr && unit->id() == entity_id;
                         });
  return it != m_units.end() ? it->get() : nullptr;
}

void ArenaViewport::clear_units() {
  if (m_world == nullptr) {
    return;
  }

  auto* selection = selection_system();
  if (selection != nullptr) {
    selection->clear_selection();
  }
  for (const auto& unit : m_units) {
    if (unit != nullptr) {
      m_world->destroy_entity(unit->id());
    }
  }
  m_units.clear();
  m_hovered_entity_id = 0;
  update();
}

void ArenaViewport::set_spawn_building_owner(int owner_id) {
  m_spawn_building_owner_id =
      (owner_id == k_enemy_owner_id) ? k_enemy_owner_id : k_local_owner_id;
}

void ArenaViewport::set_spawn_building_nation(const QString& nation_id) {
  Game::Systems::NationID parsed{};
  if (!Game::Systems::try_parse_nation_id(nation_id, parsed)) {
    return;
  }
  m_spawn_building_nation_id = parsed;
}

void ArenaViewport::set_spawn_building_type(const QString& building_type) {
  Game::Units::SpawnType parsed{};
  if (!Game::Units::try_parse_spawn_type(building_type, parsed) ||
      !Game::Units::is_building_spawn(parsed)) {
    return;
  }
  m_spawn_building_type = parsed;
}

void ArenaViewport::spawn_buildings(int count) {
  int const clamped_count = std::clamp(count, 1, 16);
  std::vector<Engine::Core::EntityID> spawned_ids;
  spawned_ids.reserve(static_cast<size_t>(clamped_count));

  for (int i = 0; i < clamped_count; ++i) {
    Engine::Core::EntityID const entity_id = spawn_single_building(
        m_spawn_building_owner_id, m_spawn_building_nation_id, m_spawn_building_type);
    if (entity_id != 0U) {
      spawned_ids.push_back(entity_id);
    }
  }

  if (spawned_ids.empty()) {
    return;
  }

  select_spawned_entities(spawned_ids);
  update();
}

auto ArenaViewport::spawn_single_building(int owner_id,
                                          Game::Systems::NationID nation_id,
                                          Game::Units::SpawnType building_type,
                                          std::optional<QVector3D> requested_position,
                                          bool ai_controlled,
                                          int max_population,
                                          float rotation_y) -> Engine::Core::EntityID {
  if (m_unit_factory == nullptr || m_world == nullptr) {
    return 0U;
  }
  QVector3D const spawn_position =
      requested_position.has_value()
          ? m_session.terrain().resolve_surface_world_position(requested_position->x(),
                                                               requested_position->z(),
                                                               0.0F,
                                                               requested_position->y())
          : find_available_spawn_position(resolve_spawn_anchor_world(),
                                          k_building_spawn_clearance);

  Game::Units::SpawnParams params;
  params.position = spawn_position;
  params.player_id = owner_id;
  params.spawn_type = building_type;
  params.ai_controlled = ai_controlled;
  params.nation_id = nation_id;
  if (max_population > 0) {
    params.max_population = max_population;
  }
  params.rotation_y = rotation_y;

  auto unit = m_unit_factory->create(building_type, *m_world, params);
  if (unit == nullptr) {
    return 0U;
  }

  Engine::Core::EntityID const entity_id = unit->id();

  m_session.building_collision().register_building(
      entity_id,
      Game::Units::spawn_typeToQString(building_type).toStdString(),
      spawn_position.x(),
      spawn_position.z(),
      owner_id);
  if (rotation_y != 0.0F) {
    auto& collision = m_session.building_collision();
    collision.resize_building(
        entity_id,
        Game::Systems::BuildingCollisionRegistry::axis_aligned_size(
            Game::Systems::BuildingCollisionRegistry::get_building_size(
                Game::Units::spawn_typeToQString(building_type).toStdString()),
            rotation_y));
  }

  m_units.push_back(std::move(unit));
  return entity_id;
}

void ArenaViewport::clear_buildings() {
  if (m_world == nullptr) {
    return;
  }

  auto* selection = selection_system();

  auto it = m_units.begin();
  while (it != m_units.end()) {
    if (*it == nullptr) {
      it = m_units.erase(it);
      continue;
    }
    auto* entity = m_world->get_entity((*it)->id());
    auto* unit_component = entity != nullptr
                               ? entity->get_component<Engine::Core::UnitComponent>()
                               : nullptr;
    if (unit_component != nullptr &&
        Game::Units::is_building_spawn(unit_component->spawn_type)) {
      if (selection != nullptr) {
        selection->deselect_unit((*it)->id());
      }
      m_session.building_collision().unregister_building((*it)->id());
      m_world->destroy_entity((*it)->id());
      it = m_units.erase(it);
    } else {
      ++it;
    }
  }

  m_hovered_entity_id = 0;
  update();
}

void ArenaViewport::set_spawn_world_prop_type(const QString& prop_type) {
  m_spawn_world_prop_type = world_prop_type_from_string(prop_type);
}

void ArenaViewport::set_spawn_world_prop_scale(float value) {
  m_spawn_world_prop_scale = std::max(0.1F, value);
}

void ArenaViewport::set_spawn_world_prop_rotation_degrees(float value) {
  m_spawn_world_prop_rotation = qDegreesToRadians(value);
}

void ArenaViewport::set_spawn_fire_camp_intensity(float value) {
  m_spawn_fire_camp_intensity = std::max(0.1F, value);
}

void ArenaViewport::set_spawn_fire_camp_radius(float value) {
  m_spawn_fire_camp_radius = std::max(0.5F, value);
}

void ArenaViewport::spawn_world_prop() {
  auto& terrain_service = m_session.terrain();
  if (terrain_service.terrain_field().empty()) {
    return;
  }

  QVector3D const anchor = resolve_spawn_anchor_world();
  QVector2D const grid_position =
      grid_position_from_world(terrain_service.terrain_field(), anchor);

  Game::Map::WorldProp prop;
  prop.type = m_spawn_world_prop_type;
  prop.x = grid_position.x();
  prop.z = grid_position.y();
  prop.scale = m_spawn_world_prop_scale;
  prop.rotation = m_spawn_world_prop_rotation;
  prop.intensity = m_spawn_fire_camp_intensity;
  prop.radius = m_spawn_fire_camp_radius;
  prop.persistent = true;
  m_world_props.push_back(prop);
  reconfigure_terrain_from_state();
}

void ArenaViewport::place_scenario_resource_patches(
    const Arena::ArenaScenarioDefinition& definition,
    const QVector3D& scenario_origin) {
  if (definition.resource_patches.empty()) {
    return;
  }

  constexpr float k_prop_building_clearance = 2.6F;

  constexpr float k_prop_gap = 1.0F;

  constexpr float k_prop_dry_margin = 1.0F;

  constexpr float k_prop_road_clearance = 0.25F;

  constexpr std::array<float, 4> k_nudge_rings{0.0F, 1.2F, 2.4F, 3.6F};
  constexpr int k_nudge_directions = 8;

  auto& collision = m_session.building_collision();
  auto& terrain_service = m_session.terrain();
  const auto& terrain_field = terrain_service.terrain_field();

  struct PlacedProp {
    float x{0.0F};
    float z{0.0F};
    float radius{0.0F};
  };
  std::vector<PlacedProp> placed;
  placed.reserve(m_world_props.size() + (definition.resource_patches.size() * 4));

  for (const auto& existing : m_world_props) {
    const QVector3D at = terrain_service.world_prop_world_position(existing);
    placed.push_back(
        {at.x(),
         at.z(),
         Game::Map::world_prop_ground_radius(existing.type, existing.scale)});
  }

  auto const stands_clear = [&](float x, float z, float radius) {
    if (collision.is_circle_overlapping_building(
            x, z, radius + k_prop_building_clearance)) {
      return false;
    }
    if (terrain_service.is_point_near_water(x, z, radius + k_prop_dry_margin) ||
        terrain_service.is_point_near_bridge(x, z, radius + k_prop_dry_margin)) {
      return false;
    }
    if (terrain_service.is_point_near_road(x, z, radius + k_prop_road_clearance)) {
      return false;
    }
    for (const auto& other : placed) {
      float const dx = other.x - x;
      float const dz = other.z - z;
      float const minimum = other.radius + radius + k_prop_gap;
      if (((dx * dx) + (dz * dz)) < (minimum * minimum)) {
        return false;
      }
    }
    return true;
  };

  for (const auto& patch : definition.resource_patches) {
    const auto type = world_prop_type_from_string(patch.prop_type);
    const bool varies =
        patch.jitter > 0.0F || patch.yaw_spread > 0.0F || patch.scale_spread > 0.0F;

    const std::uint32_t patch_seed =
        variation_seed(patch.prop_type, patch.origin, patch.count);

    for (int index = 0; index < patch.count; ++index) {
      QVector3D wanted = scenario_origin + patch.origin + patch.spacing * index;

      float instance_scale = patch.scale;
      float instance_yaw = 0.0F;
      if (varies) {
        const std::uint32_t seed =
            mix_seed(patch_seed, static_cast<std::uint32_t>(index));
        if (patch.jitter > 0.0F) {
          const float angle = signed_unit(seed, 1U) * 3.14159265F;
          const float reach = patch.jitter * std::sqrt(unit_float(seed, 2U));
          wanted.setX(wanted.x() + (std::cos(angle) * reach));
          wanted.setZ(wanted.z() + (std::sin(angle) * reach));
        }
        if (patch.yaw_spread > 0.0F) {
          instance_yaw =
              signed_unit(seed, 3U) * patch.yaw_spread * 0.5F * (3.14159265F / 180.0F);
        }
        if (patch.scale_spread > 0.0F) {
          instance_scale =
              patch.scale * (1.0F + (signed_unit(seed, 4U) * patch.scale_spread));
        }
      }
      float const radius = Game::Map::world_prop_ground_radius(type, instance_scale);

      std::optional<QVector3D> spot;
      if (patch.exact) {

        spot = App::Utils::snap_to_walkable_ground(wanted);
      }
      for (std::size_t ring = 0; ring < k_nudge_rings.size() && !spot.has_value();
           ++ring) {
        float const reach = k_nudge_rings.at(ring);
        int const directions = reach <= 0.0F ? 1 : k_nudge_directions;
        float const bias = static_cast<float>(ring) * 0.4F;
        for (int step = 0; step < directions; ++step) {
          float const angle = bias + (6.2831853F * static_cast<float>(step) /
                                      static_cast<float>(std::max(1, directions)));
          QVector3D const candidate(wanted.x() + (std::cos(angle) * reach),
                                    wanted.y(),
                                    wanted.z() + (std::sin(angle) * reach));
          if (stands_clear(candidate.x(), candidate.z(), radius)) {
            spot = candidate;
            break;
          }
        }
      }

      if (!spot.has_value()) {
        continue;
      }

      placed.push_back({spot->x(), spot->z(), radius});

      const QVector2D grid_position = grid_position_from_world(terrain_field, *spot);
      Game::Map::WorldProp prop;
      prop.type = type;
      prop.x = grid_position.x();
      prop.z = grid_position.y();
      prop.scale = instance_scale;
      prop.rotation = instance_yaw;
      if (type == Game::Map::WorldProp::Type::FireCamp) {
        prop.radius *= instance_scale;
        prop.intensity *= instance_scale;
      }
      prop.persistent = true;
      m_world_props.push_back(prop);
    }
  }
}

void ArenaViewport::clear_wildlife() {
  Game::Wildlife::BirdFlockManager::instance().reset();
  if (m_world == nullptr) {
    return;
  }
  if (auto* wildlife = m_world->get_system<Game::Wildlife::WildlifeSystem>()) {
    Game::Wildlife::WildlifeSettings disabled;
    disabled.enabled = false;
    wildlife->configure(disabled, 1U);
  }
  std::vector<Engine::Core::EntityID> doomed;
  for (auto* entity :
       m_world->collect_entities_with<Engine::Core::WildlifeComponent>()) {
    if (entity != nullptr) {
      doomed.push_back(entity->get_id());
    }
  }
  for (auto entity_id : doomed) {
    m_world->destroy_entity(entity_id);
  }
}

void ArenaViewport::configure_scenario_wildlife(
    const Arena::ArenaScenarioDefinition& definition,
    const QVector3D& scenario_origin) {
  if (m_world == nullptr) {
    return;
  }
  auto* wildlife = m_world->get_system<Game::Wildlife::WildlifeSystem>();
  if (wildlife == nullptr) {
    return;
  }

  Game::Wildlife::WildlifeSettings settings = definition.wildlife;
  for (auto* config : {&settings.sheep, &settings.wolves, &settings.birds}) {
    for (auto& area : config->spawn_areas) {
      area.x += scenario_origin.x();
      area.z += scenario_origin.z();
    }
  }
  wildlife->configure(settings, 1337U);
  wildlife->set_cosmetic_focus(scenario_origin.x(), scenario_origin.z());
}

void ArenaViewport::configure_scenario_undead_zones(
    const Arena::ArenaScenarioDefinition& definition,
    const QVector3D& scenario_origin) {
  if (m_world == nullptr) {
    return;
  }
  auto* undead_system = m_world->get_system<Game::Systems::UndeadAwakeningSystem>();
  if (undead_system == nullptr) {
    return;
  }

  m_arena_undead_zones.clear();
  m_arena_undead_zones.reserve(definition.undead_zones.size());
  for (auto zone : definition.undead_zones) {
    zone.x += scenario_origin.x();
    zone.z += scenario_origin.z();
    m_arena_undead_zones.push_back(std::move(zone));
  }

  Game::Map::MapDefinition map_definition;
  map_definition.coordSystem = Game::Map::CoordSystem::World;
  map_definition.grid.width = m_terrain_grid_extent;
  map_definition.grid.height = m_terrain_grid_extent;
  map_definition.grid.tile_size = k_terrain_tile_size;
  map_definition.undead_zones = m_arena_undead_zones;
  undead_system->configure(map_definition);
  retain_zone_shrine_props(*undead_system);

  if (m_ambient_fog != nullptr) {
    auto const& terrain_service = m_session.terrain();
    std::vector<Game::Map::FogZone> fog;
    fog.reserve(m_arena_undead_zones.size());
    for (const auto& zone : m_arena_undead_zones) {
      if (zone.fog_density <= 0.0F) {
        continue;
      }
      auto patch =
          Game::Map::undead_zone_fog(zone.x, zone.z, zone.radius, zone.fog_density);
      patch.y = terrain_service.resolve_surface_world_y(patch.x, patch.z, 0.0F);
      fog.push_back(patch);
    }
    m_ambient_fog->configure(fog);
  }

  if (!m_arena_undead_zones.empty()) {
    if (auto* ai_system = m_world->get_system<Game::Systems::AISystem>()) {
      ai_system->reinitialize();
    }
  }
}

void ArenaViewport::configure_scenario_rafts(
    const Arena::ArenaScenarioDefinition& definition) {
  if (m_world == nullptr) {
    return;
  }
  auto* rafts = m_world->get_system<Game::Systems::RaftSystem>();
  if (rafts == nullptr) {
    return;
  }
  Game::Map::MapDefinition map_definition;
  map_definition.coordSystem = Game::Map::CoordSystem::World;
  map_definition.grid.width = m_terrain_grid_extent;
  map_definition.grid.height = m_terrain_grid_extent;
  map_definition.grid.tile_size = k_terrain_tile_size;
  map_definition.rafts = definition.rafts;
  rafts->configure(map_definition);
}

void ArenaViewport::configure_scenario_rockfall_traps(
    const Arena::ArenaScenarioDefinition& definition,
    const QVector3D& scenario_origin) {
  if (m_world == nullptr) {
    return;
  }
  auto* rockfall = m_world->get_system<Game::Systems::RockfallSystem>();
  if (rockfall == nullptr) {
    return;
  }
  Game::Map::MapDefinition map_definition;
  map_definition.coordSystem = Game::Map::CoordSystem::World;
  map_definition.grid.width = m_terrain_grid_extent;
  map_definition.grid.height = m_terrain_grid_extent;
  map_definition.grid.tile_size = k_terrain_tile_size;
  map_definition.rockfall_traps = definition.rockfall_traps;
  for (auto& trap : map_definition.rockfall_traps) {
    trap.release_x += scenario_origin.x();
    trap.release_z += scenario_origin.z();
    trap.target_x += scenario_origin.x();
    trap.target_z += scenario_origin.z();
  }
  rockfall->configure(map_definition);
}

void ArenaViewport::retain_zone_shrine_props(
    const Game::Systems::UndeadAwakeningSystem& undead_system) {
  constexpr float k_shrine_match_grid_distance = 1.0F;

  auto& terrain_service = m_session.terrain();
  const auto& terrain_field = terrain_service.terrain_field();
  bool planted = false;
  for (const auto& zone : m_arena_undead_zones) {
    if (!undead_system.has_shrine(zone.id)) {
      continue;
    }

    const QVector2D grid_position = grid_position_from_world(
        terrain_field, undead_system.shrine_world_position(zone.id));

    const bool already_tracked = std::any_of(
        m_world_props.begin(),
        m_world_props.end(),
        [&grid_position](const Game::Map::WorldProp& prop) {
          return prop.type == Game::Map::WorldProp::Type::MagicShrine &&
                 QVector2D(prop.x - grid_position.x(), prop.z - grid_position.y())
                         .length() < k_shrine_match_grid_distance;
        });
    if (already_tracked) {
      continue;
    }

    Game::Map::WorldProp shrine;
    shrine.type = Game::Map::WorldProp::Type::MagicShrine;
    shrine.x = grid_position.x();
    shrine.z = grid_position.y();
    shrine.persistent = true;
    m_world_props.push_back(shrine);
    planted = true;
  }

  if (planted && m_gl_initialized && m_scatter != nullptr) {
    m_scatter->refresh_runtime_world_props(terrain_service.world_props());
  }
}

void ArenaViewport::clear_undead_zones() {
  if (m_world == nullptr || m_arena_undead_zones.empty()) {
    return;
  }

  QSet<int> zone_owner_ids;
  for (const auto& zone : m_arena_undead_zones) {
    zone_owner_ids.insert(zone.owner_id);
  }

  std::vector<Engine::Core::EntityID> doomed;
  for (auto* entity : m_world->collect_entities_with<Engine::Core::UnitComponent>()) {
    auto* unit = entity != nullptr
                     ? entity->get_component<Engine::Core::UnitComponent>()
                     : nullptr;
    if (unit != nullptr && zone_owner_ids.contains(unit->owner_id)) {
      doomed.push_back(entity->get_id());
    }
  }
  for (Engine::Core::EntityID const entity_id : doomed) {
    m_world->destroy_entity(entity_id);
  }

  m_arena_undead_zones.clear();
  if (auto* undead_system =
          m_world->get_system<Game::Systems::UndeadAwakeningSystem>()) {
    undead_system->configure(Game::Map::MapDefinition{});
  }
  if (m_ambient_fog != nullptr) {
    m_ambient_fog->configure({});
  }
}

void ArenaViewport::clear_world_props() {
  if (m_world_props.empty()) {
    return;
  }
  m_world_props.clear();
  reconfigure_terrain_from_state();
}

void ArenaViewport::clear_world_props_of_type() {
  size_t const before = m_world_props.size();
  std::erase_if(m_world_props, [this](const Game::Map::WorldProp& prop) {
    return prop.type == m_spawn_world_prop_type;
  });
  if (m_world_props.size() != before) {
    reconfigure_terrain_from_state();
  }
}

void ArenaViewport::reset_arena() {
  m_session.building_collision().clear();
  m_scenario_runner.reset();
  clear_rpg_scenario_state();
  m_frame_continuity_analyzer.reset();
  m_last_scenario_issue_revision = 0U;
  m_scenario_finished_emitted = false;
  clear_undead_zones();
  if (m_world != nullptr) {
    if (auto* rockfall = m_world->get_system<Game::Systems::RockfallSystem>()) {
      rockfall->configure(Game::Map::MapDefinition{});
    }
  }
  clear_wildlife();
  clear_units();
  if (m_world != nullptr) {
    Arena::destroy_remaining_gameplay_entities(*m_world);
  }
  const bool had_custom_terrain =
      !m_arena_rivers.empty() || !m_arena_lakes.empty() || !m_arena_bridges.empty() ||
      !m_arena_roads.empty() || !m_arena_elevation_patches.empty() ||
      m_terrain_grid_extent != k_terrain_width ||
      m_arena_floor_half_extent != k_default_floor_extent ||
      m_terrain_settings.height_scale != k_default_terrain_height_scale ||
      m_ground_type != m_ground_type_baseline ||
      m_terrain_settings.seed != m_terrain_seed_baseline ||
      m_suppress_boundary_mountains || m_suppress_procedural_props ||
      m_terrain_from_map;
  m_terrain_from_map = false;
  m_arena_rivers.clear();
  m_arena_lakes.clear();
  m_arena_bridges.clear();
  m_arena_roads.clear();
  m_arena_elevation_patches.clear();
  m_arena_floor_half_extent = k_default_floor_extent;
  m_terrain_grid_extent = k_terrain_width;
  m_terrain_settings.height_scale = k_default_terrain_height_scale;
  m_ground_type = m_ground_type_baseline;
  m_terrain_settings.seed = m_terrain_seed_baseline;
  m_suppress_boundary_mountains = false;
  m_suppress_procedural_props = false;
  clear_world_props();
  if (had_custom_terrain && m_world_props.empty()) {
    reconfigure_terrain_from_state();
  }
  m_attack_scrub_entity_id = 0;
  m_attack_scrub_enabled = false;
  m_attack_scrub_phase = 0.5F;
  set_force_full_creature_lod(true);
  Render::GraphicsSettings::instance().set_quality(Render::GraphicsQuality::High);
  m_weather_lighting = {};
  m_rain_enabled = false;
  if (m_rain != nullptr) {
    m_rain->set_enabled(false);
  }
  pause_simulation(false);
  reset_camera();
  if (!m_combat_debug_overlay_enabled) {
    Render::Profiling::CombatAnimationDiagnostics::instance().set_enabled(false);
  }
}

void ArenaViewport::sync_spawn_selection_defaults() {
  const auto* nation = m_session.nations().get_nation(m_spawn_nation_id);
  if (nation == nullptr || nation->available_troops.empty()) {
    return;
  }

  auto it = std::find_if(nation->available_troops.begin(),
                         nation->available_troops.end(),
                         [this](const Game::Systems::TroopType& troop) {
                           return troop.unit_type == m_spawn_unit_type;
                         });
  if (it == nation->available_troops.end()) {
    m_spawn_unit_type = nation->available_troops.front().unit_type;
  }
}
