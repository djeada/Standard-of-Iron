#include "camera.h"
#include <QtMath>

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
    m_position += m_up * distance;
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
    // For RTS camera, we typically want to rotate around the target
    QVector3D offset = m_position - m_target;
    
    // Apply yaw rotation (around Y axis)
    QMatrix4x4 yawRotation;
    yawRotation.rotate(yaw, QVector3D(0, 1, 0));
    
    // Apply pitch rotation (around local X axis)
    QVector3D rightAxis = QVector3D::crossProduct(offset, m_up).normalized();
    QMatrix4x4 pitchRotation;
    pitchRotation.rotate(pitch, rightAxis);
    
    // Combine rotations
    offset = pitchRotation.map(yawRotation.map(offset));
    m_position = m_target + offset;
    
    m_front = (m_target - m_position).normalized();
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
    m_right = QVector3D::crossProduct(m_front, m_up).normalized();
    m_up = QVector3D::crossProduct(m_right, m_front).normalized();
}

} // namespace Render::GL