#include "camera.h"
#include "../../game/map/visibility_service.h"
#include <QtMath>
#include <cmath>
#include <algorithm>
#include <limits>

namespace Render::GL {

// -------- internal helpers, do not change public API --------
namespace {
constexpr float kEps      = 1e-6f;
constexpr float kTiny     = 1e-4f;
constexpr float kMinDist  = 1.0f;    // zoomDistance floor
constexpr float kMaxDist  = 500.0f;  // zoomDistance ceiling
constexpr float kMinFov   = 1.0f;    // perspective clamp (deg)
constexpr float kMaxFov   = 89.0f;

inline bool finite(const QVector3D& v) {
  return qIsFinite(v.x()) && qIsFinite(v.y()) && qIsFinite(v.z());
}
inline bool finite(float v) { return qIsFinite(v); }

inline QVector3D safeNormalize(const QVector3D& v,
                               const QVector3D& fallback,
                               float eps = kEps) {
  if (!finite(v)) return fallback;
  float len2 = v.lengthSquared();
  if (len2 < eps) return fallback;
  return v / std::sqrt(len2);
}

// Keep up-vector not collinear with front; pick a sane orthonormal basis.
inline void orthonormalize(const QVector3D& frontIn,
                           QVector3D& frontOut,
                           QVector3D& rightOut,
                           QVector3D& upOut) {
  QVector3D worldUp(0.f, 1.f, 0.f);
  QVector3D f = safeNormalize(frontIn, QVector3D(0, 0, -1));
  // If front is near-collinear with worldUp, choose another temp up.
  QVector3D u = (std::abs(QVector3D::dotProduct(f, worldUp)) > 1.f - 1e-3f)
                  ? QVector3D(0, 0, 1)
                  : worldUp;
  QVector3D r = QVector3D::crossProduct(f, u);
  if (r.lengthSquared() < kEps) r = QVector3D(1, 0, 0);
  r = r.normalized();
  u = QVector3D::crossProduct(r, f).normalized();

  frontOut = f;
  rightOut = r;
  upOut    = u;
}

inline void clampOrthoBox(float& left, float& right,
                          float& bottom, float& top) {
  if (left == right) {
    left -= 0.5f; right += 0.5f;
  } else if (left > right) {
    std::swap(left, right);
  }
  if (bottom == top) {
    bottom -= 0.5f; top += 0.5f;
  } else if (bottom > top) {
    std::swap(bottom, top);
  }
}
} // anonymous namespace
// ------------------------------------------------------------

Camera::Camera() { updateVectors(); }

void Camera::setPosition(const QVector3D &position) {
  if (!finite(position)) return; // ignore invalid input
  m_position = position;
  clampAboveGround();
  // keep looking at current target; repair front/up basis if needed
  QVector3D newFront = (m_target - m_position);
  orthonormalize(newFront, m_front, m_right, m_up);
}

void Camera::setTarget(const QVector3D &target) {
  if (!finite(target)) return;
  m_target = target;

  QVector3D dir = (m_target - m_position);
  if (dir.lengthSquared() < kEps) {
    // Nudge target forward to avoid zero-length front
    m_target = m_position + (m_front.lengthSquared() < kEps
                              ? QVector3D(0, 0, -1)
                              : m_front);
    dir = (m_target - m_position);
  }
  orthonormalize(dir, m_front, m_right, m_up);
  clampAboveGround();
}

void Camera::setUp(const QVector3D &up) {
  if (!finite(up)) return;
  QVector3D upN = up;
  if (upN.lengthSquared() < kEps) upN = QVector3D(0, 1, 0);
  // Ensure up is not collinear with front; re-orthonormalize
  orthonormalize(m_target - m_position, m_front, m_right, m_up);
}

void Camera::lookAt(const QVector3D &position, const QVector3D &target,
                    const QVector3D &up) {
  if (!finite(position) || !finite(target) || !finite(up)) return;
  m_position = position;
  m_target   = (position == target)
                 ? position + QVector3D(0, 0, -1)
                 : target;

  QVector3D f = (m_target - m_position);
  m_up = up.lengthSquared() < kEps ? QVector3D(0, 1, 0) : up.normalized();
  orthonormalize(f, m_front, m_right, m_up);
  clampAboveGround();
}

void Camera::setPerspective(float fov, float aspect, float nearPlane,
                            float farPlane) {
  if (!finite(fov) || !finite(aspect) || !finite(nearPlane) || !finite(farPlane))
    return;

  m_isPerspective = true;
  // Robust clamps
  m_fov       = std::clamp(fov, kMinFov, kMaxFov);
  m_aspect    = std::max(aspect, 1e-6f);
  m_nearPlane = std::max(nearPlane, 1e-4f);
  m_farPlane  = std::max(farPlane, m_nearPlane + 1e-3f);
}

void Camera::setOrthographic(float left, float right, float bottom, float top,
                             float nearPlane, float farPlane) {
  if (!finite(left) || !finite(right) || !finite(bottom) || !finite(top) ||
      !finite(nearPlane) || !finite(farPlane))
    return;

  m_isPerspective = false;
  clampOrthoBox(left, right, bottom, top);
  m_orthoLeft   = left;
  m_orthoRight  = right;
  m_orthoBottom = bottom;
  m_orthoTop    = top;
  m_nearPlane   = std::min(nearPlane, farPlane - 1e-3f);
  m_farPlane    = std::max(farPlane, m_nearPlane + 1e-3f);
}

void Camera::moveForward(float distance) {
  if (!finite(distance)) return;
  m_position += m_front * distance;
  m_target    = m_position + m_front;
  clampAboveGround();
}

void Camera::moveRight(float distance) {
  if (!finite(distance)) return;
  m_position += m_right * distance;
  m_target    = m_position + m_front;
  clampAboveGround();
}

void Camera::moveUp(float distance) {
  if (!finite(distance)) return;
  m_position += QVector3D(0, 1, 0) * distance;
  clampAboveGround();
  m_target = m_position + m_front;
}

void Camera::zoom(float delta) {
  if (!finite(delta)) return;
  if (m_isPerspective) {
    m_fov = qBound(kMinFov, m_fov - delta, kMaxFov);
  } else {
    // Keep scale positive and bounded
    float scale = 1.0f + delta * 0.1f;
    if (!finite(scale) || scale <= 0.05f) scale = 0.05f;
    if (scale > 20.0f) scale = 20.0f;
    m_orthoLeft   *= scale;
    m_orthoRight  *= scale;
    m_orthoBottom *= scale;
    m_orthoTop    *= scale;
    clampOrthoBox(m_orthoLeft, m_orthoRight, m_orthoBottom, m_orthoTop);
  }
}

void Camera::zoomDistance(float delta) {
  if (!finite(delta)) return;

  QVector3D offset = m_position - m_target;
  float r = offset.length();
  if (r < kTiny) r = kTiny;

  float factor = 1.0f - delta * 0.15f;
  if (!finite(factor)) factor = 1.0f;
  factor = std::clamp(factor, 0.1f, 10.0f);

  float newR = std::clamp(r * factor, kMinDist, kMaxDist);

  QVector3D dir = safeNormalize(offset, QVector3D(0,0,1));
  m_position = m_target + dir * newR;
  clampAboveGround();
  QVector3D f = (m_target - m_position);
  orthonormalize(f, m_front, m_right, m_up);
}

void Camera::rotate(float yaw, float pitch) { orbit(yaw, pitch); }

void Camera::pan(float rightDist, float forwardDist) {
  if (!finite(rightDist) || !finite(forwardDist)) return;

  QVector3D right = m_right;
  QVector3D front = m_front;
  front.setY(0.0f);
  if (front.lengthSquared() > 0) front.normalize();

  QVector3D delta = right * rightDist + front * forwardDist;
  if (!finite(delta)) return;

  m_position += delta;
  m_target   += delta;
  clampAboveGround();
}

void Camera::elevate(float dy) {
  if (!finite(dy)) return;
  m_position.setY(m_position.y() + dy);
  clampAboveGround();
}

void Camera::yaw(float degrees) {
  if (!finite(degrees)) return;
  // computeYawPitchFromOffset already guards degeneracy
  orbit(degrees, 0.0f);
}

void Camera::orbit(float yawDeg, float pitchDeg) {
  if (!finite(yawDeg) || !finite(pitchDeg)) return;

  QVector3D offset = m_position - m_target;
  float curYaw = 0.f, curPitch = 0.f;
  computeYawPitchFromOffset(offset, curYaw, curPitch);

  m_orbitStartYaw   = curYaw;
  m_orbitStartPitch = curPitch;
  m_orbitTargetYaw  = curYaw + yawDeg;
  m_orbitTargetPitch = qBound(m_pitchMinDeg, curPitch + pitchDeg, m_pitchMaxDeg);
  m_orbitTime    = 0.0f;
  m_orbitPending = true;
}

void Camera::update(float dt) {
  if (!m_orbitPending) return;
  if (!finite(dt)) return;

  m_orbitTime += std::max(0.0f, dt);
  float t = (m_orbitDuration <= 0.0f)
              ? 1.0f
              : std::clamp(m_orbitTime / m_orbitDuration, 0.0f, 1.0f);

  // Smoothstep
  float s = t * t * (3.0f - 2.0f * t);

  // Interpolate yaw/pitch
  float newYaw   = m_orbitStartYaw   + (m_orbitTargetYaw   - m_orbitStartYaw)   * s;
  float newPitch = m_orbitStartPitch + (m_orbitTargetPitch - m_orbitStartPitch) * s;

  QVector3D offset = m_position - m_target;
  float r = offset.length();
  if (r < kTiny) r = kTiny;

  float yawRad   = qDegreesToRadians(newYaw);
  float pitchRad = qDegreesToRadians(newPitch);
  QVector3D newDir(std::sin(yawRad) * std::cos(pitchRad),
                   std::sin(pitchRad),
                   std::cos(yawRad) * std::cos(pitchRad));

  QVector3D fwd = safeNormalize(newDir, m_front);
  m_position = m_target - fwd * r;
  clampAboveGround();
  orthonormalize((m_target - m_position), m_front, m_right, m_up);

  if (t >= 1.0f) {
    m_orbitPending = false;
  }
}

bool Camera::screenToGround(float sx, float sy, float screenW, float screenH,
                            QVector3D &outWorld) const {
  if (screenW <= 0 || screenH <= 0) return false;
  if (!finite(sx) || !finite(sy))   return false;

  float x = (2.0f * sx / screenW) - 1.0f;
  float y = 1.0f - (2.0f * sy / screenH);

  bool ok = false;
  QMatrix4x4 invVP = (getProjectionMatrix() * getViewMatrix()).inverted(&ok);
  if (!ok) return false;

  QVector4D nearClip(x, y, 0.0f, 1.0f);
  QVector4D farClip (x, y, 1.0f, 1.0f);
  QVector4D nearWorld4 = invVP * nearClip;
  QVector4D farWorld4  = invVP * farClip;

  if (std::abs(nearWorld4.w()) < kEps || std::abs(farWorld4.w()) < kEps) return false;

  QVector3D rayOrigin = (nearWorld4 / nearWorld4.w()).toVector3D();
  QVector3D rayEnd    = (farWorld4  / farWorld4.w()).toVector3D();
  if (!finite(rayOrigin) || !finite(rayEnd)) return false;

  QVector3D rayDir = safeNormalize(rayEnd - rayOrigin, QVector3D(0, -1, 0));
  if (std::abs(rayDir.y()) < kEps) return false;

  float t = (m_groundY - rayOrigin.y()) / rayDir.y();
  if (!finite(t) || t < 0.0f) return false;

  outWorld = rayOrigin + rayDir * t;
  return finite(outWorld);
}

bool Camera::worldToScreen(const QVector3D &world, int screenW, int screenH,
                           QPointF &outScreen) const {
  if (screenW <= 0 || screenH <= 0) return false;
  if (!finite(world)) return false;

  QVector4D clip = getProjectionMatrix() * getViewMatrix() * QVector4D(world, 1.0f);
  if (std::abs(clip.w()) < kEps) return false;

  QVector3D ndc = (clip / clip.w()).toVector3D();
  if (!qIsFinite(ndc.x()) || !qIsFinite(ndc.y()) || !qIsFinite(ndc.z())) return false;
  if (ndc.z() < -1.0f || ndc.z() > 1.0f) return false;

  float sx = (ndc.x() * 0.5f + 0.5f) * float(screenW);
  float sy = (1.0f - (ndc.y() * 0.5f + 0.5f)) * float(screenH);
  outScreen = QPointF(sx, sy);
  return qIsFinite(sx) && qIsFinite(sy);
}

void Camera::updateFollow(const QVector3D &targetCenter) {
  if (!m_followEnabled) return;
  if (!finite(targetCenter)) return;

  if (m_followOffset.lengthSquared() < 1e-5f) {
    m_followOffset = m_position - m_target; // initialize lazily
  }
  QVector3D desiredPos = targetCenter + m_followOffset;
  QVector3D newPos =
      (m_followLerp >= 0.999f)
          ? desiredPos
          : (m_position + (desiredPos - m_position) * std::clamp(m_followLerp, 0.0f, 1.0f));

  if (!finite(newPos)) return;

  m_target   = targetCenter;
  m_position = newPos;
  clampAboveGround();
  orthonormalize((m_target - m_position), m_front, m_right, m_up);
}

void Camera::setRTSView(const QVector3D &center, float distance, float angle,
                        float yawDeg) {
  if (!finite(center) || !finite(distance) || !finite(angle) || !finite(yawDeg)) return;

  m_target = center;

  distance = std::max(distance, 0.01f);
  float pitchRad = qDegreesToRadians(angle);
  float yawRad   = qDegreesToRadians(yawDeg);

  float y    = distance * qSin(pitchRad);
  float horiz= distance * qCos(pitchRad);

  float x = std::sin(yawRad) * horiz;
  float z = std::cos(yawRad) * horiz;

  m_position = center + QVector3D(x, y, z);

  QVector3D f = (m_target - m_position);
  orthonormalize(f, m_front, m_right, m_up);
  clampAboveGround();
}

void Camera::setTopDownView(const QVector3D &center, float distance) {
  if (!finite(center) || !finite(distance)) return;

  m_target   = center;
  m_position = center + QVector3D(0, std::max(distance, 0.01f), 0);
  m_up       = QVector3D(0, 0, -1);
  m_front    = safeNormalize((m_target - m_position), QVector3D(0,0,1));
  updateVectors();
  clampAboveGround();
}

QMatrix4x4 Camera::getViewMatrix() const {
  QMatrix4x4 view;
  view.lookAt(m_position, m_target, m_up);
  return view;
}

QMatrix4x4 Camera::getProjectionMatrix() const {
  QMatrix4x4 projection;
  if (m_isPerspective) {
    // perspective() assumes sane inputs — we enforce those in setters
    projection.perspective(m_fov, m_aspect, m_nearPlane, m_farPlane);
  } else {
    // get local copies because this method is const and clampOrthoBox
    // expects non-const references
    float left = m_orthoLeft;
    float right = m_orthoRight;
    float bottom = m_orthoBottom;
    float top = m_orthoTop;
    clampOrthoBox(left, right, bottom, top);
    projection.ortho(left, right, bottom, top, m_nearPlane, m_farPlane);
  }
  return projection;
}

QMatrix4x4 Camera::getViewProjectionMatrix() const {
  return getProjectionMatrix() * getViewMatrix();
}

float Camera::getDistance() const { return (m_position - m_target).length(); }

float Camera::getPitchDeg() const {
  QVector3D off = m_position - m_target;
  QVector3D dir = -off;
  if (dir.lengthSquared() < 1e-6f) return 0.0f;
  float lenXZ = std::sqrt(dir.x() * dir.x() + dir.z() * dir.z());
  float pitchRad = std::atan2(dir.y(), lenXZ);
  return qRadiansToDegrees(pitchRad);
}

void Camera::updateVectors() {
  QVector3D f = (m_target - m_position);
  orthonormalize(f, m_front, m_right, m_up);
}

void Camera::clampAboveGround() {
  if (!qIsFinite(m_position.y())) return;

  if (m_position.y() < m_groundY + m_minHeight) {
    m_position.setY(m_groundY + m_minHeight);
  }

  auto &vis = Game::Map::VisibilityService::instance();
  if (vis.isInitialized()) {
    const float tile  = vis.getTileSize();
    const float halfW = vis.getWidth()  * 0.5f - 0.5f;
    const float halfH = vis.getHeight() * 0.5f - 0.5f;

    if (tile > 0.0f && halfW >= 0.0f && halfH >= 0.0f) {
      const float minX = -halfW * tile;
      const float maxX =  halfW * tile;
      const float minZ = -halfH * tile;
      const float maxZ =  halfH * tile;

      m_position.setX(std::clamp(m_position.x(), minX, maxX));
      m_position.setZ(std::clamp(m_position.z(), minZ, maxZ));

      m_target.setX(std::clamp(m_target.x(), minX, maxX));
      m_target.setZ(std::clamp(m_target.z(), minZ, maxZ));
    }
  }
}

void Camera::computeYawPitchFromOffset(const QVector3D &off, float &yawDeg,
                                       float &pitchDeg) const {
  QVector3D dir = -off;
  if (dir.lengthSquared() < 1e-6f) {
    yawDeg = 0.f; pitchDeg = 0.f; return;
  }
  float yaw   = qRadiansToDegrees(std::atan2(dir.x(), dir.z()));
  float lenXZ = std::sqrt(dir.x() * dir.x() + dir.z() * dir.z());
  float pitch = qRadiansToDegrees(std::atan2(dir.y(), lenXZ));
  yawDeg = yaw;
  pitchDeg = pitch;
}

} // namespace Render::GL
