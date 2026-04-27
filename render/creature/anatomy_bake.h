#pragma once

#include "../../game/core/entity.h"
#include "animation_state_components.h"

#include <QVector3D>
#include <cstdint>

namespace Render::Creature {

auto get_or_bake_horse_anatomy(Engine::Core::Entity *entity, std::uint32_t seed,
                               const QVector3D &leather_base,
                               const QVector3D &cloth_base,
                               float mount_scale) -> HorseAnatomyComponent &;

auto get_or_bake_elephant_anatomy(
    Engine::Core::Entity *entity, std::uint32_t seed,
    const QVector3D &fabric_base,
    const QVector3D &metal_base) -> ElephantAnatomyComponent &;

} // namespace Render::Creature
