#pragma once

#include <QMatrix4x4>
#include <QPointF>
#include <QVector3D>

namespace Render::GL {

class Camera {
      friend void solveConstraints(Render::GL::Camera* self, bool allowTargetShift);
public:
  Camera();

  void setPosition(const QVector3D &position);
  void setTarget(const QVector3D &target);
  void setUp(const QVector3D &up);
  void lookAt(const QVector3D &position, const QVector3D &target,
              const QVector3D &up);

  void setPerspective(float fov, float aspect, float nearPlane, float farPlane);
  void setOrthographic(float left, float right, float bottom, float top,
                       float nearPlane, float farPlane);

  void moveForward(float distance);
  void moveRight(float distance);
  void moveUp(float distance);
  void zoom(float delta);

  void zoomDistance(float delta);
  void rotate(float yaw, float pitch);

  void pan(float rightDist, float forwardDist);

  void elevate(float dy);
  void yaw(float degrees);
  void orbit(float yawDeg, float pitchDeg);

  void update(float dt);
  bool screenToGround(float sx, float sy, float screenW, float screenH,
                      QVector3D &outWorld) const;
  bool worldToScreen(const QVector3D &world, int screenW, int screenH,
                     QPointF &outScreen) const;

  void setFollowEnabled(bool enable) { m_followEnabled = enable; }
  bool isFollowEnabled() const { return m_followEnabled; }
  void setFollowLerp(float alpha) { m_followLerp = alpha; }
  void setFollowOffset(const QVector3D &off) { m_followOffset = off; }
  void captureFollowOffset() { m_followOffset = m_position - m_target; }
  void updateFollow(const QVector3D &targetCenter);

  void setRTSView(const QVector3D &center, float distance = 10.0f,
                  float angle = 45.0f, float yawDeg = 45.0f);
  void setTopDownView(const QVector3D &center, float distance = 10.0f);

  QMatrix4x4 getViewMatrix() const;
  QMatrix4x4 getProjectionMatrix() const;
  QMatrix4x4 getViewProjectionMatrix() const;

  const QVector3D &getPosition() const { return m_position; }
  const QVector3D &getTarget() const { return m_target; }
  float getDistance() const;
  float getPitchDeg() const;
  float getFOV() const { return m_fov; }
  float getAspect() const { return m_aspect; }
  float getNear() const { return m_nearPlane; }
  float getFar() const { return m_farPlane; }

private:
  QVector3D m_position{0.0f, 0.0f, 0.0f};
  QVector3D m_target{0.0f, 0.0f, -1.0f};
  QVector3D m_up{0.0f, 1.0f, 0.0f};
  QVector3D m_front{0.0f, 0.0f, -1.0f};
  QVector3D m_right{1.0f, 0.0f, 0.0f};

  bool m_isPerspective = true;
  float m_fov = 45.0f;
  float m_aspect = 16.0f / 9.0f;

  float m_nearPlane = 1.0f;
  float m_farPlane = 200.0f;

  float m_orthoLeft = -10.0f;
  float m_orthoRight = 10.0f;
  float m_orthoBottom = -10.0f;
  float m_orthoTop = 10.0f;

  bool m_followEnabled = false;
  QVector3D m_followOffset{0, 0, 0};
  float m_followLerp = 0.15f;

  float m_groundY = 0.0f;
  float m_minHeight = 0.5f;

  float m_pitchMinDeg = -85.0f;
  float m_pitchMaxDeg = -5.0f;

  bool m_orbitPending = false;
  float m_orbitStartYaw = 0.0f;
  float m_orbitStartPitch = 0.0f;
  float m_orbitTargetYaw = 0.0f;
  float m_orbitTargetPitch = 0.0f;
  float m_orbitTime = 0.0f;
  float m_orbitDuration = 0.12f;

  void updateVectors();
  void clampAboveGround();
  void computeYawPitchFromOffset(const QVector3D &off, float &yawDeg,
                                 float &pitchDeg) const;
};

} // namespace Render::GL