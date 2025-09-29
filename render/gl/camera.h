#pragma once

#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {

class Camera {
public:
    Camera();
    
    // View matrix
    void setPosition(const QVector3D& position);
    void setTarget(const QVector3D& target);
    void setUp(const QVector3D& up);
    void lookAt(const QVector3D& position, const QVector3D& target, const QVector3D& up);
    
    // Projection matrix
    void setPerspective(float fov, float aspect, float nearPlane, float farPlane);
    void setOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane);
    
    // Camera movement for RTS-style controls
    void moveForward(float distance);
    void moveRight(float distance);
    void moveUp(float distance);
    void zoom(float delta);
    void rotate(float yaw, float pitch);
    
    // RTS camera presets
    void setRTSView(const QVector3D& center, float distance = 10.0f, float angle = 45.0f);
    void setTopDownView(const QVector3D& center, float distance = 10.0f);
    
    // Matrix getters
    QMatrix4x4 getViewMatrix() const;
    QMatrix4x4 getProjectionMatrix() const;
    QMatrix4x4 getViewProjectionMatrix() const;
    
    // Getters
    const QVector3D& getPosition() const { return m_position; }
    const QVector3D& getTarget() const { return m_target; }
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
    
    // Projection parameters
    bool m_isPerspective = true;
    float m_fov = 45.0f;
    float m_aspect = 16.0f / 9.0f;
    float m_nearPlane = 0.1f;
    float m_farPlane = 1000.0f;
    
    // Orthographic parameters
    float m_orthoLeft = -10.0f;
    float m_orthoRight = 10.0f;
    float m_orthoBottom = -10.0f;
    float m_orthoTop = 10.0f;
    
    void updateVectors();
};

} // namespace Render::GL