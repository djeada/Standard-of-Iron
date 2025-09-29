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
} }

namespace Game { namespace Systems { class SelectionSystem; } }

class QQuickWindow;

class GameEngine : public QObject {
    Q_OBJECT
public:
    GameEngine();
    ~GameEngine();

    Q_INVOKABLE void onMapClicked(qreal sx, qreal sy);

    void setWindow(QQuickWindow* w) { m_window = w; }

    // Render-thread friendly calls (must be invoked when a valid GL context is current)
    void ensureInitialized();
    void update(float dt);
    void render(int pixelWidth, int pixelHeight);

private:
    void initialize();
    void setupTestScene();
    bool screenToGround(const QPointF& screenPt, QVector3D& outWorld);

    std::unique_ptr<Engine::Core::World> m_world;
    std::unique_ptr<Render::GL::Renderer> m_renderer;
    std::unique_ptr<Render::GL::Camera>   m_camera;
    std::unique_ptr<Game::Systems::SelectionSystem> m_selectionSystem;
    QQuickWindow* m_window = nullptr;
    Engine::Core::EntityID m_playerUnitId = 0;
    bool m_initialized = false;
};
