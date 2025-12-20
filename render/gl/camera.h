#pragma once

#include <QMatrix4x4>
#include <QPointF>
#include <QVector3D>

namespace Render::GL {

namespace CameraDefaults {
inline constexpr float k_default_rts_distance = 10.0F;
inline constexpr float k_default_rts_angle = 45.0F;
inline constexpr float k_default_rts_yaw = 45.0F;
inline constexpr float k_default_fov = 45.0F;
inline constexpr float k_default_aspect_ratio = 16.0F / 9.0F;
inline constexpr float k_default_far_plane = 200.0F;
inline constexpr float k_default_ortho_size = 10.0F;
inline constexpr float k_default_pitch_min = -85.0F;
} // namespace CameraDefaults

class Camera {
  friend void solve_constraints(Render::GL::Camera *self,
                                bool allowTargetShift);

public:
  Camera();

  void set_position(const QVector3D &position);
  void set_target(const QVector3D &target);
  void set_up(const QVector3D &up);
  void look_at(const QVector3D &position, const QVector3D &target,
               const QVector3D &up);

  void set_perspective(float fov, float aspect, float near_plane,
                       float far_plane);
  void set_orthographic(float left, float right, float bottom, float top,
                        float near_plane, float far_plane);

  void move_forward(float distance);
  void move_right(float distance);
  void move_up(float distance);
  void zoom(float delta);

  void zoom_distance(float delta);
  void rotate(float yaw, float pitch);

  void pan(float right_dist, float forward_dist);

  void elevate(float dy);
  void yaw(float degrees);
  void orbit(float yaw_deg, float pitch_deg);

  void update(float dt);
  auto screen_to_ground(qreal sx, qreal sy, qreal screenW, qreal screenH,
                        QVector3D &outWorld) const -> bool;
  auto world_to_screen(const QVector3D &world, qreal screenW, qreal screenH,
                       QPointF &outScreen) const -> bool;

  void set_follow_enabled(bool enable) { m_followEnabled = enable; }
  [[nodiscard]] auto is_follow_enabled() const -> bool {
    return m_followEnabled;
  }
  void set_follow_lerp(float alpha) { m_followLerp = alpha; }
  void set_follow_offset(const QVector3D &off) { m_followOffset = off; }
  void capture_follow_offset() { m_followOffset = m_position - m_target; }
  void update_follow(const QVector3D &targetCenter);

  void set_rts_view(const QVector3D &center,
                    float distance = CameraDefaults::k_default_rts_distance,
                    float angle = CameraDefaults::k_default_rts_angle,
                    float yaw_deg = CameraDefaults::k_default_rts_yaw);
  void
  set_top_down_view(const QVector3D &center,
                    float distance = CameraDefaults::k_default_rts_distance);
  void apply_soft_boundaries(bool isPanning = false);

  [[nodiscard]] auto get_view_matrix() const -> QMatrix4x4;
  [[nodiscard]] auto get_projection_matrix() const -> QMatrix4x4;
  [[nodiscard]] auto get_view_projection_matrix() const -> QMatrix4x4;

  [[nodiscard]] auto get_target() const -> const QVector3D & {
    return m_target;
  }
  [[nodiscard]] auto get_up_vector() const -> const QVector3D & { return m_up; }
  [[nodiscard]] auto get_right_vector() const -> const QVector3D & {
    return m_right;
  }
  [[nodiscard]] auto get_forward_vector() const -> const QVector3D & {
    return m_front;
  }
  [[nodiscard]] auto get_position() const -> const QVector3D & {
    return m_position;
  }
  [[nodiscard]] auto get_distance() const -> float;
  [[nodiscard]] auto get_pitch_deg() const -> float;
  [[nodiscard]] auto get_fov() const -> float { return m_fov; }
  [[nodiscard]] auto get_aspect() const -> float { return m_aspect; }
  [[nodiscard]] auto get_near() const -> float { return m_near_plane; }
  [[nodiscard]] auto get_far() const -> float { return m_far_plane; }

  [[nodiscard]] auto is_in_frustum(const QVector3D &center,
                                   float radius) const -> bool;

private:
  QVector3D m_position{0.0F, 0.0F, 0.0F};
  QVector3D m_target{0.0F, 0.0F, -1.0F};
  QVector3D m_up{0.0F, 1.0F, 0.0F};
  QVector3D m_front{0.0F, 0.0F, -1.0F};
  QVector3D m_right{1.0F, 0.0F, 0.0F};
  QVector3D m_lastPosition;

  bool m_isPerspective = true;
  float m_fov = CameraDefaults::k_default_fov;
  float m_aspect = CameraDefaults::k_default_aspect_ratio;

  float m_near_plane = 1.0F;
  float m_far_plane = CameraDefaults::k_default_far_plane;

  float m_orthoLeft = -CameraDefaults::k_default_ortho_size;
  float m_orthoRight = CameraDefaults::k_default_ortho_size;
  float m_orthoBottom = -CameraDefaults::k_default_ortho_size;
  float m_orthoTop = CameraDefaults::k_default_ortho_size;

  bool m_followEnabled = false;
  QVector3D m_followOffset{0, 0, 0};
  float m_followLerp = 0.15F;

  float m_ground_y = 0.0F;
  float m_min_height = 0.5F;

  float m_pitchMinDeg = CameraDefaults::k_default_pitch_min;
  float m_pitchMaxDeg = -5.0F;

  bool m_orbitPending = false;
  float m_orbitStartYaw = 0.0F;
  float m_orbitStartPitch = 0.0F;
  float m_orbitTargetYaw = 0.0F;
  float m_orbitTargetPitch = 0.0F;
  float m_orbitTime = 0.0F;
  float m_orbitDuration = 0.12F;

  void update_vectors();

  void clamp_above_ground();
  static void compute_yaw_pitch_from_offset(const QVector3D &off,
                                            float &yaw_deg, float &pitch_deg);
};

} // namespace Render::GL
