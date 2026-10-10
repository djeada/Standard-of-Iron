#include <cstdio>
#include <cstdlib>
#include "production_system_construction.h"

#include <QVector3D>

#include <optional>
#include <vector>

#include "build_site.h"
#include "construction_cost_catalog.h"
#include "core/ambient_session.h"
#include "core/component_core.h"
#include "core/component_gameplay.h"
#include "core/world.h"
#include "map/map_transformer.h"
#include "map/terrain_service.h"
#include "production_system_builder_task.h"
#include "production_system_wall.h"
#include "systems/navigation/wall_network_service.h"
#include "systems/player_feedback.h"
#include "systems/player_resource_registry.h"
#include "systems/spawn_flare.h"
#include "units/factory.h"
#include "units/spawn_type.h"

namespace Game::Systems::ProductionTasks {

namespace {

constexpr float k_finished_site_nudge = 5.0F;

struct SitePose {
  QVector3D position;
  float rotation_y{0.0F};
  Engine::Core::Entity* wall_site_entity{nullptr};
};

auto spawn_type_for_product(const std::string& product_type)
    -> std::optional<Game::Units::SpawnType> {
  using Game::Units::SpawnType;
  if (product_type == "catapult") {
    return SpawnType::Catapult;
  }
  if (product_type == "ballista") {
    return SpawnType::Ballista;
  }
  if (product_type == "ram") {
    return SpawnType::Ram;
  }
  if (product_type == "siege_tower") {
    return SpawnType::SiegeTower;
  }
  if (product_type == "barracks") {
    return SpawnType::Barracks;
  }
  if (product_type == "defense_tower") {
    return SpawnType::DefenseTower;
  }
  if (product_type == "wall_gate") {
    return SpawnType::WallGate;
  }
  if (product_type == "wall_ladder") {
    return SpawnType::WallLadder;
  }
  if (is_wall_network_product(product_type)) {
    return SpawnType::WallSegment;
  }
  if (product_type == "home") {
    return SpawnType::Home;
  }
  if (product_type == "marketplace") {
    return SpawnType::Marketplace;
  }
  if (product_type == "farm") {
    return SpawnType::Farm;
  }
  if (product_type == "temple") {
    return SpawnType::Temple;
  }
  return std::nullopt;
}

auto initial_site_position(Engine::Core::World& world,
                           const Engine::Core::BuilderProductionComponent& builder,
                           const Engine::Core::TransformComponent& t) -> QVector3D {
  if (!builder.has_construction_site) {
    return {t.position.x, t.position.y, t.position.z};
  }
  float site_y = t.position.y;
  auto* terrain = Game::Session::services_for(world).terrain;
  if (terrain != nullptr && terrain->is_initialized()) {
    site_y = terrain->resolve_surface_world_y(
        builder.construction_site_x, builder.construction_site_z, 0.0F, site_y);
  }
  return {builder.construction_site_x, site_y, builder.construction_site_z};
}

void abandon_unfinished_structure(Engine::Core::World& world,
                                  Engine::Core::BuilderProductionComponent& builder) {
  builder.in_progress = false;
  builder.time_remaining = 0.0F;
  builder.has_construction_site = false;
  builder.at_construction_site = false;
  clear_builder_task_target(world, &builder, false);
}

auto resolve_wall_site_pose(Engine::Core::World& world,
                            Engine::Core::Entity& builder_entity,
                            Engine::Core::BuilderProductionComponent& builder,
                            SitePose& pose) -> bool {
  if (!is_wall_network_product(builder.product_type) ||
      builder.construction_site_entity_id == 0) {
    return true;
  }
  if (skip_invalid_wall_site(&world, &builder_entity, &builder)) {
    return false;
  }
  pose.wall_site_entity = world.get_entity(builder.construction_site_entity_id);
  if (auto* site_transform =
          pose.wall_site_entity != nullptr
              ? pose.wall_site_entity->get_component<Engine::Core::TransformComponent>()
              : nullptr) {
    pose.position = QVector3D(site_transform->position.x,
                              site_transform->position.y,
                              site_transform->position.z);
    pose.rotation_y = site_transform->rotation.y;
  }
  return true;
}

auto crew_finishing_with(const FinishedSites& finishing_sites,
                         Engine::Core::EntityID builder_id)
    -> std::vector<Engine::Core::EntityID> {
  std::vector<Engine::Core::EntityID> finishing_crew{builder_id};
  for (const auto& [lead_id, crew] : finishing_sites) {
    if (lead_id == builder_id) {
      finishing_crew = crew;
    }
  }
  return finishing_crew;
}

auto settle_on_clear_site(Engine::Core::World& world,
                          Engine::Core::Entity& builder_entity,
                          Engine::Core::BuilderProductionComponent& builder,
                          const Engine::Core::UnitComponent& unit,
                          const FinishedSites& finishing_sites,
                          bool free_standing_wall,
                          SitePose& pose) -> bool {
  const auto clear_site =
      find_clear_site(world,
                      builder.product_type,
                      pose.position,
                      free_standing_wall ? 0.0F : k_finished_site_nudge,
                      pose.rotation_y,
                      crew_finishing_with(finishing_sites, builder_entity.get_id()));
  if (!clear_site.has_value()) {
    grant_resources_at(unit.owner_id,
                       pose.position.x(),
                       pose.position.y(),
                       pose.position.z(),
                       construction_cost_info(builder.product_type).resource_costs);
if (std::getenv("SOI_TMP_BUILD") != nullptr) { std::fprintf(stderr, "FAULT SETTLE %s at %.1f,%.1f\n", builder.product_type.c_str(), pose.position.x(), pose.position.z()); }
      abandon_unfinished_structure(world, builder);
    builder.report_fault(Engine::Core::BuilderTaskFault::Unreachable);
    return false;
  }
  pose.position = QVector3D(clear_site->x(), pose.position.y(), clear_site->z());
  return true;
}

} // namespace

auto raise_structure(Engine::Core::World& world,
                     Engine::Core::Entity& builder_entity,
                     Engine::Core::BuilderProductionComponent& builder,
                     const Engine::Core::TransformComponent& transform,
                     const Engine::Core::UnitComponent& unit,
                     Engine::Core::MovementComponent* movement,
                     const FinishedSites& finishing_sites) -> StructureOutcome {
  auto reg = Game::Map::MapTransformer::get_factory_registry();
  if (!reg) {
    return StructureOutcome::NotRaised;
  }

  Game::Units::SpawnParams sp;
  SitePose pose{.position = initial_site_position(world, builder, transform),
                .rotation_y = builder.construction_site_rotation_y};
  if (!resolve_wall_site_pose(world, builder_entity, builder, pose)) {
    return StructureOutcome::Skipped;
  }
  sp.player_id = unit.owner_id;
  sp.ai_controlled =
      world.has<Engine::Core::AIControlledComponent>(builder_entity.get_id());
  sp.nation_id = unit.nation_id;
  sp.is_initial_spawn = false;
  sp.rotation_y = pose.rotation_y;

  const auto spawn_type = spawn_type_for_product(builder.product_type);
  if (!spawn_type.has_value()) {
    abandon_unfinished_structure(world, builder);
    return StructureOutcome::Skipped;
  }
  sp.spawn_type = *spawn_type;

  const bool free_standing_wall =
      is_wall_network_product(builder.product_type) && pose.wall_site_entity == nullptr;
  if ((!is_wall_network_product(builder.product_type) || free_standing_wall) &&
      !settle_on_clear_site(world,
                            builder_entity,
                            builder,
                            unit,
                            finishing_sites,
                            free_standing_wall,
                            pose)) {
    return StructureOutcome::Skipped;
  }
  sp.position = pose.position;

  bool raised = false;
  if (auto completed = reg->create(sp.spawn_type, world, sp)) {
    attach_spawn_flare(
        world, completed->id(), sp.spawn_type, Engine::Core::SpawnFlareStyle::Recruit);
    raised = true;
  }

  if (is_wall_network_product(builder.product_type) &&
      builder.construction_site_entity_id != 0) {
    world.destroy_entity(builder.construction_site_entity_id);
    builder.construction_site_entity_id = 0;
    WallNetworkService::refresh_world(world);
  }
  if (builder.has_construction_site && movement != nullptr) {
    movement->stop();
  }
  return raised ? StructureOutcome::Raised : StructureOutcome::NotRaised;
}

} // namespace Game::Systems::ProductionTasks
