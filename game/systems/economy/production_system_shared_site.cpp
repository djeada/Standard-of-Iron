#include "production_system_shared_site.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "build_site.h"
#include "construction_cost_catalog.h"
#include "core/component_gameplay.h"
#include "core/world.h"
#include "production_system_approach.h"
#include "production_system_builder_task.h"
#include "systems/builder_product_types.h"
#include "units/squad.h"

namespace Game::Systems::ProductionTasks {

namespace {

struct SharedSiteKey {
  int owner_id = 0;
  std::string product_type;
  float x = 0.0F;
  float z = 0.0F;
  float rotation_y = 0.0F;

  [[nodiscard]] auto operator==(const SharedSiteKey& other) const -> bool = default;
};

struct SharedSite {
  SharedSiteKey key;
  std::vector<Engine::Core::EntityID> crews;
};

auto crew_hands(const Engine::Core::World& world, Engine::Core::EntityID id) -> float {
  const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
  return unit != nullptr ? std::max(0.05F, Game::Units::squad_fraction(*unit)) : 1.0F;
}

auto collect_shared_sites(Engine::Core::World& world) -> std::vector<SharedSite> {
  std::vector<SharedSite> sites;
  for (auto [entity_ref, builder] :
       world.entity_view<Engine::Core::BuilderProductionComponent>()) {
    if (!raises_shared_site(builder)) {
      continue;
    }
    const Engine::Core::EntityID id = entity_ref.get_id();
    const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
    SharedSiteKey key{.owner_id = unit != nullptr ? unit->owner_id : 0,
                      .product_type = builder.product_type,
                      .x = builder.construction_site_x,
                      .z = builder.construction_site_z,
                      .rotation_y = builder.construction_site_rotation_y};
    auto site = std::find_if(sites.begin(), sites.end(), [&key](const SharedSite& s) {
      return s.key == key;
    });
    if (site == sites.end()) {
      sites.push_back(SharedSite{.key = std::move(key), .crews = {}});
      site = std::prev(sites.end());
    }
    site->crews.push_back(id);
  }
  for (auto& site : sites) {
    std::sort(site.crews.begin(), site.crews.end());
  }
  return sites;
}

void release_helper_crew(Engine::Core::World& world, Engine::Core::EntityID crew) {
  auto* builder = world.try_get<Engine::Core::BuilderProductionComponent>(crew);
  auto* movement = world.try_get<Engine::Core::MovementComponent>(crew);
  if (builder == nullptr) {
    return;
  }
  if (movement != nullptr && builder->at_construction_site) {
    movement->stop();
  } else if (movement != nullptr) {
    abandon_site_route(*builder, movement);
  }
  builder->in_progress = false;
  builder->time_remaining = 0.0F;
  builder->construction_complete = true;
  builder->has_construction_site = false;
  builder->at_construction_site = false;
  reset_site_approach(*builder);
  clear_builder_task_target(world, builder, false);
}

struct SiteWork {
  float progress{0.0F};
  float hands{0.0F};
  Engine::Core::EntityID lead{0};
};

auto measure_site_work(const Engine::Core::World& world,
                       const SharedSite& site) -> SiteWork {
  SiteWork work;
  for (const auto crew : site.crews) {
    const auto& builder =
        *world.try_get<Engine::Core::BuilderProductionComponent>(crew);
    work.progress = std::max(work.progress, site_progress(builder));
    if (builder.at_construction_site && builder.in_progress) {
      work.hands += crew_hands(world, crew);
      if (work.lead == 0) {
        work.lead = crew;
      }
    }
  }
  return work;
}

void publish_crew_pace(Engine::Core::World& world,
                       const SharedSite& site,
                       float progress,
                       float seconds_at_this_pace) {
  for (const auto crew : site.crews) {
    auto& builder = *world.try_get<Engine::Core::BuilderProductionComponent>(crew);
    builder.build_time = seconds_at_this_pace;
    builder.time_remaining = (1.0F - progress) * seconds_at_this_pace;
  }
}

} // namespace

auto raises_shared_site(const Engine::Core::BuilderProductionComponent& builder)
    -> bool {
  return builder.has_construction_site && builder.construction_site_entity_id == 0 &&
         !builder.product_type.empty() &&
         !is_wall_builder_product(builder.product_type) &&
         !is_gather_builder_product(builder.product_type) &&
         builder.product_type != k_builder_product_repair &&
         builder.product_type != k_builder_product_dismantle;
}

auto site_progress(const Engine::Core::BuilderProductionComponent& builder) -> float {
  if (!builder.in_progress || builder.build_time <= 0.0F) {
    return 0.0F;
  }
  return std::clamp(1.0F - (builder.time_remaining / builder.build_time), 0.0F, 1.0F);
}

auto crew_gather_pace(const Engine::Core::World& world,
                      Engine::Core::EntityID id) -> float {
  constexpr float k_gather_crew_exponent = 1.5F;
  constexpr float k_min_gather_pace = 0.02F;
  const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
  if (unit == nullptr) {
    return 1.0F;
  }
  float const fraction = std::clamp(Game::Units::squad_fraction(*unit), 0.0F, 1.0F);
  return std::max(k_min_gather_pace, std::pow(fraction, k_gather_crew_exponent));
}

auto advance_shared_sites(Engine::Core::World& world,
                          float delta_time) -> FinishedSites {
  FinishedSites finishing;
  for (auto& site : collect_shared_sites(world)) {
    const float work = construction_build_time(site.key.product_type);
    if (work <= 0.0F) {
      continue;
    }

    SiteWork measured = measure_site_work(world, site);
    if (measured.lead == 0) {
      continue;
    }

    const float progress = std::min(
        1.0F, measured.progress + (std::max(0.0F, delta_time) * measured.hands / work));
    publish_crew_pace(world, site, progress, work / measured.hands);
    if (progress < 1.0F) {
      continue;
    }

    for (const auto crew : site.crews) {
      if (crew != measured.lead) {
        release_helper_crew(world, crew);
      }
    }
    world.try_get<Engine::Core::BuilderProductionComponent>(measured.lead)
        ->time_remaining = 0.0F;
    finishing.emplace_back(measured.lead, site.crews);
  }
  return finishing;
}

} // namespace Game::Systems::ProductionTasks
