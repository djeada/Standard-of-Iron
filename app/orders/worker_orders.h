#pragma once

#include <QVector3D>

#include <cstdint>
#include <string>
#include <vector>

#include "app/orders/order_submission.h"

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace App::Orders {

[[nodiscard]] auto harvest_order(std::vector<Engine::Core::EntityID> crew,
                                 std::string product_type,
                                 Engine::Core::EntityID resource_target,
                                 float x,
                                 float z) -> App::Core::OrderRequest;

[[nodiscard]] auto builder_crew_of(Engine::Core::World& world,
                                   const std::vector<Engine::Core::EntityID>& selected)
    -> std::vector<Engine::Core::EntityID>;

void sort_nearest_first(Engine::Core::World& world,
                        std::vector<Engine::Core::EntityID>& crew,
                        float x,
                        float z);

} // namespace App::Orders
