#pragma once

#include <cstdint>

#include "render/entity/registry.h"

namespace Engine::Core {
class Entity;
}

namespace Render::GL::Carthage {

auto elephant_anatomy_seed(const Engine::Core::Entity& entity) -> std::uint32_t;

void register_elephant_renderer(EntityRendererRegistry& registry);

} // namespace Render::GL::Carthage
