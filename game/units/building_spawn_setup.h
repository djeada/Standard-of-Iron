#pragma once

#include <QVector3D>

#include <string_view>

#include "../systems/nation_id.h"

namespace Engine::Core {
class Entity;
class RenderableComponent;
} // namespace Engine::Core

namespace Game::Units {

auto add_building_renderable(Engine::Core::Entity& entity,
                             Game::Systems::NationID nation_id,
                             std::string_view building_type)
    -> Engine::Core::RenderableComponent*;

auto building_transform_scale(std::string_view building_type) -> QVector3D;

} // namespace Game::Units
