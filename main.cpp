#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QDebug>
#include <QDir>
#include <QQuickWindow>
#include <QQmlContext>

#include "engine/core/world.h"
#include "engine/core/component.h"
#include "render/gl/renderer.h"
#include "render/gl/camera.h"
#include "game/systems/movement_system.h"
#include "game/systems/combat_system.h"
#include "game/systems/selection_system.h"

class GameEngine : public QObject {
    Q_OBJECT

public:
    GameEngine() {
        // Initialize core systems
        m_world = std::make_unique<Engine::Core::World>();
        m_renderer = std::make_unique<Render::GL::Renderer>();
        m_camera = std::make_unique<Render::GL::Camera>();
        
        // Add game systems
        m_world->addSystem(std::make_unique<Game::Systems::MovementSystem>());
        m_world->addSystem(std::make_unique<Game::Systems::CombatSystem>());
        m_world->addSystem(std::make_unique<Game::Systems::AISystem>());
        
        m_selectionSystem = std::make_unique<Game::Systems::SelectionSystem>();
        m_world->addSystem(std::make_unique<Game::Systems::SelectionSystem>());
        
        setupTestScene();
    }
    Q_INVOKABLE void onMapClicked(qreal sx, qreal sy) {
        if (!m_window) return;
        ensureInitialized();
        // Convert screen coords to world point on ground (y = 0)
        QVector3D hit;
        if (!screenToGround(QPointF(sx, sy), hit)) return;
        // Move our unit to that point
        if (auto* entity = m_world->getEntity(m_playerUnitId)) {
            if (auto* move = entity->getComponent<Engine::Core::MovementComponent>()) {
                move->targetX = hit.x();
                move->targetY = hit.z();
                move->hasTarget = true;
            }
        }
    }
    void setWindow(QQuickWindow* w) { m_window = w; }
    
    void initialize() {
        if (!m_renderer->initialize()) {
            qWarning() << "Failed to initialize renderer";
            return;
        }
        
        m_renderer->setCamera(m_camera.get());
        
        // Set up RTS camera view
        m_camera->setRTSView(QVector3D(0, 0, 0), 15.0f, 45.0f);
        m_camera->setPerspective(45.0f, 16.0f/9.0f, 0.1f, 1000.0f);
        
        m_initialized = true;
        qDebug() << "Game engine initialized successfully";
    }
    
    void ensureInitialized() {
        if (!m_initialized) {
            initialize();
        }
    }
    
    void update(float deltaTime) {
        if (m_world) {
            m_world->update(deltaTime);
        }
    }
    
    void render() {
        if (m_renderer && m_world && m_initialized) {
            if (m_window) {
                m_renderer->setViewport(m_window->width(), m_window->height());
            }
            m_renderer->beginFrame();
            m_renderer->renderWorld(m_world.get());
            m_renderer->endFrame();
        }
    }

private:
    void setupTestScene() {
        // Create a single archer unit at origin
        auto entity = m_world->createEntity();
        m_playerUnitId = entity->getId();

        auto transform = entity->addComponent<Engine::Core::TransformComponent>();
        transform->position = {0.0f, 0.0f, 0.0f};
        transform->scale = {0.5f, 0.5f, 0.5f}; // smaller quad as a unit

    auto renderable = entity->addComponent<Engine::Core::RenderableComponent>("", "");
        renderable->visible = true;

        auto unit = entity->addComponent<Engine::Core::UnitComponent>();
        unit->unitType = "archer";
        unit->health = 80;
        unit->maxHealth = 80;
        unit->speed = 3.0f;

        entity->addComponent<Engine::Core::MovementComponent>();

    // Rotate unit quad to lie on the ground (XZ plane)
    transform->rotation.x = -90.0f;

        qDebug() << "Test scene created with 1 archer (entity" << m_playerUnitId << ")";
    }

    bool screenToGround(const QPointF& screenPt, QVector3D& outWorld) {
        if (!m_window || !m_camera) return false;
        // Viewport
        float w = static_cast<float>(m_window->width());
        float h = static_cast<float>(m_window->height());
        if (w <= 0 || h <= 0) return false;

        // Convert to Normalized Device Coordinates
        float x = (2.0f * static_cast<float>(screenPt.x()) / w) - 1.0f;
        float y = 1.0f - (2.0f * static_cast<float>(screenPt.y()) / h);

    bool ok = false;
    QMatrix4x4 invVP = (m_camera->getProjectionMatrix() * m_camera->getViewMatrix()).inverted(&ok);
    if (!ok) return false;

        // Ray from near to far in world space
        QVector4D nearClip(x, y, 0.0f, 1.0f);
        QVector4D farClip(x, y, 1.0f, 1.0f);
        QVector4D nearWorld4 = invVP * nearClip;
        QVector4D farWorld4 = invVP * farClip;
        if (nearWorld4.w() == 0.0f || farWorld4.w() == 0.0f) return false;
        QVector3D rayOrigin = (nearWorld4 / nearWorld4.w()).toVector3D();
        QVector3D rayEnd = (farWorld4 / farWorld4.w()).toVector3D();
        QVector3D rayDir = (rayEnd - rayOrigin).normalized();

        // Intersect with plane y=0
        if (qFuzzyIsNull(rayDir.y())) return false; // parallel
        float t = -rayOrigin.y() / rayDir.y();
        if (t < 0.0f) return false; // behind camera
        outWorld = rayOrigin + rayDir * t;
        return true;
    }

    std::unique_ptr<Engine::Core::World> m_world;
    std::unique_ptr<Render::GL::Renderer> m_renderer;
    std::unique_ptr<Render::GL::Camera> m_camera;
    std::unique_ptr<Game::Systems::SelectionSystem> m_selectionSystem;
    QQuickWindow* m_window = nullptr;
    Engine::Core::EntityID m_playerUnitId = 0;
    bool m_initialized = false;
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // Set up OpenGL 3.3 Core Profile
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4); // 4x MSAA
    QSurfaceFormat::setDefaultFormat(format);

    // Set up QML engine
    QQmlApplicationEngine engine;

    // Register C++ types with QML if needed
    // qmlRegisterType<GameEngine>("StandardOfIron", 1, 0, "GameEngine");

    // Load QML from the compiled resource path (see generated :/StandardOfIron/ui/qml/*)
    engine.load(QUrl(QStringLiteral("qrc:/StandardOfIron/ui/qml/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        qWarning() << "Failed to load QML file";
        return -1;
    }

    qDebug() << "Application started successfully";
    qDebug() << "Assets directory:" << QDir::currentPath() + "/assets";

    // Get the QQuickWindow from the loaded QML
    QObject* rootObj = engine.rootObjects().first();
    QQuickWindow* window = qobject_cast<QQuickWindow*>(rootObj);
    if (!window) {
        // Try to find a QQuickWindow child
        window = rootObj->findChild<QQuickWindow*>();
    }

    auto gameEngine = new GameEngine();
    // Expose to QML
    engine.rootContext()->setContextProperty("game", gameEngine);

    if (window) {
        window->setColor(Qt::transparent);
        gameEngine->setWindow(window);
        // Per-frame update/render loop (context is current here)
        QObject::connect(window, &QQuickWindow::beforeRendering, gameEngine, [gameEngine, window]() {
            window->beginExternalCommands();
            gameEngine->ensureInitialized();
            gameEngine->update(1.0f / 60.0f); // Fixed timestep for now
            gameEngine->render();
            window->endExternalCommands();
        }, Qt::DirectConnection);

        // Keep camera aspect ratio in sync with window size
        auto updateAspect = [gameEngine, window]() {
            if (!window->height()) return;
            // We access the camera via the renderer setup inside gameEngine.initialize();
            // For simplicity here, re-run ensureInitialized and reset perspective with new aspect.
            gameEngine->ensureInitialized();
            // Compute aspect as width/height
            float aspect = static_cast<float>(window->width()) / static_cast<float>(window->height());
            // Reconfigure camera projection
            // Note: We'd ideally expose a setter on GameEngine, but for a quick prototype,
            // we simply reset via renderer's camera already set inside initialize().
            // m_camera is internal; so we emit a beforeRendering tick soon which uses updated size.
        };
        QObject::connect(window, &QQuickWindow::widthChanged, gameEngine, updateAspect);
        QObject::connect(window, &QQuickWindow::heightChanged, gameEngine, updateAspect);
    // In Qt 6, the default clear before rendering is handled by the scene graph; our renderer also clears per frame.
    } else {
        qWarning() << "No QQuickWindow found for OpenGL initialization.";
    }

    return app.exec();
}

#include "main.moc"