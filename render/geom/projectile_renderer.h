#pragma once
#include <QMatrix4x4>
#include <QVector3D>

namespace Render {
namespace GL {
class Renderer;
class ResourceManager;
} // namespace GL
} // namespace Render

namespace Game::Systems {
class ProjectileSystem;
class Projectile;
class ArrowProjectile;
class StoneProjectile;
} // namespace Game::Systems

namespace Render::GL {

void render_projectiles(
    Renderer *renderer, ResourceManager *resources,
    const Game::Systems::ProjectileSystem &projectile_system);

void render_arrow_projectile(Renderer *renderer, ResourceManager *resources,
                             const Game::Systems::ArrowProjectile &arrow,
                             const QVector3D &pos,
                             const QMatrix4x4 &base_model);

void render_stone_projectile(Renderer *renderer, ResourceManager *resources,
                             const Game::Systems::StoneProjectile &stone,
                             const QVector3D &pos,
                             const QMatrix4x4 &base_model);

} // namespace Render::GL
