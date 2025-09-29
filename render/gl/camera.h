#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <QPointF>

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
    // High-level RTS utilities
    void pan(float rightDist, float forwardDist);   // move position & target along right and ground-forward (XZ)
    void elevate(float dy);                         // move position vertically; target unchanged
    void yaw(float degrees);                        // rotate around target (yaw only)
    void orbit(float yawDeg, float pitchDeg);       // rotate around target (yaw & pitch)
    bool screenToGround(float sx, float sy, float screenW, float screenH, QVector3D& outWorld) const; // ray-plane y=0
    bool worldToScreen(const QVector3D& world, int screenW, int screenH, QPointF& outScreen) const;    // to pixels

    // Follow helpers (kept in camera to avoid UI/engine low-level math)
    void setFollowEnabled(bool enable) { m_followEnabled = enable; }
    bool isFollowEnabled() const { return m_followEnabled; }
    void setFollowLerp(float alpha) { m_followLerp = alpha; }
    void setFollowOffset(const QVector3D& off) { m_followOffset = off; }
    void captureFollowOffset() { m_followOffset = m_position - m_target; }
    void updateFollow(const QVector3D& targetCenter); // call each frame with current target center
    
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
    
    // Follow state
    bool m_followEnabled = false;
    QVector3D m_followOffset{0,0,0};
    float m_followLerp = 0.15f;

    // RTS constraints
    float m_groundY = 0.0f;
    float m_minHeight = 0.5f;          // do not go below this height above ground
    // Pitch is measured from the XZ plane; negative means looking down.
    // Clamp to a downward-looking range to keep ground at bottom and avoid flips.
    float m_pitchMinDeg = -85.0f;      // min (most downward) pitch
    float m_pitchMaxDeg = -5.0f;       // max (least downward) pitch

    void updateVectors();
    void clampAboveGround();
    void computeYawPitchFromOffset(const QVector3D& off, float& yawDeg, float& pitchDeg) const;
};

} // namespace Render::GL