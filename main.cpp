#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QOpenGLContext>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>

#include "app/game_engine.h"
#include "ui/gl_view.h"

int main(int argc, char *argv[]) {
  if (qEnvironmentVariableIsSet("WAYLAND_DISPLAY") &&
      qEnvironmentVariableIsSet("DISPLAY")) {
    qputenv("QT_QPA_PLATFORM", "xcb");
  }
  qputenv("QT_OPENGL", "desktop");
  qputenv("QSG_RHI_BACKEND", "opengl");
  QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGLRhi);

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
  engine.rootContext()->setContextProperty("game", gameEngine);
  qmlRegisterType<GLView>("StandardOfIron", 1, 0, "GLView");
  engine.load(QUrl(QStringLiteral("qrc:/StandardOfIron/ui/qml/Main.qml")));
  if (engine.rootObjects().isEmpty()) {
    qWarning() << "Failed to load QML file";
    return -1;
  }

  QObject *rootObj = engine.rootObjects().first();
  QQuickWindow *window = qobject_cast<QQuickWindow *>(rootObj);
  if (!window)
    window = rootObj->findChild<QQuickWindow *>();
  if (!window) {
    qWarning() << "No QQuickWindow found for OpenGL initialization.";
    return -2;
  }

  gameEngine->setWindow(window);

  QObject::connect(
      window, &QQuickWindow::sceneGraphInitialized, window, [window]() {
        if (auto *ri = window->rendererInterface()) {
          auto api = ri->graphicsApi();
          const char *name =
              api == QSGRendererInterface::OpenGLRhi       ? "OpenGLRhi"
              : api == QSGRendererInterface::VulkanRhi     ? "VulkanRhi"
              : api == QSGRendererInterface::Direct3D11Rhi ? "D3D11Rhi"
              : api == QSGRendererInterface::MetalRhi      ? "MetalRhi"
              : api == QSGRendererInterface::Software      ? "Software"
                                                           : "Unknown";
          qInfo() << "QSG graphicsApi:" << name;
        }
      });

  QObject::connect(window, &QQuickWindow::sceneGraphError, &app,
                   [&](QQuickWindow::SceneGraphError, const QString &msg) {
                     qCritical()
                         << "Failed to initialize OpenGL scene graph:" << msg;
                     app.exit(3);
                   });

  qDebug() << "Application started successfully";
  qDebug() << "Assets directory:" << QDir::currentPath() + "/assets";

  return app.exec();
}
