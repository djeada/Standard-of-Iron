#pragma once
#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <functional>

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

enum class ProjectileRelation : std::uint8_t {
  Neutral,
  Outgoing,
  Incoming,
};

[[nodiscard]] auto
classify_projectile_relation(int local_owner_id,
                             int attacker_owner_id,
                             int target_owner_id) -> ProjectileRelation;

struct ProjectileViewContext {
  int local_owner_id = 0;
  std::function<int(std::uint64_t)> owner_of;
  bool reduced_effects = false;

  [[nodiscard]] auto relation_for(std::uint64_t attacker_id,
                                  std::uint64_t target_id) const -> ProjectileRelation;
};

inline constexpr int k_projectile_impact_effect_budget = 40;

void render_projectiles(Renderer* renderer,
                        ResourceManager* resources,
                        const Game::Systems::ProjectileSystem& projectile_system,
                        const ProjectileViewContext* view = nullptr);

void render_arrow_projectile(Renderer* renderer,
                             ResourceManager* resources,
                             const Game::Systems::ArrowProjectile& arrow,
                             const QVector3D& pos,
                             const QMatrix4x4& base_model,
                             ProjectileRelation relation = ProjectileRelation::Neutral);

void render_stone_projectile(Renderer* renderer,
                             ResourceManager* resources,
                             const Game::Systems::StoneProjectile& stone,
                             const QVector3D& pos,
                             const QMatrix4x4& base_model);

} // namespace Render::GL
