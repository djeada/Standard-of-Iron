#pragma once
#include <QQuaternion>
#include <QVector3D>

#include <cstdint>
#include <vector>

#include "arrow_instance.h"
#include "arrow_visual_style.h"
#include "projectile_impact_event.h"
#include "projectile_kind.h"
#include "spent_projectile.h"

namespace Game::Systems {

struct HealingBeamView {
  QVector3D start;
  QVector3D end;
  QVector3D color;
  float progress{};
  float beam_width{};
  float intensity{};
};

enum class ProjectileShape : std::uint8_t {
  Arrow,
  Stone
};

struct ProjectileView {
  ProjectileShape shape{ProjectileShape::Arrow};
  QVector3D start;
  QVector3D end;
  QVector3D color;
  float progress{};
  float arc_height{};
  float scale{1.0F};
  float length_scale{1.0F};
  float roll_deg{};
  float spin_rate_deg{};
  float trail_alpha{};
  float trail_length{};
  float brightness{1.0F};
  ProjectileKind kind{ProjectileKind::Arrow};
  ArrowVisualStyle visual_style{ArrowVisualStyle::Focused};
  bool ballista_bolt{false};
  int attacker_owner{0};
  int target_owner{0};
};

struct RockfallBoulderView {
  QVector3D position;
  QQuaternion orientation;
  float radius{1.0F};

  float sink{0.0F};
  std::uint32_t seed{0};
};

struct RockfallCacheView {
  QVector3D position;

  QVector3D downhill;
  QVector3D zone_target;
  float zone_radius{0.0F};
  float boulder_radius{0.55F};
  int boulders_left{0};
  int owner_id{-1};

  float push_progress{-1.0F};

  bool threatened{false};

  bool armed{true};
  std::uint32_t seed{0};
};

struct RockfallDustView {
  QVector3D position;
  float scale{1.0F};
  float age{0.0F};
};

struct RenderEffectsFrame {
  std::vector<ArrowInstance> arrows;
  std::vector<HealingBeamView> healing_beams;
  std::vector<ProjectileView> projectiles;
  std::vector<SpentProjectile> spent_projectiles;
  std::vector<ProjectileImpactEvent> projectile_impacts;
  std::vector<int> impact_attacker_owners;
  std::vector<int> impact_target_owners;
  std::vector<RockfallBoulderView> rockfall_boulders;
  std::vector<RockfallCacheView> rockfall_caches;
  std::vector<RockfallDustView> rockfall_dust;
};

} // namespace Game::Systems
