#pragma once
#include <QVector3D>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "../core/entity.h"
#include "../core/system.h"
#include "../core/world.h"
#include "../game_config.h"
#include "arrow_visual_profile.h"
#include "projectile.h"
#include "spent_projectile.h"

namespace Game::Systems {

struct ProjectileImpactEvent {
  std::uint64_t sequence{0};
  QVector3D position;
  QVector3D incoming_direction;
  QVector3D color;
  ProjectileKind kind{ProjectileKind::Arrow};
  float age{0.0F};
  float lifetime{0.65F};
  float scale{1.0F};
  bool ballista_bolt{false};
  bool hit_target{false};
  bool damage_applied{false};
  Engine::Core::EntityID attacker_id{0};
  Engine::Core::EntityID target_id{0};
};

class ProjectileSystem : public Engine::Core::System {
public:
  ProjectileSystem();
  void update(Engine::Core::World* world, float delta_time) override;

  void spawn_arrow(const QVector3D& start,
                   const QVector3D& end,
                   const QVector3D& color,
                   float speed = 8.0F,
                   bool is_ballista_bolt = false,
                   ProjectileKind kind = ProjectileKind::Arrow,
                   bool should_apply_damage = false,
                   int damage = 0,
                   Engine::Core::EntityID attacker_id = 0,
                   Engine::Core::EntityID target_id = 0,
                   float impact_radius = 0.0F,
                   float splash_damage_multiplier = 0.6F,
                   bool friendly_fire = false,
                   ArrowVisualStyle visual_style = ArrowVisualStyle::Focused,
                   std::optional<QVector3D> target_origin_at_launch = std::nullopt,

                   float arc_scale = 1.0F);

  void spawn_stone(const QVector3D& start,
                   const QVector3D& end,
                   const QVector3D& color,
                   float speed = 5.0F,
                   float scale = 1.0F,
                   bool should_apply_damage = false,
                   int damage = 0,
                   Engine::Core::EntityID attacker_id = 0,
                   Engine::Core::EntityID target_id = 0,
                   std::optional<QVector3D> target_origin_at_launch = std::nullopt,
                   ProjectileKind kind = ProjectileKind::Stone);

  [[nodiscard]] auto projectiles() const -> const std::vector<ProjectilePtr>& {
    return m_projectiles;
  }
  [[nodiscard]] auto impacts() const -> const std::vector<ProjectileImpactEvent>& {
    return m_impacts;
  }
  [[nodiscard]] auto spent_projectiles() const -> const std::vector<SpentProjectile>& {
    return m_spent;
  }

private:
  struct ImpactResolution {
    bool hit_target{false};
    bool damage_applied{false};
  };

  [[nodiscard]] auto resolve_impact(Engine::Core::World* world,
                                    Projectile* projectile) -> ImpactResolution;
  void publish_impact(const Projectile& projectile, const ImpactResolution& resolution);
  void record_spent_projectile(const Projectile& projectile,
                               const QVector3D& incoming_direction,
                               bool ballista_bolt);

  static constexpr int k_volley_arrow_threshold = 4;

  std::vector<ProjectilePtr> m_projectiles;
  std::vector<ProjectileImpactEvent> m_impacts;
  std::vector<SpentProjectile> m_spent;
  ArrowConfig m_arrow_config;
  std::uint32_t m_arrow_spawn_sequence{0};
  std::uint64_t m_impact_sequence{0};
  int m_arrows_launched_this_tick{0};
};

} // namespace Game::Systems
