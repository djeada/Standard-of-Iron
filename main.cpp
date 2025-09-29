// System/Qt headers
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QDebug>
#include <QDir>
#include <QQuickWindow>
#include <QQmlContext>
#include <QSGRendererInterface>

// App headers
#include "app/game_engine.h"
#include "ui/gl_view.h"

int main(int argc, char *argv[])
{
    // Force desktop OpenGL + GLX path BEFORE any window is created.
    if (qEnvironmentVariableIsSet("WAYLAND_DISPLAY") && qEnvironmentVariableIsSet("DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", "xcb");   // prefer XCB/GLX over Wayland/EGL when XWayland is present
    }
    qputenv("QT_OPENGL", "desktop");         // desktop GL, not GLES/EGL
    qputenv("QSG_RHI_BACKEND", "opengl");    // OpenGL RHI
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGLRhi);

    // Request desktop GL 3.3 core.
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setSamples(0);
    QSurfaceFormat::setDefaultFormat(fmt);

    QGuiApplication app(argc, argv);

    auto gameEngine = new GameEngine();

    QQmlApplicationEngine engine;
    // Expose to QML BEFORE loading (safer if QML binds early).
    engine.rootContext()->setContextProperty("game", gameEngine);
    // Register our GLView item so QML can embed the GL scene inside the scene graph
    qmlRegisterType<GLView>("StandardOfIron", 1, 0, "GLView");
    engine.load(QUrl(QStringLiteral("qrc:/StandardOfIron/ui/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qWarning() << "Failed to load QML file";
        return -1;
    }

    QObject* rootObj = engine.rootObjects().first();
    QQuickWindow* window = qobject_cast<QQuickWindow*>(rootObj);
    if (!window) window = rootObj->findChild<QQuickWindow*>();
    if (!window) {
        qWarning() << "No QQuickWindow found for OpenGL initialization.";
        return -2;
    }

    // Let Qt Quick manage the clear; keep window color default.

    gameEngine->setWindow(window);

    // Informative logging (no current-context check here).
    QObject::connect(window, &QQuickWindow::sceneGraphInitialized, window, [window]() {
        if (auto *ri = window->rendererInterface()) {
            auto api = ri->graphicsApi();
            const char* name = api == QSGRendererInterface::OpenGLRhi     ? "OpenGLRhi"  :
                               api == QSGRendererInterface::VulkanRhi      ? "VulkanRhi"  :
                               api == QSGRendererInterface::Direct3D11Rhi  ? "D3D11Rhi"   :
                               api == QSGRendererInterface::MetalRhi       ? "MetalRhi"   :
                               api == QSGRendererInterface::Software       ? "Software"   : "Unknown";
            qInfo() << "QSG graphicsApi:" << name;
        }
    });

    QObject::connect(window, &QQuickWindow::sceneGraphError, &app,
                     [&](QQuickWindow::SceneGraphError, const QString &msg){
        qCritical() << "Failed to initialize OpenGL scene graph:" << msg;
        app.exit(3);
    });


    qDebug() << "Application started successfully";
    qDebug() << "Assets directory:" << QDir::currentPath() + "/assets";

    return app.exec();
}

// no Q_OBJECT in this TU anymore

