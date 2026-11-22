#pragma once

#include <QVector3D>

#include <cstdint>

#include "game/core/entity.h"
#include "render/creature/horse/horse_components.h"

namespace Render::Creature {

// The entity's horse anatomy, baked on first use and again whenever the seed or
// tack colours change. A null entity bakes into a thread-local scratch.
auto get_or_bake_horse_anatomy(Engine::Core::Entity* entity,
                               std::uint32_t seed,
                               const QVector3D& leather_base,
                               const QVector3D& cloth_base,
                               float mount_scale) -> HorseAnatomyComponent&;

} // namespace Render::Creature
