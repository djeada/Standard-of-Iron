#include <QCommandLineOption>
#include <QCommandLineParser>
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
#include <QTimer>
#include <QUrl>
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

#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <string_view>

#ifdef Q_OS_WIN
#include <gl/gl.h>
#include <windows.h>
#pragma comment(lib, "opengl32.lib")

#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#endif

#ifndef WGL_CONTEXT_MINOR_VERSION_ARB
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#endif

#ifndef WGL_CONTEXT_PROFILE_MASK_ARB
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#endif

#ifndef WGL_CONTEXT_CORE_PROFILE_BIT_ARB
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif

using PFNWGLCREATECONTEXTATTRIBSARBPROC = HGLRC(WINAPI*)(HDC hDC,
                                                         HGLRC hShareContext,
                                                         const int* attribList);

namespace {

constexpr int k_required_gl_major = 3;
constexpr int k_required_gl_minor = 3;

struct NativeOpenGLProbeResult {
  bool supported = false;
  bool generic_software = false;
  bool used_core_context = false;
  int major = 0;
  int minor = 0;
  QString vendor = QStringLiteral("<unknown>");
  QString renderer = QStringLiteral("<unknown>");
  QString version = QStringLiteral("<unknown>");
};

auto windows_software_requested_from_argv(int argc, char* argv[]) -> bool {
  for (int index = 1; index < argc; ++index) {
    const std::string_view arg =
        argv[index] != nullptr ? std::string_view(argv[index]) : std::string_view();
    if (arg == "-s" || arg == "--force-software" || arg == "--quality=none" ||
        arg == "--quality=software") {
      return true;
    }
    if (arg == "--quality" && index + 1 < argc) {
      const std::string_view value = argv[index + 1] != nullptr
                                         ? std::string_view(argv[index + 1])
                                         : std::string_view();
      if (value == "none" || value == "software") {
        return true;
      }
    }
  }
  return false;
}

auto parse_opengl_version(const char* version, int* major, int* minor) -> bool {
  return version != nullptr && major != nullptr && minor != nullptr &&
         std::sscanf(version, "%d.%d", major, minor) == 2;
}

void capture_current_gl_info(NativeOpenGLProbeResult& result) {
  const auto* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
  const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));

  result.vendor =
      vendor != nullptr ? QString::fromLatin1(vendor) : QStringLiteral("<unknown>");
  result.renderer =
      renderer != nullptr ? QString::fromLatin1(renderer) : QStringLiteral("<unknown>");
  result.version =
      version != nullptr ? QString::fromLatin1(version) : QStringLiteral("<unknown>");
  result.major = 0;
  result.minor = 0;
  if (version != nullptr) {
    (void)parse_opengl_version(version, &result.major, &result.minor);
  }
}

auto opengl_version_supported(int major, int minor) -> bool {
  return major > k_required_gl_major ||
         (major == k_required_gl_major && minor >= k_required_gl_minor);
}

} // namespace
#endif

#include "app/core/game_engine.h"
#include "app/core/language_manager.h"
#include "app/core/user_settings.h"
#include "app/models/graphics_settings_proxy.h"
#include "app/models/map_preview_image_provider.h"
#include "app/models/minimap_image_provider.h"
#include "render/graphics_settings.h"
#include "render/i_render_backend.h"
#if defined(SOI_ENABLE_RUNTIME_TRACING)
#include "render/profiling/profiling_hud.h"
#endif
#include "ui/campaign_map_view.h"
#include "ui/gl_view.h"
#include "ui/theme.h"

constexpr int k_depth_buffer_bits = 24;
constexpr int k_stencil_buffer_bits = 8;

#ifdef Q_OS_WIN

static auto testNativeOpenGL() -> NativeOpenGLProbeResult {
  NativeOpenGLProbeResult result;

  WNDCLASSA wc = {};
  wc.lpfnWndProc = DefWindowProcA;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.lpszClassName = "OpenGLTest";

  if (!RegisterClassA(&wc)) {
    return result;
  }

  HWND hwnd = CreateWindowExA(0,
                              "OpenGLTest",
                              "",
                              WS_OVERLAPPEDWINDOW,
                              0,
                              0,
                              1,
                              1,
                              nullptr,
                              nullptr,
                              wc.hInstance,
                              nullptr);
  if (!hwnd) {
    UnregisterClassA("OpenGLTest", wc.hInstance);
    return result;
  }

  HDC hdc = GetDC(hwnd);
  if (!hdc) {
    DestroyWindow(hwnd);
    UnregisterClassA("OpenGLTest", wc.hInstance);
    return result;
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

  int pixel_format = ChoosePixelFormat(hdc, &pfd);
  if (pixel_format != 0 && SetPixelFormat(hdc, pixel_format, &pfd)) {
    PIXELFORMATDESCRIPTOR chosen_pfd = {};
    if (DescribePixelFormat(hdc, pixel_format, sizeof(chosen_pfd), &chosen_pfd) != 0) {
      result.generic_software = (chosen_pfd.dwFlags & PFD_GENERIC_FORMAT) != 0 &&
                                (chosen_pfd.dwFlags & PFD_GENERIC_ACCELERATED) == 0;
    }

    HGLRC hglrc = wglCreateContext(hdc);
    if (hglrc) {
      if (wglMakeCurrent(hdc, hglrc)) {
        capture_current_gl_info(result);

        auto* create_core_context = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(
            wglGetProcAddress("wglCreateContextAttribsARB"));
        if (create_core_context != nullptr) {
          const int attribs[] = {WGL_CONTEXT_MAJOR_VERSION_ARB,
                                 k_required_gl_major,
                                 WGL_CONTEXT_MINOR_VERSION_ARB,
                                 k_required_gl_minor,
                                 WGL_CONTEXT_PROFILE_MASK_ARB,
                                 WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                                 0};
          HGLRC core_ctx = create_core_context(hdc, nullptr, attribs);
          if (core_ctx != nullptr) {
            wglMakeCurrent(nullptr, nullptr);
            if (wglMakeCurrent(hdc, core_ctx)) {
              result.used_core_context = true;
              capture_current_gl_info(result);
            }
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(core_ctx);
            (void)wglMakeCurrent(hdc, hglrc);
          }
        }

        QByteArray vendor_bytes = result.vendor.toLocal8Bit();
        QByteArray renderer_bytes = result.renderer.toLocal8Bit();
        QByteArray version_bytes = result.version.toLocal8Bit();
        fprintf(stderr, "[OpenGL Test] Native context created successfully\n");
        fprintf(stderr, "[OpenGL Test] Vendor: %s\n", vendor_bytes.constData());
        fprintf(stderr, "[OpenGL Test] Renderer: %s\n", renderer_bytes.constData());
        fprintf(stderr, "[OpenGL Test] Version: %s\n", version_bytes.constData());
        fprintf(stderr,
                "[OpenGL Test] Probe context: %s\n",
                result.used_core_context ? "3.3 core" : "legacy");
        if (result.generic_software) {
          fprintf(stderr, "[OpenGL Test] Pixel format is generic software rendering\n");
        }

        const bool microsoft_gdi =
            result.vendor.contains("Microsoft", Qt::CaseInsensitive) ||
            result.renderer.contains("GDI Generic", Qt::CaseInsensitive);
        const bool version_ok = opengl_version_supported(result.major, result.minor);
        result.supported = version_ok && !result.generic_software && !microsoft_gdi;
        if (!version_ok) {
          fprintf(stderr,
                  "[OpenGL Test] Rejected: requires OpenGL %d.%d Core, found %d.%d\n",
                  k_required_gl_major,
                  k_required_gl_minor,
                  result.major,
                  result.minor);
        }
        if (microsoft_gdi) {
          fprintf(
              stderr,
              "[OpenGL Test] Rejected: Microsoft GDI generic renderer is not usable "
              "for the 3D renderer\n");
        }

        wglMakeCurrent(nullptr, nullptr);
      }
      wglDeleteContext(hglrc);
    }
  }

  ReleaseDC(hwnd, hdc);
  DestroyWindow(hwnd);
  UnregisterClassA("OpenGLTest", wc.hInstance);

  return result;
}

static bool g_opengl_crashed = false;
static LONG WINAPI crashHandler(EXCEPTION_POINTERS* exceptionInfo) {
  if (exceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {

    FILE* crash_log = fopen("opengl_crash.txt", "w");
    if (crash_log) {
      fprintf(crash_log, "OpenGL/Qt rendering crash detected (Access Violation)\n");
      fprintf(crash_log, "Try running with: run_debug_softwaregl.cmd\n");
      fprintf(crash_log, "Or set environment variable: QT_OPENGL=software\n");
      fclose(crash_log);
    }

    qCritical() << "=== CRASH DETECTED ===";
    qCritical() << "OpenGL rendering failed. This usually means:";
    qCritical() << "1. Graphics drivers are outdated";
    qCritical() << "2. Running in a VM with incomplete OpenGL support";
    qCritical() << "3. GPU doesn't support required OpenGL version";
    qCritical() << "";
    qCritical() << "To fix: Run run_debug_softwaregl.cmd instead";
    qCritical() << "Or set: set QT_OPENGL=software";

    g_opengl_crashed = true;
  }
  return EXCEPTION_CONTINUE_SEARCH;
}
#endif

auto main(int argc, char* argv[]) -> int {
  // QQuickFramebufferObject only works with the OpenGL scene graph. A global
  // Qt Quick software backend would leave the menus visible but suppress the
  // gameplay framebuffer, which is indistinguishable from the reported blank
  // game view. Keep Qt Quick on OpenGL; platform software GL remains available.
  if (qEnvironmentVariable("QT_QUICK_BACKEND")
          .compare("software", Qt::CaseInsensitive) == 0) {
    fprintf(stderr,
            "[Pre-Init] QT_QUICK_BACKEND=software is incompatible with the "
            "gameplay framebuffer; selecting the OpenGL scene graph instead\n");
    qunsetenv("QT_QUICK_BACKEND");
#ifdef Q_OS_WIN
    if (!qEnvironmentVariableIsSet("QT_OPENGL")) {
      qputenv("QT_OPENGL", "software");
    }
#endif
  }

#ifdef Q_OS_WIN

  SetUnhandledExceptionFilter(crashHandler);

  if (windows_software_requested_from_argv(argc, argv)) {
    fprintf(stderr, "[Pre-Init] Command line requested software OpenGL fallback\n");
    qputenv("QT_OPENGL", "software");
  }

  const QString requested_qt_opengl =
      qEnvironmentVariable("QT_OPENGL").trimmed().toLower();
  const bool explicit_qt_opengl = !requested_qt_opengl.isEmpty();

  if (!explicit_qt_opengl) {
    fprintf(stderr, "[Pre-Init] Testing native OpenGL availability...\n");
    const auto probe = testNativeOpenGL();
    if (!probe.supported) {
      fprintf(stderr, "[Pre-Init] WARNING: hardware OpenGL probe failed\n");
      fprintf(stderr,
              "[Pre-Init] Falling back to Qt software OpenGL (opengl32sw.dll)\n");
      qputenv("QT_OPENGL", "software");
    } else {
      fprintf(stderr, "[Pre-Init] OpenGL test passed\n");
    }
  } else {
    fprintf(stderr,
            "[Pre-Init] Respecting QT_OPENGL=%s\n",
            requested_qt_opengl.toLocal8Bit().constData());
  }

  if (qEnvironmentVariable("QT_OPENGL").compare("software", Qt::CaseInsensitive) == 0) {
    fprintf(stderr, "[Pre-Init] Software OpenGL fallback enabled\n");
  }
#endif

  qInstallMessageHandler(
      [](QtMsgType type, const QMessageLogContext& context, const QString& msg) {
        QByteArray const local_msg = msg.toLocal8Bit();
        const char* file = (context.file != nullptr) ? context.file : "";
        const char* function = (context.function != nullptr) ? context.function : "";

        FILE* out = stderr;
        switch (type) {
        case QtDebugMsg:
          fprintf(out,
                  "[DEBUG] %s (%s:%u, %s)\n",
                  local_msg.constData(),
                  file,
                  context.line,
                  function);
          break;
        case QtInfoMsg:
          fprintf(out, "[INFO] %s\n", local_msg.constData());
          break;
        case QtWarningMsg:
          fprintf(out,
                  "[WARNING] %s (%s:%u, %s)\n",
                  local_msg.constData(),
                  file,
                  context.line,
                  function);

          if (msg.contains("OpenGL", Qt::CaseInsensitive) ||
              msg.contains("scene graph", Qt::CaseInsensitive) ||
              msg.contains("RHI", Qt::CaseInsensitive)) {
            fprintf(out,
                    "[HINT] If you see crashes, try software rendering: set "
                    "QT_OPENGL=software\n");
          }
          break;
        case QtCriticalMsg:
          fprintf(out,
                  "[CRITICAL] %s (%s:%u, %s)\n",
                  local_msg.constData(),
                  file,
                  context.line,
                  function);
          if (msg.contains("scene graph is not using OpenGL", Qt::CaseInsensitive)) {
            fprintf(out,
                    "[CRITICAL] Do not use QT_QUICK_BACKEND=software; the game "
                    "requires Qt Quick's OpenGL backend\n");
          } else {
            fprintf(out,
                    "[CRITICAL] Try running with software OpenGL if this persists\n");
          }
          break;
        case QtFatalMsg:
          fprintf(out,
                  "[FATAL] %s (%s:%u, %s)\n",
                  local_msg.constData(),
                  file,
                  context.line,
                  function);
          fprintf(out, "[FATAL] === RECOVERY SUGGESTION ===\n");
          fprintf(out, "[FATAL] Run: run_debug_softwaregl.cmd\n");
          fprintf(out, "[FATAL] Or set: QT_OPENGL=software\n");
          abort();
        }
        fflush(out);
      });

  qInfo() << "=== Standard of Iron - Starting ===";
  qInfo() << "Qt version:" << QT_VERSION_STR;

  if (!qEnvironmentVariableIsSet("QML_XHR_ALLOW_FILE_READ")) {
    qputenv("QML_XHR_ALLOW_FILE_READ", "1");
  }

  qInfo() << "Setting OpenGL environment...";

  if (!qEnvironmentVariableIsSet("QT_OPENGL")) {
    qputenv("QT_OPENGL", "desktop");
  }
  qputenv("QSG_RHI_BACKEND", "opengl");
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  qInfo() << "Setting graphics API to OpenGLRhi...";
  QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGLRhi);
#endif

  qInfo() << "Configuring OpenGL surface format...";
  QSurfaceFormat fmt;
  fmt.setRenderableType(QSurfaceFormat::OpenGL);
  fmt.setVersion(3, 3);
  fmt.setProfile(QSurfaceFormat::CoreProfile);
  fmt.setRedBufferSize(8);
  fmt.setGreenBufferSize(8);
  fmt.setBlueBufferSize(8);
  fmt.setAlphaBufferSize(8);
  fmt.setDepthBufferSize(k_depth_buffer_bits);
  fmt.setStencilBufferSize(k_stencil_buffer_bits);
  fmt.setSamples(0);
  fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);

  QSurfaceFormat::setDefaultFormat(fmt);
  qInfo() << "Surface format configured: OpenGL" << fmt.majorVersion() << "."
          << fmt.minorVersion();

  qInfo() << "Creating QGuiApplication...";
  QGuiApplication app(argc, argv);
  qInfo() << "QGuiApplication created successfully";
  const bool renderer_self_test =
      QCoreApplication::arguments().contains(QStringLiteral("--renderer-self-test"));

  App::Core::UserSettings::apply_saved_graphics_quality();

  QString direct_campaign_mission;
#if defined(SOI_ENABLE_RUNTIME_TRACING)
  double runtime_benchmark_seconds = 0.0;
  QString runtime_benchmark_output;
#endif

  {
    QCommandLineParser parser;
    parser.setApplicationDescription("Standard of Iron");
    parser.addHelpOption();
    QCommandLineOption const force_software_opt(
        QStringList{"s", "force-software"},
        "Force the CPU software rendering backend (ShaderQuality::None).");
    QCommandLineOption const quality_opt(
        "quality",
        "Override shader quality: full | reduced | minimal | none.",
        "level");
    QCommandLineOption const renderer_self_test_opt(
        "renderer-self-test",
        "Show the gameplay view, render and present one frame, then exit.");
    QCommandLineOption const graphics_preset_opt(
        "graphics-preset",
        "Override the complete graphics preset: low | medium | high | ultra.",
        "preset");
    QCommandLineOption const campaign_mission_opt(
        "campaign-mission",
        "Start a campaign mission directly (campaign_id/mission_id).",
        "path");
#if defined(SOI_ENABLE_RUNTIME_TRACING)
    QCommandLineOption const benchmark_seconds_opt(
        "benchmark-seconds",
        "Measure the directly started mission after a two-second warm-up, then exit.",
        "seconds");
    QCommandLineOption const benchmark_output_opt(
        "benchmark-output",
        "Write the runtime benchmark JSON report to this path.",
        "path");
#endif
    parser.addOption(force_software_opt);
    parser.addOption(quality_opt);
    parser.addOption(renderer_self_test_opt);
    parser.addOption(graphics_preset_opt);
    parser.addOption(campaign_mission_opt);
#if defined(SOI_ENABLE_RUNTIME_TRACING)
    parser.addOption(benchmark_seconds_opt);
    parser.addOption(benchmark_output_opt);
#endif
    parser.process(app);

    if (parser.isSet(graphics_preset_opt)) {
      const QString preset = parser.value(graphics_preset_opt).trimmed().toLower();
      auto& gfx = Render::GraphicsSettings::instance();
      if (preset == QStringLiteral("low")) {
        gfx.set_quality(Render::GraphicsQuality::Low);
      } else if (preset == QStringLiteral("medium")) {
        gfx.set_quality(Render::GraphicsQuality::Medium);
      } else if (preset == QStringLiteral("high")) {
        gfx.set_quality(Render::GraphicsQuality::High);
      } else if (preset == QStringLiteral("ultra")) {
        gfx.set_quality(Render::GraphicsQuality::Ultra);
      } else {
        qWarning() << "Unknown --graphics-preset value:" << preset;
      }
    }

    direct_campaign_mission = parser.value(campaign_mission_opt).trimmed();
#if defined(SOI_ENABLE_RUNTIME_TRACING)
    bool benchmark_seconds_valid = false;
    runtime_benchmark_seconds =
        parser.value(benchmark_seconds_opt).toDouble(&benchmark_seconds_valid);
    if (!benchmark_seconds_valid || runtime_benchmark_seconds < 0.0) {
      runtime_benchmark_seconds = 0.0;
    }
    runtime_benchmark_output = parser.value(benchmark_output_opt).trimmed();
    if (runtime_benchmark_seconds > 0.0) {
      qputenv("SOI_RUNTIME_BENCHMARK_SECONDS",
              QByteArray::number(runtime_benchmark_seconds, 'f', 3));
      if (!runtime_benchmark_output.isEmpty()) {
        qputenv("SOI_RUNTIME_BENCHMARK_OUTPUT", runtime_benchmark_output.toUtf8());
      }
    }
#endif

    std::optional<Render::ShaderQuality> requested;
    if (parser.isSet(quality_opt)) {
      const QString v = parser.value(quality_opt).trimmed().toLower();
      if (v == "full") {
        requested = Render::ShaderQuality::Full;
      } else if (v == "reduced") {
        requested = Render::ShaderQuality::Reduced;
      } else if (v == "minimal") {
        requested = Render::ShaderQuality::Minimal;
      } else if (v == "none" || v == "software") {
        requested = Render::ShaderQuality::None;
      } else {
        qWarning() << "Unknown --quality value:" << v
                   << "(expected full|reduced|minimal|none)";
      }
    }
    if (parser.isSet(force_software_opt)) {
      requested = Render::ShaderQuality::None;
    }
    if (requested.has_value()) {
      auto& gfx = Render::GraphicsSettings::instance();
      auto features = gfx.features();
      features.shader_quality = *requested;

      switch (*requested) {
      case Render::ShaderQuality::None:
        qInfo() << "[CLI] shader_quality = None (software backend)";
        break;
      case Render::ShaderQuality::Minimal:
        gfx.set_quality(Render::GraphicsQuality::Low);
        qInfo() << "[CLI] shader_quality = Minimal";
        break;
      case Render::ShaderQuality::Reduced:
        gfx.set_quality(Render::GraphicsQuality::Medium);
        qInfo() << "[CLI] shader_quality = Reduced";
        break;
      case Render::ShaderQuality::Full:
        gfx.set_quality(Render::GraphicsQuality::High);
        qInfo() << "[CLI] shader_quality = Full";
        break;
      }

      const_cast<Render::GraphicsFeatures&>(gfx.features()).shader_quality = *requested;
    }
  }

  std::unique_ptr<LanguageManager> language_manager;
  std::unique_ptr<GameEngine> game_engine;
  std::unique_ptr<App::Models::GraphicsSettingsProxy> graphics_settings;
  std::unique_ptr<QQmlApplicationEngine> engine;

  qInfo() << "Creating LanguageManager...";
  language_manager = std::make_unique<LanguageManager>(&app);
  qInfo() << "LanguageManager created";

  qInfo() << "Creating GameEngine...";
  game_engine = std::make_unique<GameEngine>(&app);
  qInfo() << "GameEngine created";

  qInfo() << "Creating GraphicsSettingsProxy...";
  graphics_settings = std::make_unique<App::Models::GraphicsSettingsProxy>(&app);
  qInfo() << "GraphicsSettingsProxy created";

  qInfo() << "Setting up QML engine...";
  engine = std::make_unique<QQmlApplicationEngine>();

  qInfo() << "Registering minimap image provider...";
  auto* minimap_provider = new MinimapImageProvider();
  engine->addImageProvider("minimap", minimap_provider);

  qInfo() << "Registering map preview image provider...";
  auto* map_preview_provider = new MapPreviewImageProvider();
  engine->addImageProvider("mappreview", map_preview_provider);

  qInfo() << "Adding context properties...";
  engine->rootContext()->setContextProperty("language_manager", language_manager.get());
  engine->rootContext()->setContextProperty("game", game_engine.get());
  engine->rootContext()->setContextProperty("map_preview_provider",
                                            map_preview_provider);
  engine->rootContext()->setContextProperty("graphics_settings",
                                            graphics_settings.get());

#if defined(SOI_ENABLE_RUNTIME_TRACING)
  auto profiling_hud = std::make_unique<Render::Profiling::ProfilingHud>();
  engine->rootContext()->setContextProperty("profiling_hud", profiling_hud.get());
#endif

  QObject::connect(
      game_engine.get(),
      &GameEngine::minimap_image_changed,
      &app,
      [minimap_provider, game_engine_ptr = game_engine.get()]() {
        minimap_provider->set_minimap_image(game_engine_ptr->minimap_image());
      },
      Qt::DirectConnection);

  if (!game_engine->minimap_image().isNull()) {
    qInfo() << "Setting initial minimap image";
    minimap_provider->set_minimap_image(game_engine->minimap_image());
  }

  qInfo() << "Adding import path...";
  engine->addImportPath("qrc:/StandardOfIron/ui/qml");
  engine->addImportPath("qrc:/");
  qInfo() << "Registering QML types...";
  qmlRegisterType<GLView>("StandardOfIron", 1, 0, "GLView");
  qmlRegisterType<CampaignMapView>("StandardOfIron", 1, 0, "CampaignMapView");

  qmlRegisterSingletonType<Theme>("StandardOfIron", 1, 0, "Theme", &Theme::create);

  qmlRegisterSingletonType(QUrl("qrc:/StandardOfIron/ui/qml/StyleGuide.qml"),
                           "StandardOfIron",
                           1,
                           0,
                           "StyleGuide");

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

  qInfo() << "Connecting language change handler...";
  QObject::connect(language_manager.get(),
                   &LanguageManager::language_changed,
                   engine.get(),
                   &QQmlApplicationEngine::retranslate);
  qInfo() << "Language change handler connected";

  qInfo() << "Finding QQuickWindow...";
  auto* root_obj = engine->rootObjects().first();
  auto* window = qobject_cast<QQuickWindow*>(root_obj);
  if (window == nullptr) {
    qInfo() << "Root object is not a window, searching children...";
    window = root_obj->findChild<QQuickWindow*>();
  }
  if (window == nullptr) {
    qWarning() << "No QQuickWindow found for OpenGL initialization.";
    return -2;
  }
  qInfo() << "QQuickWindow found";

  qInfo() << "Setting window in GameEngine...";
  game_engine->setWindow(window);
  qInfo() << "Window set successfully";

  if (!direct_campaign_mission.isEmpty()) {
    if (!root_obj->setProperty("game_started", true) ||
        !root_obj->setProperty("menu_visible", false)) {
      qCritical() << "Could not expose GameView for direct campaign mission";
      return 10;
    }
    auto* gl_view = root_obj->findChild<GLView*>();
    if (gl_view == nullptr) {
      qCritical() << "Could not find gameplay GLView for direct campaign mission";
      return 10;
    }
    QObject::connect(
        gl_view,
        &GLView::renderer_ready,
        &app,
        [game_engine_ptr = game_engine.get(), direct_campaign_mission]() {
          qInfo() << "Starting campaign mission directly:"
                  << direct_campaign_mission;
          game_engine_ptr->start_campaign_mission(direct_campaign_mission);
        },
        Qt::QueuedConnection);
    window->show();
    window->update();
  }

  qInfo() << "Connecting scene graph signals...";
  qInfo() << "Connecting scene graph signals...";
  QObject::connect(window,
                   &QQuickWindow::sceneGraphInitialized,
                   window,
                   [window, renderer_self_test, &app]() {
                     qInfo() << "Scene graph initialized!";
                     if (auto* renderer_interface = window->rendererInterface()) {
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
                       if (api != QSGRendererInterface::OpenGLRhi) {
                         qCritical() << "The Qt Quick scene graph is not using OpenGL; "
                                        "the gameplay framebuffer cannot be displayed.";
                         if (renderer_self_test) {
                           app.exit(10);
                         }
                       }
                     }
                   });

  QObject::connect(window,
                   &QQuickWindow::sceneGraphError,
                   &app,
                   [&](QQuickWindow::SceneGraphError, const QString& msg) {
                     qCritical() << "Failed to initialize OpenGL scene graph:" << msg;
                     QGuiApplication::exit(3);
                   });

  if (renderer_self_test) {
    auto* gl_view = root_obj->findChild<GLView*>();
    if (gl_view == nullptr) {
      qCritical() << "SOI_RENDERER_SELF_TEST: FAIL - GLView was not created";
      return 10;
    }

    auto renderer_ready = std::make_shared<bool>(false);
    QObject::connect(
        gl_view, &GLView::renderer_ready, &app, [window, renderer_ready]() {
          *renderer_ready = true;
          window->update();
        });
    QObject::connect(
        window, &QQuickWindow::frameSwapped, &app, [&app, renderer_ready]() {
          if (!*renderer_ready) {
            return;
          }
          qInfo() << "SOI_RENDERER_SELF_TEST: PASS - gameplay OpenGL "
                     "frame rendered and presented";
          app.exit(0);
        });

    if (!root_obj->setProperty("game_started", true) ||
        !root_obj->setProperty("menu_visible", false)) {
      qCritical() << "SOI_RENDERER_SELF_TEST: FAIL - could not expose GameView";
      return 10;
    }
    window->show();
    window->update();

    QTimer::singleShot(30000, &app, [&app]() {
      qCritical() << "SOI_RENDERER_SELF_TEST: FAIL - no gameplay frame was "
                     "presented within 30 seconds";
      app.exit(10);
    });
  }

  qInfo() << "Starting event loop...";

  int const result = QGuiApplication::exec();

  qInfo() << "Shutting down...";

  engine.reset();
  qInfo() << "QML engine destroyed";

  game_engine.reset();
  qInfo() << "GameEngine destroyed";

  language_manager.reset();
  qInfo() << "LanguageManager destroyed";

#ifdef Q_OS_WIN

  if (g_opengl_crashed) {
    qCritical() << "";
    qCritical() << "========================================";
    qCritical() << "OPENGL CRASH RECOVERY";
    qCritical() << "========================================";
    qCritical() << "";
    qCritical() << "The application crashed during OpenGL initialization.";
    qCritical() << "This is a known issue with Qt + some Windows graphics drivers.";
    qCritical() << "";
    qCritical() << "SOLUTION: Set environment variable before running:";
    qCritical() << "  set QT_OPENGL=software";
    qCritical() << "";
    qCritical() << "Or use the provided launcher:";
    qCritical() << "  run_debug_softwaregl.cmd";
    qCritical() << "";
    return -1;
  }
#endif

  return result;
}
