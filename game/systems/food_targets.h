#pragma once

#include <QVector3D>

#include <optional>
#include <string_view>

#include "../core/entity.h"

namespace Engine::Core {
class World;
class BuilderProductionComponent;
class MovementComponent;
} // namespace Engine::Core

namespace Game::Systems {

inline constexpr float k_sheep_work_reach = 2.4F;

struct FoodTarget {
  Engine::Core::EntityID id{0};
  std::string_view product_type{};
  float x{0.0F};
  float z{0.0F};
};

[[nodiscard]] auto farm_is_harvestable(const Engine::Core::Entity& farm,
                                       int owner_id) -> bool;
[[nodiscard]] auto sheep_is_slaughterable(const Engine::Core::Entity& sheep) -> bool;

[[nodiscard]] auto
food_target_claimed(Engine::Core::World& world,
                    Engine::Core::EntityID target_id,
                    Engine::Core::EntityID except_worker = 0) -> bool;

[[nodiscard]] auto resolve_food_target(Engine::Core::World& world,
                                       Engine::Core::EntityID target_id,
                                       int owner_id) -> std::optional<FoodTarget>;

[[nodiscard]] auto find_food_target_near(Engine::Core::World& world,
                                         std::string_view product_type,
                                         int owner_id,
                                         float x,
                                         float z,
                                         float radius,
                                         Engine::Core::EntityID except_worker = 0)
    -> std::optional<FoodTarget>;

[[nodiscard]] auto owner_has_farm_near(
    Engine::Core::World& world, int owner_id, float x, float z, float radius) -> bool;

[[nodiscard]] auto food_work_position(Engine::Core::World& world,
                                      Engine::Core::EntityID worker_id,
                                      const QVector3D& worker_position,
                                      const FoodTarget& target) -> QVector3D;

void assign_food_task(Engine::Core::BuilderProductionComponent& builder,
                      Engine::Core::MovementComponent* movement,
                      const FoodTarget& target,
                      const QVector3D& work_position);

} // namespace Game::Systems
