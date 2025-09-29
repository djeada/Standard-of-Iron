#include "camera.h"
#include <QtMath>
#include <cmath>

namespace Render::GL {

Camera::Camera() {
    updateVectors();
}

void Camera::setPosition(const QVector3D& position) {
    m_position = position;
}

void Camera::setTarget(const QVector3D& target) {
    m_target = target;
    m_front = (m_target - m_position).normalized();
    updateVectors();
}

void Camera::setUp(const QVector3D& up) {
    m_up = up.normalized();
    updateVectors();
}

void Camera::lookAt(const QVector3D& position, const QVector3D& target, const QVector3D& up) {
    m_position = position;
    m_target = target;
    m_up = up.normalized();
    m_front = (m_target - m_position).normalized();
    updateVectors();
}

void Camera::setPerspective(float fov, float aspect, float nearPlane, float farPlane) {
    m_isPerspective = true;
    m_fov = fov;
    m_aspect = aspect;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
}

void Camera::setOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane) {
    m_isPerspective = false;
    m_orthoLeft = left;
    m_orthoRight = right;
    m_orthoBottom = bottom;
    m_orthoTop = top;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;
}

void Camera::moveForward(float distance) {
    m_position += m_front * distance;
    m_target = m_position + m_front;
}

void Camera::moveRight(float distance) {
    m_position += m_right * distance;
    m_target = m_position + m_front;
}

void Camera::moveUp(float distance) {
    // Elevate along world up to avoid roll artifacts
    m_position += QVector3D(0,1,0) * distance;
    clampAboveGround();
    m_target = m_position + m_front;
}

void Camera::zoom(float delta) {
    if (m_isPerspective) {
        m_fov = qBound(1.0f, m_fov - delta, 89.0f);
    } else {
        float scale = 1.0f + delta * 0.1f;
        m_orthoLeft *= scale;
        m_orthoRight *= scale;
        m_orthoBottom *= scale;
        m_orthoTop *= scale;
    }
}

void Camera::rotate(float yaw, float pitch) {
    // Delegate to orbit with clamping/world-up behavior
    orbit(yaw, pitch);
}

void Camera::pan(float rightDist, float forwardDist) {
    // Move along camera right and ground-projected front
    QVector3D right = m_right;
    QVector3D front = m_front; front.setY(0.0f); if (front.lengthSquared()>0) front.normalize();
    QVector3D delta = right * rightDist + front * forwardDist;
    m_position += delta;
    m_target   += delta;
    clampAboveGround();
}

void Camera::elevate(float dy) {
    m_position.setY(m_position.y() + dy);
    clampAboveGround();
}

void Camera::yaw(float degrees) {
    // Pure yaw: keep current pitch and radius exactly
    QVector3D offset = m_position - m_target;
    float curYaw=0.f, curPitch=0.f;
    computeYawPitchFromOffset(offset, curYaw, curPitch);
    orbit(degrees, 0.0f);
}

void Camera::orbit(float yawDeg, float pitchDeg) {
    // Rotate around target with pitch clamped and world-up locked
    QVector3D offset = m_position - m_target;
    if (offset.lengthSquared() < 1e-6f) {
        // Nudge back if somehow at target
        offset = QVector3D(0, 0, 1);
    }
    float curYaw=0.f, curPitch=0.f;
    computeYawPitchFromOffset(offset, curYaw, curPitch);
    float newYaw = curYaw + yawDeg;
    float newPitch = qBound(m_pitchMinDeg, curPitch + pitchDeg, m_pitchMaxDeg);
    float r = offset.length();
    float yawRad = qDegreesToRadians(newYaw);
    float pitchRad = qDegreesToRadians(newPitch);
    QVector3D newDir(
        std::sin(yawRad) * std::cos(pitchRad),
        std::sin(pitchRad),
        std::cos(yawRad) * std::cos(pitchRad)
    );
    m_position = m_target - newDir.normalized() * r;
    clampAboveGround();
    m_front = (m_target - m_position).normalized();
    updateVectors();
}

bool Camera::screenToGround(float sx, float sy, float screenW, float screenH, QVector3D& outWorld) const {
    if (screenW <= 0 || screenH <= 0) return false;
    float x = (2.0f * sx / screenW) - 1.0f;
    float y = 1.0f - (2.0f * sy / screenH);
    bool ok=false;
    QMatrix4x4 invVP = (getProjectionMatrix() * getViewMatrix()).inverted(&ok);
    if (!ok) return false;
    QVector4D nearClip(x, y, 0.0f, 1.0f);
    QVector4D farClip (x, y, 1.0f, 1.0f);
    QVector4D nearWorld4 = invVP * nearClip;
    QVector4D farWorld4  = invVP * farClip;
    if (nearWorld4.w() == 0.0f || farWorld4.w() == 0.0f) return false;
    QVector3D rayOrigin = (nearWorld4 / nearWorld4.w()).toVector3D();
    QVector3D rayEnd    = (farWorld4  / farWorld4.w()).toVector3D();
    QVector3D rayDir    = (rayEnd - rayOrigin).normalized();
    if (qFuzzyIsNull(rayDir.y())) return false;
    // Intersect with plane y = m_groundY
    float t = (m_groundY - rayOrigin.y()) / rayDir.y();
    if (t < 0.0f) return false;
    outWorld = rayOrigin + rayDir * t;
    return true;
}

bool Camera::worldToScreen(const QVector3D& world, int screenW, int screenH, QPointF& outScreen) const {
    if (screenW <= 0 || screenH <= 0) return false;
    QVector4D clip = getProjectionMatrix() * getViewMatrix() * QVector4D(world, 1.0f);
    if (clip.w() == 0.0f) return false;
    QVector3D ndc = (clip / clip.w()).toVector3D();
    if (ndc.z() < -1.0f || ndc.z() > 1.0f) return false;
    float sx = (ndc.x() * 0.5f + 0.5f) * float(screenW);
    float sy = (1.0f - (ndc.y() * 0.5f + 0.5f)) * float(screenH);
    outScreen = QPointF(sx, sy);
    return true;
}

void Camera::updateFollow(const QVector3D& targetCenter) {
    if (!m_followEnabled) return;
    if (m_followOffset.lengthSquared() < 1e-5f) {
        // Initialize offset from current camera state
        m_followOffset = m_position - m_target;
    }
    QVector3D desiredPos = targetCenter + m_followOffset;
    QVector3D newPos = (m_followLerp >= 0.999f) ? desiredPos
                        : (m_position + (desiredPos - m_position) * m_followLerp);
    m_target = targetCenter;
    m_position = newPos;
    // Always look at the target while following
    m_front = (m_target - m_position).normalized();
    clampAboveGround();
    updateVectors();
}

void Camera::setRTSView(const QVector3D& center, float distance, float angle) {
    m_target = center;
    
    // Calculate position based on angle and distance
    float radians = qDegreesToRadians(angle);
    m_position = center + QVector3D(0, distance * qSin(radians), distance * qCos(radians));
    
    m_up = QVector3D(0, 1, 0);
    m_front = (m_target - m_position).normalized();
    updateVectors();
}

void Camera::setTopDownView(const QVector3D& center, float distance) {
    m_target = center;
    m_position = center + QVector3D(0, distance, 0);
    m_up = QVector3D(0, 0, -1);
    m_front = (m_target - m_position).normalized();
    updateVectors();
}

QMatrix4x4 Camera::getViewMatrix() const {
    QMatrix4x4 view;
    view.lookAt(m_position, m_target, m_up);
    return view;
}

QMatrix4x4 Camera::getProjectionMatrix() const {
    QMatrix4x4 projection;
    
    if (m_isPerspective) {
        projection.perspective(m_fov, m_aspect, m_nearPlane, m_farPlane);
    } else {
        projection.ortho(m_orthoLeft, m_orthoRight, m_orthoBottom, m_orthoTop, m_nearPlane, m_farPlane);
    }
    
    return projection;
}

QMatrix4x4 Camera::getViewProjectionMatrix() const {
    return getProjectionMatrix() * getViewMatrix();
}

void Camera::updateVectors() {
    // Keep camera upright: lock roll by using world up for building right vector
    QVector3D worldUp(0,1,0);
    // If front is parallel to worldUp, skip to a safe right
    QVector3D r = QVector3D::crossProduct(m_front, worldUp);
    if (r.lengthSquared() < 1e-6f) r = QVector3D(1,0,0);
    m_right = r.normalized();
    m_up = QVector3D::crossProduct(m_right, m_front).normalized();
}

void Camera::clampAboveGround() {
    if (m_position.y() < m_groundY + m_minHeight) {
        m_position.setY(m_groundY + m_minHeight);
    }
}

void Camera::computeYawPitchFromOffset(const QVector3D& off, float& yawDeg, float& pitchDeg) const {
    QVector3D dir = -off; // look direction from position to target
    if (dir.lengthSquared() < 1e-6f) {
        yawDeg = 0.f; pitchDeg = 0.f; return;
    }
    float yaw = qRadiansToDegrees(std::atan2(dir.x(), dir.z()));
    float lenXZ = std::sqrt(dir.x()*dir.x() + dir.z()*dir.z());
    float pitch = qRadiansToDegrees(std::atan2(dir.y(), lenXZ));
    yawDeg = yaw;
    pitchDeg = pitch;
}

} // namespace Render::GL