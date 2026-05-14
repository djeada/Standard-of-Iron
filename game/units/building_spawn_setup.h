#pragma once

#include <string_view>

#include "../systems/nation_id.h"

namespace Engine::Core {
class Entity;
class RenderableComponent;
} // namespace Engine::Core

namespace Game::Units {

auto add_building_renderable(Engine::Core::Entity& entity,
                             int owner_id,
                             Game::Systems::NationID nation_id,
                             std::string_view building_type)
    -> Engine::Core::RenderableComponent*;

}
