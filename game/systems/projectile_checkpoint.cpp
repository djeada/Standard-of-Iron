#include <QJsonObject>

#include "arrow_projectile.h"
#include "game/session/checkpoint_values.h"
#include "projectile_system.h"
#include "stone_projectile.h"

namespace Game::Systems {
namespace {

using Game::Session::CheckpointValues::read;
using Game::Session::CheckpointValues::write;

auto encode_vector(const QVector3D& value) -> QJsonArray {
  return write(value.x(), value.y(), value.z());
}

auto decode_vector(const QJsonValue& value, QVector3D& out) -> bool {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
  if (!read(value, x, y, z)) {
    return false;
  }
  out = QVector3D(x, y, z);
  return true;
}

} // namespace

auto ProjectileSystem::capture_checkpoint() const -> QJsonObject {
  QJsonArray projectiles;
  for (const auto& projectile : m_projectiles) {
    QJsonObject row{
        {"start", encode_vector(projectile->get_start())},
        {"end", encode_vector(projectile->get_end())},
        {"color", encode_vector(projectile->get_color())},
        {"locked_position", encode_vector(projectile->get_target_locked_position())},
        {"flight",
         write(projectile->get_speed(),
               projectile->get_arc_height(),
               projectile->get_progress(),
               projectile->get_scale(),
               projectile->get_kind(),
               projectile->is_active(),
               projectile->should_apply_damage(),
               projectile->get_damage(),
               projectile->get_attacker_id(),
               projectile->get_target_id())}};
    if (const auto* arrow = dynamic_cast<const ArrowProjectile*>(projectile.get())) {
      row["arrow"] = write(arrow->m_inv_dist,
                           arrow->m_is_ballista_bolt,
                           arrow->m_impact_radius,
                           arrow->m_splash_damage_multiplier,
                           arrow->m_friendly_fire,
                           arrow->m_visual_style,
                           arrow->m_length_scale,
                           arrow->m_roll_deg,
                           arrow->m_spin_rate_deg,
                           arrow->m_trail_alpha,
                           arrow->m_trail_length,
                           arrow->m_brightness);
    } else if (const auto* stone =
                   dynamic_cast<const StoneProjectile*>(projectile.get())) {
      row["stone"] = write(stone->m_inv_dist);
    } else {
      continue;
    }
    projectiles.append(row);
  }
  return {{"sequence", write(m_arrow_spawn_sequence, m_impact_sequence)},
          {"projectiles", projectiles}};
}

auto ProjectileSystem::restore_checkpoint(const QJsonObject& state) -> bool {
  std::uint32_t arrow_sequence = 0;
  std::uint64_t impact_sequence = 0;
  if (!read(state.value("sequence"), arrow_sequence, impact_sequence) ||
      !state.value("projectiles").isArray()) {
    return false;
  }
  std::vector<ProjectilePtr> restored;
  for (const auto value : state.value("projectiles").toArray()) {
    const auto row = value.toObject();
    QVector3D start;
    QVector3D end;
    QVector3D color;
    QVector3D locked_position;
    float speed = 0.0F;
    float arc = 0.0F;
    float progress = 0.0F;
    float scale = 1.0F;
    ProjectileKind kind{};
    bool active = false;
    bool damaging = false;
    int damage = 0;
    Engine::Core::EntityID attacker = 0;
    Engine::Core::EntityID target = 0;
    if (!decode_vector(row.value("start"), start) ||
        !decode_vector(row.value("end"), end) ||
        !decode_vector(row.value("color"), color) ||
        !decode_vector(row.value("locked_position"), locked_position) ||
        !read(row.value("flight"),
              speed,
              arc,
              progress,
              scale,
              kind,
              active,
              damaging,
              damage,
              attacker,
              target)) {
      return false;
    }
    float inverse_distance = 0.0F;
    if (row.contains("arrow")) {
      bool bolt = false;
      float radius = 0.0F;
      float splash = 0.0F;
      bool friendly_fire = false;
      ArrowVisualStyle style{};
      ArrowVisualProfile visual{.initial_progress = progress, .scale = scale};
      if (!read(row.value("arrow"),
                inverse_distance,
                bolt,
                radius,
                splash,
                friendly_fire,
                style,
                visual.length_scale,
                visual.roll_deg,
                visual.spin_rate_deg,
                visual.trail_alpha,
                visual.trail_length,
                visual.brightness)) {
        return false;
      }
      auto arrow = std::make_unique<ArrowProjectile>(start,
                                                     end,
                                                     color,
                                                     speed,
                                                     arc,
                                                     inverse_distance,
                                                     bolt,
                                                     kind,
                                                     damaging,
                                                     damage,
                                                     attacker,
                                                     target,
                                                     radius,
                                                     splash,
                                                     friendly_fire,
                                                     style,
                                                     visual,
                                                     locked_position);
      arrow->m_t = progress;
      arrow->m_active = active;
      restored.push_back(std::move(arrow));
    } else {
      if (!read(row.value("stone"), inverse_distance)) {
        return false;
      }
      auto stone = std::make_unique<StoneProjectile>(start,
                                                     end,
                                                     color,
                                                     speed,
                                                     arc,
                                                     inverse_distance,
                                                     scale,
                                                     damaging,
                                                     damage,
                                                     attacker,
                                                     target,
                                                     locked_position,
                                                     kind);
      stone->m_t = progress;
      stone->m_active = active;
      restored.push_back(std::move(stone));
    }
  }
  m_projectiles.swap(restored);
  m_arrow_spawn_sequence = arrow_sequence;
  m_impact_sequence = impact_sequence;
  m_impacts.clear();
  m_spent.clear();
  m_pending_launch_cues.clear();
  return true;
}

} // namespace Game::Systems
