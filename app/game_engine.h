#pragma once

#include <QObject>
#include <QVector3D>
#include <QMatrix4x4>
#include <QPointF>
#include <memory>
#include <algorithm>
#include <QVariant>
#include <vector>

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

namespace Game { namespace Systems { class SelectionSystem; class ArrowSystem; class PickingService; } }

class QQuickWindow;

class GameEngine : public QObject {
    Q_OBJECT
public:
    GameEngine();
    ~GameEngine();

    Q_PROPERTY(QObject* selectedUnitsModel READ selectedUnitsModel NOTIFY selectedUnitsChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused)
    Q_PROPERTY(float timeScale READ timeScale WRITE setGameSpeed)

    Q_INVOKABLE void onMapClicked(qreal sx, qreal sy);
    Q_INVOKABLE void onRightClick(qreal sx, qreal sy);
    Q_INVOKABLE void onClickSelect(qreal sx, qreal sy, bool additive = false);
    Q_INVOKABLE void onAreaSelected(qreal x1, qreal y1, qreal x2, qreal y2, bool additive = false);
    Q_INVOKABLE void setHoverAtScreen(qreal sx, qreal sy);

    // Camera controls exposed to QML
    Q_INVOKABLE void cameraMove(float dx, float dz);      // move along ground plane (right/forward XZ)
    Q_INVOKABLE void cameraElevate(float dy);             // move up/down in Y
    Q_INVOKABLE void cameraYaw(float degrees);            // rotate around current target (yaw)
    Q_INVOKABLE void cameraOrbit(float yawDeg, float pitchDeg); // orbit around target by yaw/pitch
    Q_INVOKABLE void cameraFollowSelection(bool enable);  // follow the currently selected troops
    Q_INVOKABLE void cameraSetFollowLerp(float alpha);    // 0..1, 1 = snap to center

    // Game loop control
    Q_INVOKABLE void setPaused(bool paused) { m_runtime.paused = paused; }
    Q_INVOKABLE void setGameSpeed(float speed) { m_runtime.timeScale = std::max(0.0f, speed); }
    bool paused() const { return m_runtime.paused; }
    float timeScale() const { return m_runtime.timeScale; }

    // Selection queries and actions (for HUD/production)
    Q_INVOKABLE bool hasSelectedType(const QString& type) const;
    Q_INVOKABLE void recruitNearSelected(const QString& unitType);
    Q_INVOKABLE QVariantMap getSelectedProductionState() const; // {hasBarracks, inProgress, timeRemaining, buildTime, producedCount, maxUnits}
    Q_INVOKABLE void setRallyAtScreen(qreal sx, qreal sy);

    void setWindow(QQuickWindow* w) { m_window = w; }

    // Render-thread friendly calls (must be invoked when a valid GL context is current)
    void ensureInitialized();
    void update(float dt);
    void render(int pixelWidth, int pixelHeight);

    // Lightweight data accessors for UI/view models (avoid exposing raw pointers)
    void getSelectedUnitIds(std::vector<Engine::Core::EntityID>& out) const;
    bool getUnitInfo(Engine::Core::EntityID id, QString& name, int& health, int& maxHealth,
                     bool& isBuilding, bool& alive) const;

private:
    // Small state groups to keep engine fields tidy
    struct RuntimeState { bool initialized = false; bool paused = false; float timeScale = 1.0f; int localOwnerId = 1; };
    struct ViewportState { int width = 0; int height = 0; };
    struct LevelState { QString mapName; Engine::Core::EntityID playerUnitId = 0; float camFov = 45.0f; float camNear = 0.1f; float camFar = 1000.0f; };
    struct HoverState { Engine::Core::EntityID buildingId = 0; };

    Game::Systems::ArrowSystem* m_arrowSystem = nullptr; // owned by world
    void initialize();
    bool screenToGround(const QPointF& screenPt, QVector3D& outWorld);
    bool worldToScreen(const QVector3D& world, QPointF& outScreen) const;
    void syncSelectionFlags();
    QObject* selectedUnitsModel();

    std::unique_ptr<Engine::Core::World> m_world;
    std::unique_ptr<Render::GL::Renderer> m_renderer;
    std::unique_ptr<Render::GL::Camera>   m_camera;
    std::shared_ptr<Render::GL::ResourceManager> m_resources;
    std::unique_ptr<Game::Systems::SelectionSystem> m_selectionSystem;
    std::unique_ptr<Game::Systems::PickingService> m_pickingService;
    QQuickWindow* m_window = nullptr;
    RuntimeState m_runtime;
    ViewportState m_viewport;
    // Follow behavior
    bool m_followSelectionEnabled = false;
    // Visual config placeholder removed; visuals are driven by render registry/services
    // Level state
    LevelState m_level;
    QObject* m_selectedUnitsModel = nullptr;
    // Hover state for subtle outline
    HoverState m_hover;
    // Grace window now handled inside PickingService
signals:
    void selectedUnitsChanged();

};
