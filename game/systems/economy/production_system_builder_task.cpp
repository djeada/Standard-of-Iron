#include "production_system_builder_task.h"

#include "core/ambient_session.h"
#include "core/component_gameplay.h"
#include "core/world.h"
#include "map/terrain_service.h"
#include "systems/builder_product_types.h"
#include "systems/harvest_yields.h"
#include "systems/navigation/nav_grid.h"
#include "systems/navigation/pathfinding.h"

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
