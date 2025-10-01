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

    Q_INVOKABLE void cameraMove(float dx, float dz);
    Q_INVOKABLE void cameraElevate(float dy);
    Q_INVOKABLE void cameraYaw(float degrees);
    Q_INVOKABLE void cameraOrbit(float yawDeg, float pitchDeg);
    Q_INVOKABLE void cameraFollowSelection(bool enable);
    Q_INVOKABLE void cameraSetFollowLerp(float alpha);

    Q_INVOKABLE void setPaused(bool paused) { m_runtime.paused = paused; }
    Q_INVOKABLE void setGameSpeed(float speed) { m_runtime.timeScale = std::max(0.0f, speed); }
    bool paused() const { return m_runtime.paused; }
    float timeScale() const { return m_runtime.timeScale; }

    Q_INVOKABLE bool hasSelectedType(const QString& type) const;
    Q_INVOKABLE void recruitNearSelected(const QString& unitType);
    Q_INVOKABLE QVariantMap getSelectedProductionState() const;
    Q_INVOKABLE void setRallyAtScreen(qreal sx, qreal sy);

    void setWindow(QQuickWindow* w) { m_window = w; }

    void ensureInitialized();
    void update(float dt);
    void render(int pixelWidth, int pixelHeight);

    void getSelectedUnitIds(std::vector<Engine::Core::EntityID>& out) const;
    bool getUnitInfo(Engine::Core::EntityID id, QString& name, int& health, int& maxHealth,
                     bool& isBuilding, bool& alive) const;

private:
    struct RuntimeState { bool initialized = false; bool paused = false; float timeScale = 1.0f; int localOwnerId = 1; };
    struct ViewportState { int width = 0; int height = 0; };
    struct LevelState { QString mapName; Engine::Core::EntityID playerUnitId = 0; float camFov = 45.0f; float camNear = 0.1f; float camFar = 1000.0f; };
    struct HoverState { Engine::Core::EntityID buildingId = 0; };

    Game::Systems::ArrowSystem* m_arrowSystem = nullptr;
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
    bool m_followSelectionEnabled = false;
    LevelState m_level;
    QObject* m_selectedUnitsModel = nullptr;
    HoverState m_hover;
signals:
    void selectedUnitsChanged();
};
