#pragma once

#include <QVector3D>

namespace Render::GL {

enum class BuildingState { Normal, Damaged, Destroyed };

inline constexpr float HEALTH_THRESHOLD_NORMAL = 0.70F;
inline constexpr float HEALTH_THRESHOLD_DAMAGED = 0.30F;

inline constexpr float HEALTHBAR_PULSE_MIN = 0.7F;
inline constexpr float HEALTHBAR_PULSE_AMPLITUDE = 0.3F;
inline constexpr float HEALTHBAR_PULSE_SPEED = 4.0F;

namespace HealthBarColors {
inline constexpr float NORMAL_BRIGHT_R = 0.10F;
inline constexpr float NORMAL_BRIGHT_G = 1.0F;
inline constexpr float NORMAL_BRIGHT_B = 0.30F;

inline constexpr float NORMAL_DARK_R = 0.05F;
inline constexpr float NORMAL_DARK_G = 0.60F;
inline constexpr float NORMAL_DARK_B = 0.15F;

inline constexpr float DAMAGED_BRIGHT_R = 1.0F;
inline constexpr float DAMAGED_BRIGHT_G = 0.75F;
inline constexpr float DAMAGED_BRIGHT_B = 0.10F;

inline constexpr float DAMAGED_DARK_R = 0.70F;
inline constexpr float DAMAGED_DARK_G = 0.45F;
inline constexpr float DAMAGED_DARK_B = 0.05F;

inline constexpr float CRITICAL_BRIGHT_R = 1.0F;
inline constexpr float CRITICAL_BRIGHT_G = 0.15F;
inline constexpr float CRITICAL_BRIGHT_B = 0.15F;

inline constexpr float CRITICAL_DARK_R = 0.70F;
inline constexpr float CRITICAL_DARK_G = 0.08F;
inline constexpr float CRITICAL_DARK_B = 0.08F;

inline const QVector3D NORMAL_BRIGHT{NORMAL_BRIGHT_R, NORMAL_BRIGHT_G,
                                     NORMAL_BRIGHT_B};
inline const QVector3D NORMAL_DARK{NORMAL_DARK_R, NORMAL_DARK_G, NORMAL_DARK_B};
inline const QVector3D DAMAGED_BRIGHT{DAMAGED_BRIGHT_R, DAMAGED_BRIGHT_G,
                                      DAMAGED_BRIGHT_B};
inline const QVector3D DAMAGED_DARK{DAMAGED_DARK_R, DAMAGED_DARK_G,
                                    DAMAGED_DARK_B};
inline const QVector3D CRITICAL_BRIGHT{CRITICAL_BRIGHT_R, CRITICAL_BRIGHT_G,
                                       CRITICAL_BRIGHT_B};
inline const QVector3D CRITICAL_DARK{CRITICAL_DARK_R, CRITICAL_DARK_G,
                                     CRITICAL_DARK_B};

inline const QVector3D BORDER{0.45F, 0.45F, 0.50F};
inline const QVector3D INNER_BORDER{0.25F, 0.25F, 0.28F};
inline const QVector3D BACKGROUND{0.08F, 0.08F, 0.10F};
inline const QVector3D SEGMENT{0.35F, 0.35F, 0.40F};
inline const QVector3D SEGMENT_HIGHLIGHT{0.55F, 0.55F, 0.60F};
inline const QVector3D SHINE{1.0F, 1.0F, 1.0F};
inline const QVector3D GLOW_ATTACK{0.9F, 0.2F, 0.2F};
} // namespace HealthBarColors

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
