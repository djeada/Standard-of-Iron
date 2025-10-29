#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>
#include <QTextStream>
#include <QUrl>
#include <cstdio>
#include <memory>
#include <qglobal.h>
#include <qguiapplication.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qqml.h>
#include <qqmlapplicationengine.h>
#include <qsgrendererinterface.h>
#include <qstringliteral.h>
#include <qstringview.h>
#include <qsurfaceformat.h>
#include <qurl.h>

#ifdef Q_OS_WIN
#include <QProcess>
#include <gl/gl.h>
#include <windows.h>
#pragma comment(lib, "opengl32.lib")
#endif

#include "app/core/game_engine.h"
#include "app/core/language_manager.h"
#include "ui/gl_view.h"
#include "ui/theme.h"

// Constants replacing magic numbers
constexpr int k_depth_buffer_bits = 24;
constexpr int k_stencil_buffer_bits = 8;

#ifdef Q_OS_WIN
// Test OpenGL using native Win32 API (before any Qt initialization)
// Returns true if OpenGL is available, false otherwise
static bool testNativeOpenGL() {
  WNDCLASSA wc = {};
  wc.lpfnWndProc = DefWindowProcA;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.lpszClassName = "OpenGLTest";

  if (!RegisterClassA(&wc)) {
    return false;
  }

  HWND hwnd = CreateWindowExA(0, "OpenGLTest", "", WS_OVERLAPPEDWINDOW, 0, 0, 1,
                              1, nullptr, nullptr, wc.hInstance, nullptr);
  if (!hwnd) {
    UnregisterClassA("OpenGLTest", wc.hInstance);
    return false;
  }

  HDC hdc = GetDC(hwnd);
  if (!hdc) {
    DestroyWindow(hwnd);
    UnregisterClassA("OpenGLTest", wc.hInstance);
    return false;
  }

  PIXELFORMATDESCRIPTOR pfd = {};
  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 24;
  pfd.cDepthBits = 24;
  pfd.cStencilBits = 8;
  pfd.iLayerType = PFD_MAIN_PLANE;

  int pixelFormat = ChoosePixelFormat(hdc, &pfd);
  bool success = false;

  if (pixelFormat != 0 && SetPixelFormat(hdc, pixelFormat, &pfd)) {
    HGLRC hglrc = wglCreateContext(hdc);
    if (hglrc) {
      if (wglMakeCurrent(hdc, hglrc)) {
        // Successfully created OpenGL context
        const char *vendor = (const char *)glGetString(GL_VENDOR);
        const char *renderer = (const char *)glGetString(GL_RENDERER);
        const char *version = (const char *)glGetString(GL_VERSION);

        if (vendor && renderer && version) {
          fprintf(stderr,
                  "[OpenGL Test] Native context created successfully\n");
          fprintf(stderr, "[OpenGL Test] Vendor: %s\n", vendor);
          fprintf(stderr, "[OpenGL Test] Renderer: %s\n", renderer);
          fprintf(stderr, "[OpenGL Test] Version: %s\n", version);
          success = true;
        }

        wglMakeCurrent(nullptr, nullptr);
      }
      wglDeleteContext(hglrc);
    }
  }

  ReleaseDC(hwnd, hdc);
  DestroyWindow(hwnd);
  UnregisterClassA("OpenGLTest", wc.hInstance);

  return success;
}

// Windows crash handler to detect OpenGL failures and suggest fallback
static bool g_opengl_crashed = false;
static LONG WINAPI crashHandler(EXCEPTION_POINTERS *exceptionInfo) {
  if (exceptionInfo->ExceptionRecord->ExceptionCode ==
      EXCEPTION_ACCESS_VIOLATION) {
    // Log crash
    FILE *crash_log = fopen("opengl_crash.txt", "w");
    if (crash_log) {
      fprintf(crash_log,
              "OpenGL/Qt rendering crash detected (Access Violation)\n");
      fprintf(crash_log, "Try running with: run_debug_softwaregl.cmd\n");
      fprintf(crash_log,
              "Or set environment variable: QT_QUICK_BACKEND=software\n");
      fclose(crash_log);
    }

    qCritical() << "=== CRASH DETECTED ===";
    qCritical() << "OpenGL rendering failed. This usually means:";
    qCritical() << "1. Graphics drivers are outdated";
    qCritical() << "2. Running in a VM with incomplete OpenGL support";
    qCritical() << "3. GPU doesn't support required OpenGL version";
    qCritical() << "";
    qCritical() << "To fix: Run run_debug_softwaregl.cmd instead";
    qCritical() << "Or set: set QT_QUICK_BACKEND=software";

    g_opengl_crashed = true;
  }
  return EXCEPTION_CONTINUE_SEARCH;
}
#endif

auto main(int argc, char *argv[]) -> int {
#ifdef Q_OS_WIN
  // Install crash handler to detect OpenGL failures
  SetUnhandledExceptionFilter(crashHandler);

  // Test OpenGL BEFORE any Qt initialization (using native Win32 API)
  fprintf(stderr, "[Pre-Init] Testing native OpenGL availability...\n");
  bool opengl_available = testNativeOpenGL();

  if (!opengl_available) {
    fprintf(stderr, "[Pre-Init] WARNING: OpenGL test failed!\n");
    fprintf(stderr, "[Pre-Init] Forcing software rendering mode\n");
    _putenv("QT_QUICK_BACKEND=software");
    _putenv("QT_OPENGL=software");
  } else {
    fprintf(stderr, "[Pre-Init] OpenGL test passed\n");
  }

  // Check if we should use software rendering
  bool use_software = qEnvironmentVariableIsSet("QT_QUICK_BACKEND") &&
                      qEnvironmentVariable("QT_QUICK_BACKEND") == "software";

  if (use_software) {
    qInfo() << "=== SOFTWARE RENDERING MODE ===";
    qInfo() << "Using Qt Quick Software renderer (CPU-based)";
    qInfo() << "Performance will be limited but should work on all systems";
  }
#endif

  // Setup message handler for debugging
  qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &context,
                            const QString &msg) {
    QByteArray const local_msg = msg.toLocal8Bit();
    const char *file = (context.file != nullptr) ? context.file : "";
    const char *function =
        (context.function != nullptr) ? context.function : "";

    FILE *out = stderr;
    switch (type) {
    case QtDebugMsg:
      fprintf(out, "[DEBUG] %s (%s:%u, %s)\n", local_msg.constData(), file,
              context.line, function);
      break;
    case QtInfoMsg:
      fprintf(out, "[INFO] %s\n", local_msg.constData());
      break;
    case QtWarningMsg:
      fprintf(out, "[WARNING] %s (%s:%u, %s)\n", local_msg.constData(), file,
              context.line, function);
      // Check for critical OpenGL warnings
      if (msg.contains("OpenGL", Qt::CaseInsensitive) ||
          msg.contains("scene graph", Qt::CaseInsensitive) ||
          msg.contains("RHI", Qt::CaseInsensitive)) {
        fprintf(out, "[HINT] If you see crashes, try software rendering: set "
                     "QT_QUICK_BACKEND=software\n");
      }
      break;
    case QtCriticalMsg:
      fprintf(out, "[CRITICAL] %s (%s:%u, %s)\n", local_msg.constData(), file,
              context.line, function);
      fprintf(
          out,
          "[CRITICAL] Try running with software rendering if this persists\n");
      break;
    case QtFatalMsg:
      fprintf(out, "[FATAL] %s (%s:%u, %s)\n", local_msg.constData(), file,
              context.line, function);
      fprintf(out, "[FATAL] === RECOVERY SUGGESTION ===\n");
      fprintf(out, "[FATAL] Run: run_debug_softwaregl.cmd\n");
      fprintf(out, "[FATAL] Or set: QT_QUICK_BACKEND=software\n");
      abort();
    }
    fflush(out);
  });

  qInfo() << "=== Standard of Iron - Starting ===";
  qInfo() << "Qt version:" << QT_VERSION_STR;

  // Linux-specific: prefer X11 over Wayland for better OpenGL compatibility
#ifndef Q_OS_WIN
  if (qEnvironmentVariableIsSet("WAYLAND_DISPLAY") &&
      qEnvironmentVariableIsSet("DISPLAY")) {
    qputenv("QT_QPA_PLATFORM", "xcb");
    qInfo() << "Linux: Using X11 (xcb) platform";
  }
#endif

  qInfo() << "Setting OpenGL environment...";
  qputenv("QT_OPENGL", "desktop");
  qputenv("QSG_RHI_BACKEND", "opengl");

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  qInfo() << "Setting graphics API to OpenGLRhi...";
  QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGLRhi);
#endif

  qInfo() << "Configuring OpenGL surface format...";
  QSurfaceFormat fmt;
  fmt.setVersion(3, 3);
  fmt.setProfile(QSurfaceFormat::CoreProfile);
  fmt.setDepthBufferSize(k_depth_buffer_bits);
  fmt.setStencilBufferSize(k_stencil_buffer_bits);
  fmt.setSamples(0);

#ifdef Q_OS_WIN
  // Windows: Request compatibility profile for better driver support
  // Some Windows drivers have issues with Core profile on older hardware
  fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
  qInfo() << "Windows detected: Using OpenGL Compatibility Profile";
#endif

  QSurfaceFormat::setDefaultFormat(fmt);
  qInfo() << "Surface format configured: OpenGL" << fmt.majorVersion() << "."
          << fmt.minorVersion();

  qInfo() << "Creating QGuiApplication...";
  QGuiApplication app(argc, argv);
  qInfo() << "QGuiApplication created successfully";

  // Use unique_ptr with custom deleter for Qt objects
  // This ensures proper cleanup order and prevents segfaults
  std::unique_ptr<LanguageManager> language_manager;
  std::unique_ptr<GameEngine> game_engine;
  std::unique_ptr<QQmlApplicationEngine> engine;

  qInfo() << "Creating LanguageManager...";
  language_manager = std::make_unique<LanguageManager>(&app);
  qInfo() << "LanguageManager created";

  qInfo() << "Creating GameEngine...";
  game_engine = std::make_unique<GameEngine>(&app);
  qInfo() << "GameEngine created";

  qInfo() << "Setting up QML engine...";
  engine = std::make_unique<QQmlApplicationEngine>();
  qInfo() << "Adding context properties...";
  engine->rootContext()->setContextProperty("language_manager",
                                            language_manager.get());
  engine->rootContext()->setContextProperty("game", game_engine.get());
  qInfo() << "Adding import path...";
  engine->addImportPath("qrc:/StandardOfIron/ui/qml");
  qInfo() << "Registering QML types...";
  qmlRegisterType<GLView>("StandardOfIron", 1, 0, "GLView");

  // Register Theme singleton
  qmlRegisterSingletonType<Theme>("StandardOfIron.UI", 1, 0, "Theme",
                                  &Theme::create);

  qInfo() << "Loading Main.qml...";
  qInfo() << "Loading Main.qml...";
  engine->load(QUrl(QStringLiteral("qrc:/StandardOfIron/ui/qml/Main.qml")));

  qInfo() << "Checking if QML loaded...";
  if (engine->rootObjects().isEmpty()) {
    qWarning() << "Failed to load QML file";
    return -1;
  }
  qInfo() << "QML loaded successfully, root objects count:"
          << engine->rootObjects().size();

  qInfo() << "Finding QQuickWindow...";
  auto *root_obj = engine->rootObjects().first();
  auto *window = qobject_cast<QQuickWindow *>(root_obj);
  if (window == nullptr) {
    qInfo() << "Root object is not a window, searching children...";
    window = root_obj->findChild<QQuickWindow *>();
  }
  if (window == nullptr) {
    qWarning() << "No QQuickWindow found for OpenGL initialization.";
    return -2;
  }
  qInfo() << "QQuickWindow found";

  qInfo() << "Setting window in GameEngine...";
  game_engine->setWindow(window);
  qInfo() << "Window set successfully";

  qInfo() << "Connecting scene graph signals...";
  qInfo() << "Connecting scene graph signals...";
  QObject::connect(
      window, &QQuickWindow::sceneGraphInitialized, window, [window]() {
        qInfo() << "Scene graph initialized!";
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

  qInfo() << "Starting event loop...";

  int const result = QGuiApplication::exec();

  // Explicitly destroy in correct order to prevent segfault
  qInfo() << "Shutting down...";

  // Destroy QML engine first (destroys OpenGL context)
  engine.reset();
  qInfo() << "QML engine destroyed";

  // Then destroy game engine
  // OpenGL cleanup in destructors will be skipped if no valid context
  game_engine.reset();
  qInfo() << "GameEngine destroyed";

  // Finally destroy language manager
  language_manager.reset();
  qInfo() << "LanguageManager destroyed";

#ifdef Q_OS_WIN
  // Check if we crashed during OpenGL initialization
  if (g_opengl_crashed) {
    qCritical() << "";
    qCritical() << "========================================";
    qCritical() << "OPENGL CRASH RECOVERY";
    qCritical() << "========================================";
    qCritical() << "";
    qCritical() << "The application crashed during OpenGL initialization.";
    qCritical()
        << "This is a known issue with Qt + some Windows graphics drivers.";
    qCritical() << "";
    qCritical() << "SOLUTION: Set environment variable before running:";
    qCritical() << "  set QT_QUICK_BACKEND=software";
    qCritical() << "";
    qCritical() << "Or use the provided launcher:";
    qCritical() << "  run_debug_softwaregl.cmd";
    qCritical() << "";
    return -1;
  }
#endif

  return result;
}
