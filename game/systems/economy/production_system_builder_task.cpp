#include "production_system_builder_task.h"

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <string_view>

#include "construction_cost_catalog.h"
#include "core/ambient_session.h"
#include "core/component_gameplay.h"
#include "core/world.h"
#include "map/terrain_service.h"
#include "systems/builder_idle.h"
#include "systems/builder_product_types.h"
#include "systems/harvest_yields.h"
#include "systems/navigation/nav_grid.h"
#include "systems/navigation/pathfinding.h"
#include "systems/player_resource_registry.h"
#include "units/squad.h"

namespace Game::Systems::ProductionTasks {

namespace {
constexpr auto k_collect_stone_product_type = k_builder_product_collect_stone;
constexpr auto k_collect_iron_ore_product_type = k_builder_product_collect_iron_ore;
} // namespace

auto is_wall_network_product(const std::string& product_type) -> bool {
  return is_wall_builder_product(product_type);
}

void clear_builder_task_target(Engine::Core::World& world,
                               Engine::Core::BuilderProductionComponent* builder,
                               bool release_tree) {
  if (builder == nullptr) {
    return;
  }
  if (release_tree && builder->task_target_reserved) {
    Game::Session::services_for(world).terrain->release_world_prop(
        builder->task_target_id);
  }
  builder->has_task_target = false;
  builder->task_target_id = 0;
  builder->task_target_x = 0.0F;
  builder->task_target_z = 0.0F;
  builder->task_target_reserved = false;
}

void load_onto_hauler(Engine::Core::Entity* worker,
                      ResourceType resource_type,
                      int amount,
                      Engine::Core::CarriedFoodForm food_form) {
  if (worker == nullptr || amount <= 0) {
    return;
  }
  auto* carry =
      Engine::Core::get_or_add_component<Engine::Core::ResourceCarryComponent>(worker);
  if (carry == nullptr) {
    return;
  }

  if (resource_type == ResourceType::Food &&
      carry->amounts.get(ResourceType::Food) <= 0) {
    carry->food_form = food_form;
  }
  carry->amounts.add(resource_type, amount);
}

void begin_structure_site(const Engine::Core::Entity& worker,
                          Engine::Core::BuilderProductionComponent& builder,
                          const std::string& construction_type,
                          float x,
                          float z,
                          float rotation_y) {
  builder.clear_auto_gather();
  builder.product_type = construction_type;

  const auto* unit = worker.get_component<Engine::Core::UnitComponent>();
  const float hands =
      unit != nullptr ? std::max(0.05F, Game::Units::squad_fraction(*unit)) : 1.0F;
  builder.build_time = construction_build_time(construction_type) / hands;
  builder.time_remaining = builder.build_time;
  builder.has_construction_site = true;
  builder.construction_site_x = x;
  builder.construction_site_z = z;
  builder.construction_site_rotation_y = rotation_y;
  builder.at_construction_site = false;
  builder.in_progress = false;
  builder.construction_complete = false;
  builder.bypass_movement_active = false;
}

auto is_raising_structure(const Engine::Core::BuilderProductionComponent& builder)
    -> bool {
  return builder.has_construction_site && !builder.product_type.empty() &&
         !is_gather_builder_product(builder.product_type) &&
         !is_wall_builder_product(builder.product_type) &&
         builder.product_type != k_builder_product_repair &&
         builder.product_type != k_builder_product_dismantle;
}

namespace {

constexpr float k_idle_before_gathering_seconds = 10.0F;

auto shortest_gatherable(const Engine::Core::World& world,
                         int owner_id) -> std::string {
  const auto* economy = Game::Session::services_for(world).economy;
  struct Option {
    ResourceType type;
    std::string_view product;
  };
  constexpr std::array<Option, 3> options{{
      {ResourceType::Wood, k_builder_product_cut_tree},
      {ResourceType::Stone, k_builder_product_collect_stone},
      {ResourceType::Iron, k_builder_product_collect_iron_ore},
  }};
  const Option* shortest = &options[0];
  int fewest = std::numeric_limits<int>::max();
  for (const auto& option : options) {
    const int held = economy != nullptr ? economy->get(owner_id, option.type) : 0;
    if (held < fewest) {
      fewest = held;
      shortest = &option;
    }
  }
  return std::string(shortest->product);
}

void start_idle_auto_gather(Engine::Core::World& world,
                            Engine::Core::Entity& worker,
                            Engine::Core::BuilderProductionComponent& builder,
                            float delta_time) {
  const auto* economy = Game::Session::services_for(world).economy;
  const auto* unit = world.try_get<Engine::Core::UnitComponent>(worker.get_id());
  const auto* movement = world.try_get<Engine::Core::MovementComponent>(worker.get_id());
  const auto* carry = world.try_get<Engine::Core::ResourceCarryComponent>(worker.get_id());
  const bool eligible =
      economy != nullptr && economy->idle_builders_gather() && unit != nullptr &&
      unit->health > 0 && movement != nullptr && !movement->get_has_target() &&
      !world.has<Engine::Core::AIControlledComponent>(worker.get_id()) &&
      !world.has<Engine::Core::AttackTargetComponent>(worker.get_id()) &&
      (carry == nullptr || carry->empty()) && builder.queued_structure_sites.empty() &&
      !builder.has_construction_site && !builder.in_progress &&
      !builder.has_task_target && !builder.has_work_to_resume() &&
      builder_is_idle(&builder);
  if (!eligible) {
    builder.idle_seconds = 0.0F;
    return;
  }
  builder.idle_seconds += std::max(0.0F, delta_time);
  if (builder.idle_seconds < k_idle_before_gathering_seconds) {
    return;
  }
  builder.idle_seconds = 0.0F;
  builder.auto_gather = true;
  builder.auto_gather_priority = shortest_gatherable(world, unit->owner_id);
}

} // namespace

void advance_queued_work_if_idle(Engine::Core::World& world,
                                 Engine::Core::Entity& worker,
                                 Engine::Core::BuilderProductionComponent& builder,
                                 float delta_time) {
  const bool between_jobs = !builder.has_construction_site && !builder.in_progress &&
                            !builder.has_task_target &&
                            builder.structure_task_entity_id == 0;
  if (!between_jobs) {
    return;
  }

  if (!builder.queued_structure_sites.empty()) {
    const auto next = builder.queued_structure_sites.front();
    builder.queued_structure_sites.erase(builder.queued_structure_sites.begin());
    begin_structure_site(
        worker, builder, next.construction_type, next.x, next.z, next.rotation_y);
    if (auto* movement = world.try_get<Engine::Core::MovementComponent>(worker.get_id())) {
      movement->set_rest_position(next.x, next.z);
    }
    return;
  }

  if (builder.has_work_to_resume()) {
    builder.resume_remembered_work();
    return;
  }
  start_idle_auto_gather(world, worker, builder, delta_time);
}

void complete_harvest_task(Engine::Core::World& world,
                           Engine::Core::Entity& worker,
                           Engine::Core::BuilderProductionComponent& builder) {
  bool const harvested = builder.has_task_target &&
                         Game::Session::services_for(world).terrain->harvest_world_prop(
                             builder.task_target_id);
  if (!harvested) {
    clear_builder_task_target(world, &builder);
    builder.report_fault(Engine::Core::BuilderTaskFault::TargetLost);
    return;
  }

  ResourceType resource_type = ResourceType::Wood;
  int reward_amount = k_cut_tree_wood_reward;
  if (builder.product_type == k_collect_stone_product_type) {
    resource_type = ResourceType::Stone;
    reward_amount = k_collect_stone_reward;
  } else if (builder.product_type == k_collect_iron_ore_product_type) {
    resource_type = ResourceType::Iron;
    reward_amount = k_collect_iron_ore_reward;
  }
  load_onto_hauler(&worker, resource_type, reward_amount);

  if (!world.has<Engine::Core::AIControlledComponent>(worker.get_id())) {
    builder.has_gather_order = true;
    builder.gather_product_type = builder.product_type;
    builder.gather_anchor_x = builder.task_target_x;
    builder.gather_anchor_z = builder.task_target_z;
  }
  if (auto* pathfinder = NavGrid::get_pathfinder()) {
    Point const tree_grid =
        NavGrid::world_to_grid(builder.task_target_x, builder.task_target_z);
    pathfinder->mark_region_dirty(
        tree_grid.x - 1, tree_grid.x + 1, tree_grid.y - 1, tree_grid.y + 1);
  }
}

} // namespace Game::Systems::ProductionTasks
