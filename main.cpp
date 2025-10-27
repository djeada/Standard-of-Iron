#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QOpenGLContext>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>
#include <QTextStream>
#include <QUrl>
#include <qglobal.h>
#include <qguiapplication.h>
#include <qobject.h>
#include <qqml.h>
#include <qqmlapplicationengine.h>
#include <qsgrendererinterface.h>
#include <qstringliteral.h>
#include <qsurfaceformat.h>
#include <qurl.h>

#include "app/core/game_engine.h"
#include "app/core/language_manager.h"
#include "ui/gl_view.h"
#include "ui/theme.h"

// Constants replacing magic numbers
constexpr int k_depth_buffer_bits = 24;
constexpr int k_stencil_buffer_bits = 8;

auto main(int argc, char *argv[]) -> int {
  if (qEnvironmentVariableIsSet("WAYLAND_DISPLAY") &&
      qEnvironmentVariableIsSet("DISPLAY")) {
    qputenv("QT_QPA_PLATFORM", "xcb");
  }
  qputenv("QT_OPENGL", "desktop");
  qputenv("QSG_RHI_BACKEND", "opengl");

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGLRhi);
#endif

  QSurfaceFormat fmt;
  fmt.setVersion(3, 3);
  fmt.setProfile(QSurfaceFormat::CoreProfile);
  fmt.setDepthBufferSize(k_depth_buffer_bits);
  fmt.setStencilBufferSize(k_stencil_buffer_bits);
  fmt.setSamples(0);
  QSurfaceFormat::setDefaultFormat(fmt);

  QGuiApplication app(argc, argv);

  auto *language_manager = new LanguageManager();
  auto *game_engine = new GameEngine();

  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("language_manager",
                                           language_manager);
  engine.rootContext()->setContextProperty("game", game_engine);
  engine.addImportPath("qrc:/StandardOfIron/ui/qml");
  qmlRegisterType<GLView>("StandardOfIron", 1, 0, "GLView");

  // Register Theme singleton
  qmlRegisterSingletonType<Theme>("StandardOfIron.UI", 1, 0, "Theme",
                                  &Theme::create);

  engine.load(QUrl(QStringLiteral("qrc:/StandardOfIron/ui/qml/Main.qml")));
  if (engine.rootObjects().isEmpty()) {
    qWarning() << "Failed to load QML file";
    return -1;
  }

  auto *root_obj = engine.rootObjects().first();
  auto *window = qobject_cast<QQuickWindow *>(root_obj);
  if (window == nullptr) {
    window = root_obj->findChild<QQuickWindow *>();
  }
  if (window == nullptr) {
    qWarning() << "No QQuickWindow found for OpenGL initialization.";
    return -2;
  }

  game_engine->setWindow(window);

  QObject::connect(
      window, &QQuickWindow::sceneGraphInitialized, window, [window]() {
        if (auto *renderer_interface = window->rendererInterface()) {
          const auto api = renderer_interface->graphicsApi();

          QString name;
          switch (api) {
          case QSGRendererInterface::OpenGLRhi:
            name = "OpenGLRhi";
            break;
          case QSGRendererInterface::VulkanRhi:
            name = "VulkanRhi";
            break;
          case QSGRendererInterface::Direct3D11Rhi:
            name = "D3D11Rhi";
            break;
          case QSGRendererInterface::MetalRhi:
            name = "MetalRhi";
            break;
          case QSGRendererInterface::Software:
            name = "Software";
            break;
          default:
            name = "Unknown";
            break;
          }

          qInfo() << "QSG graphicsApi:" << name;
        }
      });

  QObject::connect(window, &QQuickWindow::sceneGraphError, &app,
                   [&](QQuickWindow::SceneGraphError, const QString &msg) {
                     qCritical()
                         << "Failed to initialize OpenGL scene graph:" << msg;
                     QGuiApplication::exit(3);
                   });

  return QGuiApplication::exec();
}
