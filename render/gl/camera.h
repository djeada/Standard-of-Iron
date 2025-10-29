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
  friend void solveConstraints(Render::GL::Camera *self, bool allowTargetShift);

public:
  Camera();

  void setPosition(const QVector3D &position);
  void setTarget(const QVector3D &target);
  void setUp(const QVector3D &up);
  void lookAt(const QVector3D &position, const QVector3D &target,
              const QVector3D &up);

  void setPerspective(float fov, float aspect, float near_plane,
                      float far_plane);
  void setOrthographic(float left, float right, float bottom, float top,
                       float near_plane, float far_plane);

  void moveForward(float distance);
  void moveRight(float distance);
  void moveUp(float distance);
  void zoom(float delta);

  void zoomDistance(float delta);
  void rotate(float yaw, float pitch);

  void pan(float right_dist, float forwardDist);

  void elevate(float dy);
  void yaw(float degrees);
  void orbit(float yaw_deg, float pitch_deg);

  void update(float dt);
  auto screenToGround(qreal sx, qreal sy, qreal screenW, qreal screenH,
                      QVector3D &outWorld) const -> bool;
  auto worldToScreen(const QVector3D &world, qreal screenW, qreal screenH,
                     QPointF &outScreen) const -> bool;

  void setFollowEnabled(bool enable) { m_followEnabled = enable; }
  [[nodiscard]] auto isFollowEnabled() const -> bool { return m_followEnabled; }
  void setFollowLerp(float alpha) { m_followLerp = alpha; }
  void setFollowOffset(const QVector3D &off) { m_followOffset = off; }
  void captureFollowOffset() { m_followOffset = m_position - m_target; }
  void updateFollow(const QVector3D &targetCenter);

  void setRTSView(const QVector3D &center,
                  float distance = CameraDefaults::k_default_rts_distance,
                  float angle = CameraDefaults::k_default_rts_angle,
                  float yaw_deg = CameraDefaults::k_default_rts_yaw);
  void setTopDownView(const QVector3D &center,
                      float distance = CameraDefaults::k_default_rts_distance);
  void applySoftBoundaries(bool isPanning = false);

  [[nodiscard]] auto getViewMatrix() const -> QMatrix4x4;
  [[nodiscard]] auto getProjectionMatrix() const -> QMatrix4x4;
  [[nodiscard]] auto getViewProjectionMatrix() const -> QMatrix4x4;

  [[nodiscard]] auto getPosition() const -> const QVector3D & {
    return m_position;
  }
  [[nodiscard]] auto getTarget() const -> const QVector3D & { return m_target; }
  [[nodiscard]] auto getUpVector() const -> const QVector3D & { return m_up; }
  [[nodiscard]] auto getRightVector() const -> const QVector3D & {
    return m_right;
  }
  [[nodiscard]] auto getForwardVector() const -> const QVector3D & {
    return m_front;
  }
  [[nodiscard]] auto getDistance() const -> float;
  [[nodiscard]] auto getPitchDeg() const -> float;
  [[nodiscard]] auto getFOV() const -> float { return m_fov; }
  [[nodiscard]] auto getAspect() const -> float { return m_aspect; }
  [[nodiscard]] auto getNear() const -> float { return m_near_plane; }
  [[nodiscard]] auto getFar() const -> float { return m_far_plane; }

  [[nodiscard]] auto isInFrustum(const QVector3D &center,
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

  void updateVectors();

  void clampAboveGround();
  static void computeYawPitchFromOffset(const QVector3D &off, float &yaw_deg,
                                        float &pitch_deg);
};

} // namespace Render::GL
