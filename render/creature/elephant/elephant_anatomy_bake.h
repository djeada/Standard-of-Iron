#pragma once

#include <QVector3D>

#include <cstdint>

#include "game/core/entity.h"
#include "render/creature/elephant/elephant_components.h"

namespace Render::Creature {

auto get_or_bake_elephant_anatomy(Engine::Core::Entity* entity,
                                  std::uint32_t seed,
                                  const QVector3D& fabric_base,
                                  const QVector3D& metal_base)
    -> ElephantAnatomyComponent&;

} // namespace Render::Creature
