#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QDebug>
#include <QDir>

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
    
    void initialize() {
        if (!m_renderer->initialize()) {
            qWarning() << "Failed to initialize renderer";
            return;
        }
        
        m_renderer->setCamera(m_camera.get());
        
        // Set up RTS camera view
        m_camera->setRTSView(QVector3D(0, 0, 0), 15.0f, 45.0f);
        m_camera->setPerspective(45.0f, 16.0f/9.0f, 0.1f, 1000.0f);
        
        qDebug() << "Game engine initialized successfully";
    }
    
    void update(float deltaTime) {
        if (m_world) {
            m_world->update(deltaTime);
        }
    }
    
    void render() {
        if (m_renderer && m_world) {
            m_renderer->beginFrame();
            m_renderer->renderWorld(m_world.get());
            m_renderer->endFrame();
        }
    }

private:
    void setupTestScene() {
        // Create some test units
        for (int i = 0; i < 5; ++i) {
            auto entity = m_world->createEntity();
            
            // Add transform component
            auto transform = entity->addComponent<Engine::Core::TransformComponent>();
            transform->position.x = i * 2.0f;
            transform->position.y = 0.0f;
            transform->position.z = 0.0f;
            
            // Add renderable component
            auto renderable = entity->addComponent<Engine::Core::RenderableComponent>("", "");
            renderable->visible = true;
            
            // Add unit component
            auto unit = entity->addComponent<Engine::Core::UnitComponent>();
            unit->unitType = "warrior";
            unit->health = 100;
            unit->maxHealth = 100;
            unit->speed = 2.0f;
            
            // Add movement component
            entity->addComponent<Engine::Core::MovementComponent>();
        }
        
        qDebug() << "Test scene created with 5 units";
    }

    std::unique_ptr<Engine::Core::World> m_world;
    std::unique_ptr<Render::GL::Renderer> m_renderer;
    std::unique_ptr<Render::GL::Camera> m_camera;
    std::unique_ptr<Game::Systems::SelectionSystem> m_selectionSystem;
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
    
    // Verify OpenGL context
    QOpenGLContext context;
    if (!context.create()) {
        qFatal("Cannot create OpenGL context");
    }
    
    qDebug() << "OpenGL Version:" << format.majorVersion() << "." << format.minorVersion();
    qDebug() << "OpenGL Profile:" << (format.profile() == QSurfaceFormat::CoreProfile ? "Core" : "Compatibility");
    
    // Initialize game engine
    GameEngine gameEngine;
    gameEngine.initialize();
    
    // Set up QML engine
    QQmlApplicationEngine engine;
    
    // Register C++ types with QML if needed
    // qmlRegisterType<GameEngine>("StandardOfIron", 1, 0, "GameEngine");
    
    const QUrl url(QStringLiteral("qrc:/ui/qml/Main.qml"));
    
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);
    
    // Load QML
    engine.load(url);
    
    if (engine.rootObjects().isEmpty()) {
        qWarning() << "Failed to load QML file";
        return -1;
    }
    
    qDebug() << "Application started successfully";
    qDebug() << "Assets directory:" << QDir::currentPath() + "/assets";
    
    return app.exec();
}

#include "main.moc"