#pragma once

#include <QObject>
#include <QVector3D>
#include <QMatrix4x4>
#include <QPointF>
#include <memory>

namespace Engine { namespace Core {
class World;
using EntityID = unsigned int;
struct MovementComponent;
struct TransformComponent;
struct RenderableComponent;
} }

namespace Render { namespace GL {
class Renderer;
class Camera;
class ResourceManager;
} }

namespace Game { namespace Systems { class SelectionSystem; class ArrowSystem; } }

class QQuickWindow;

class GameEngine : public QObject {
    Q_OBJECT
public:
    GameEngine();
    ~GameEngine();

    Q_PROPERTY(QObject* selectedUnitsModel READ selectedUnitsModel NOTIFY selectedUnitsChanged)

    Q_INVOKABLE void onMapClicked(qreal sx, qreal sy);
    Q_INVOKABLE void onRightClick(qreal sx, qreal sy);
    Q_INVOKABLE void onClickSelect(qreal sx, qreal sy, bool additive = false);
    Q_INVOKABLE void onAreaSelected(qreal x1, qreal y1, qreal x2, qreal y2, bool additive = false);

    // Camera controls exposed to QML
    Q_INVOKABLE void cameraMove(float dx, float dz);      // move along ground plane (right/forward XZ)
    Q_INVOKABLE void cameraElevate(float dy);             // move up/down in Y
    Q_INVOKABLE void cameraYaw(float degrees);            // rotate around current target (yaw)
    Q_INVOKABLE void cameraOrbit(float yawDeg, float pitchDeg); // orbit around target by yaw/pitch
    Q_INVOKABLE void cameraFollowSelection(bool enable);  // follow the currently selected troops
    Q_INVOKABLE void cameraSetFollowLerp(float alpha);    // 0..1, 1 = snap to center

    void setWindow(QQuickWindow* w) { m_window = w; }

    // Render-thread friendly calls (must be invoked when a valid GL context is current)
    void ensureInitialized();
    void update(float dt);
    void render(int pixelWidth, int pixelHeight);

private:
    Game::Systems::ArrowSystem* m_arrowSystem = nullptr; // owned by world
    void initialize();
    void setupFallbackTestUnit();
    bool screenToGround(const QPointF& screenPt, QVector3D& outWorld);
    bool worldToScreen(const QVector3D& world, QPointF& outScreen) const;
    void clearAllSelections();
    void syncSelectionFlags();
    QObject* selectedUnitsModel();

    std::unique_ptr<Engine::Core::World> m_world;
    std::unique_ptr<Render::GL::Renderer> m_renderer;
    std::unique_ptr<Render::GL::Camera>   m_camera;
    std::shared_ptr<Render::GL::ResourceManager> m_resources;
    std::unique_ptr<Game::Systems::SelectionSystem> m_selectionSystem;
    QQuickWindow* m_window = nullptr;
    Engine::Core::EntityID m_playerUnitId = 0;
    bool m_initialized = false;
    int m_viewW = 0;
    int m_viewH = 0;
    // Follow behavior
    bool m_followSelectionEnabled = false;
    QVector3D m_followOffset{0,0,0}; // position - target when follow enabled
    float m_followLerp = 0.15f;      // smoothing factor [0..1]
    // Map state
    QString m_loadedMapName;
    // Visual config (temporary; later load from assets)
    struct UnitVisual { QVector3D color{0.8f,0.9f,1.0f}; };
    UnitVisual m_visualArcher;
    // Cached perspective parameters from map or defaults
    float m_camFov = 45.0f;
    float m_camNear = 0.1f;
    float m_camFar = 1000.0f;
    QObject* m_selectedUnitsModel = nullptr;
    int m_localOwnerId = 1; // local player's owner/team id
signals:
    void selectedUnitsChanged();

};
