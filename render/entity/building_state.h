#pragma once

#include <QVector3D>

namespace Render::GL {

enum class BuildingState { Normal, Damaged, Destroyed };

// Health thresholds
inline constexpr float HEALTH_THRESHOLD_NORMAL = 0.70F;
inline constexpr float HEALTH_THRESHOLD_DAMAGED = 0.30F;

// Health bar visual constants
inline constexpr float HEALTHBAR_PULSE_MIN = 0.7F;
inline constexpr float HEALTHBAR_PULSE_AMPLITUDE = 0.3F;
inline constexpr float HEALTHBAR_PULSE_SPEED = 4.0F;

// Health bar colors
namespace HealthBarColors {
  inline const QVector3D NORMAL_BRIGHT{0.10F, 1.0F, 0.30F};
  inline const QVector3D NORMAL_DARK{0.05F, 0.60F, 0.15F};
  
  inline const QVector3D DAMAGED_BRIGHT{1.0F, 0.75F, 0.10F};
  inline const QVector3D DAMAGED_DARK{0.70F, 0.45F, 0.05F};
  
  inline const QVector3D CRITICAL_BRIGHT{1.0F, 0.15F, 0.15F};
  inline const QVector3D CRITICAL_DARK{0.70F, 0.08F, 0.08F};
  
  inline const QVector3D BORDER{0.45F, 0.45F, 0.50F};
  inline const QVector3D INNER_BORDER{0.25F, 0.25F, 0.28F};
  inline const QVector3D BACKGROUND{0.08F, 0.08F, 0.10F};
  inline const QVector3D SEGMENT{0.35F, 0.35F, 0.40F};
  inline const QVector3D SEGMENT_HIGHLIGHT{0.55F, 0.55F, 0.60F};
  inline const QVector3D SHINE{1.0F, 1.0F, 1.0F};
  inline const QVector3D GLOW_ATTACK{0.9F, 0.2F, 0.2F};
}

inline auto get_building_state(float health_ratio) -> BuildingState {
  if (health_ratio >= HEALTH_THRESHOLD_NORMAL) {
    return BuildingState::Normal;
  } else if (health_ratio >= HEALTH_THRESHOLD_DAMAGED) {
    return BuildingState::Damaged;
  } else {
    return BuildingState::Destroyed;
  }
}

} // namespace Render::GL
