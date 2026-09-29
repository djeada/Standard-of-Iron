#include "production_system_site_ghosts.h"

#include <QVector3D>

#include <algorithm>
#include <string>
#include <vector>

#include "construction_cost_catalog.h"
#include "core/ambient_session.h"
#include "core/component_gameplay.h"
#include "core/component_presentation.h"
#include "core/world.h"
#include "map/terrain_service.h"
#include "production_system_shared_site.h"
#include "systems/nation_registry.h"
#include "units/building_spawn_setup.h"

namespace Game::Systems::ProductionTasks {

namespace {

constexpr float k_site_ghost_match_radius_sq = 0.25F;

struct SiteGhost {
  int owner_id{0};
  Game::Systems::NationID nation_id{Game::Systems::NationID::RomanRepublic};
  std::string product_type;
  float x{0.0F};
  float z{0.0F};
  float rotation_y{0.0F};
  float progress{0.0F};
  bool matched{false};
};

auto same_site(const SiteGhost& site,
               int owner_id,
               const std::string& product_type,
               float x,
               float z) -> bool {
  if (site.owner_id != owner_id || site.product_type != product_type) {
    return false;
  }
  const float dx = site.x - x;
  const float dz = site.z - z;
  return (dx * dx + dz * dz) <= k_site_ghost_match_radius_sq;
}

auto collect_site_ghosts(Engine::Core::World& world) -> std::vector<SiteGhost> {
  std::vector<SiteGhost> sites;
  for (auto [entity_ref, builder] :
       world.entity_view<Engine::Core::BuilderProductionComponent>()) {
    if (!raises_shared_site(builder)) {
      continue;
    }
    if (construction_build_time(builder.product_type) <= 0.0F) {
      continue;
    }
    const auto* unit = world.try_get<Engine::Core::UnitComponent>(entity_ref.get_id());
    const int owner_id = unit != nullptr ? unit->owner_id : 0;
    const float progress = site_progress(builder);
    auto existing =
        std::find_if(sites.begin(), sites.end(), [&](const SiteGhost& candidate) {
          return same_site(candidate,
                           owner_id,
                           builder.product_type,
                           builder.construction_site_x,
                           builder.construction_site_z);
        });
    if (existing != sites.end()) {
      existing->progress = std::max(existing->progress, progress);
      continue;
    }
    sites.push_back(SiteGhost{.owner_id = owner_id,
                              .nation_id = unit != nullptr
                                               ? unit->nation_id
                                               : Game::Systems::NationID::RomanRepublic,
                              .product_type = builder.product_type,
                              .x = builder.construction_site_x,
                              .z = builder.construction_site_z,
                              .rotation_y = builder.construction_site_rotation_y,
                              .progress = progress});
  }
  return sites;
}

void raise_site_ghost(Engine::Core::World& world, const SiteGhost& site) {
  auto* entity = world.create_entity();
  if (entity == nullptr) {
    return;
  }
  const auto entity_id = entity->get_id();
  auto* transform = world.emplace<Engine::Core::TransformComponent>(entity_id);
  auto* preview = world.emplace<Engine::Core::ConstructionPreviewComponent>(entity_id);
  if (transform == nullptr || preview == nullptr) {
    world.destroy_entity(entity_id);
    return;
  }

  float surface_y = 0.0F;
  const auto* services = Game::Session::services_for_or_null(world);
  if (services != nullptr && services->terrain != nullptr &&
      services->terrain->is_initialized()) {
    surface_y = services->terrain->resolve_surface_world_y(site.x, site.z);
  }
  transform->position = {site.x, surface_y, site.z};
  transform->rotation = {0.0F, site.rotation_y, 0.0F};
  const QVector3D scale = Game::Units::building_transform_scale(site.product_type);
  transform->scale = {scale.x(), scale.y(), scale.z()};

  auto* renderable =
      Game::Units::add_building_renderable(*entity, site.nation_id, site.product_type);
  if (renderable == nullptr) {
    world.destroy_entity(entity_id);
    return;
  }
  renderable->visible = false;

  preview->owner_id = site.owner_id;
  preview->nation_id = site.nation_id;
  preview->product_type = site.product_type;
  preview->valid = true;
  preview->site_ghost = true;
  preview->progress = site.progress;
}

} // namespace

void sync_site_ghosts(Engine::Core::World& world) {
  auto sites = collect_site_ghosts(world);

  std::vector<Engine::Core::EntityID> stale;
  for (auto [entity_id, preview, transform] :
       world.view<Engine::Core::ConstructionPreviewComponent,
                  Engine::Core::TransformComponent>()) {
    if (!preview.site_ghost) {
      continue;
    }
    auto site =
        std::find_if(sites.begin(), sites.end(), [&](const SiteGhost& candidate) {
          return !candidate.matched && same_site(candidate,
                                                 preview.owner_id,
                                                 preview.product_type,
                                                 transform.position.x,
                                                 transform.position.z);
        });
    if (site == sites.end()) {
      stale.push_back(entity_id);
      continue;
    }
    site->matched = true;
    preview.valid = true;
    preview.progress = site->progress;
  }

  for (const auto id : stale) {
    world.destroy_entity(id);
  }
  for (const auto& site : sites) {
    if (!site.matched) {
      raise_site_ghost(world, site);
    }
  }
}

} // namespace Game::Systems::ProductionTasks
