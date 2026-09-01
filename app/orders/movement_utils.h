#pragma once

#include <QPointF>
#include <QVector3D>

#include <vector>

#include "app/orders/order_feedback.h"
#include "game/core/entity.h"

namespace Engine::Core {
class World;
}
namespace Game::Systems {
class PickingService;
}
namespace Render::GL {
class Camera;
}

namespace App::Utils {

auto snap_to_walkable_ground(const QVector3D& world_position) -> QVector3D;

auto issue_civilian_delivery_command(
    Engine::Core::World* world,
    const std::vector<Engine::Core::EntityID>& selected,
    Game::Systems::PickingService* picking_service,
    Render::GL::Camera* camera,
    qreal sx,
    qreal sy,
    int viewport_width,
    int viewport_height,
    int local_owner_id) -> App::Core::OrderOutcome;

auto issue_builder_repair_command(Engine::Core::World* world,
                                  const std::vector<Engine::Core::EntityID>& selected,
                                  Game::Systems::PickingService* picking_service,
                                  Render::GL::Camera* camera,
                                  qreal sx,
                                  qreal sy,
                                  int viewport_width,
                                  int viewport_height,
                                  int local_owner_id) -> App::Core::OrderOutcome;

auto issue_builder_dismantle_command(
    Engine::Core::World* world,
    const std::vector<Engine::Core::EntityID>& selected,
    Game::Systems::PickingService* picking_service,
    Render::GL::Camera* camera,
    qreal sx,
    qreal sy,
    int viewport_width,
    int viewport_height,
    int local_owner_id) -> App::Core::OrderOutcome;

auto submit_ground_move(Engine::Core::World& world,
                        const std::vector<Engine::Core::EntityID>& units,
                        const QVector3D& destination,
                        int owner_id) -> App::Core::OrderOutcome;

[[nodiscard]] auto
pick_enemy_unit_at_screen(Engine::Core::World* world,
                          Render::GL::Camera* camera,
                          qreal sx,
                          qreal sy,
                          int viewport_width,
                          int viewport_height,
                          int local_owner_id) -> Engine::Core::EntityID;

auto issue_attack_command(Engine::Core::World* world,
                          const std::vector<Engine::Core::EntityID>& selected,
                          Engine::Core::EntityID target_id,
                          int local_owner_id) -> App::Core::OrderOutcome;

auto issue_move_or_attack_command(Engine::Core::World* world,
                                  const std::vector<Engine::Core::EntityID>& selected,
                                  Game::Systems::PickingService* picking_service,
                                  Render::GL::Camera* camera,
                                  qreal sx,
                                  qreal sy,
                                  int viewport_width,
                                  int viewport_height,
                                  int local_owner_id) -> App::Core::OrderOutcome;

} // namespace App::Utils
