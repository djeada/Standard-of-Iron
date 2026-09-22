#pragma once
#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>

namespace Render {
namespace GL {
class Renderer;
class ResourceManager;
} // namespace GL
} // namespace Render

namespace Game::Systems {
struct RenderEffectsFrame;
struct ProjectileView;
struct ProjectileImpactEvent;
struct SpentProjectile;
} // namespace Game::Systems

namespace Render::GL {

[[nodiscard]] auto prewarm_projectile_geometry() -> bool;

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
  bool reduced_effects = false;

  [[nodiscard]] auto relation_for_owners(int attacker_owner,
                                         int target_owner) const -> ProjectileRelation;
};

inline constexpr int k_projectile_impact_effect_budget = 40;

[[nodiscard]] auto fireball_impact_envelope(float progress) -> float;

void render_projectiles(Renderer* renderer,
                        ResourceManager* resources,
                        const Game::Systems::RenderEffectsFrame& effects,
                        const ProjectileViewContext* view = nullptr);

void render_arrow_projectile(Renderer* renderer,
                             ResourceManager* resources,
                             const Game::Systems::ProjectileView& projectile,
                             const QVector3D& pos,
                             const QMatrix4x4& base_model,
                             ProjectileRelation relation = ProjectileRelation::Neutral);

void render_stone_projectile(Renderer* renderer,
                             ResourceManager* resources,
                             const Game::Systems::ProjectileView& projectile,
                             const QVector3D& pos,
                             const QMatrix4x4& base_model);

} // namespace Render::GL
