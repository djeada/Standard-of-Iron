#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSettings>
#include <QSurfaceFormat>
#include <QTemporaryDir>
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

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <string_view>

#include "render/gl/context_requirements.h"

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

constexpr int k_required_gl_major = Render::GL::ContextRequirements::required.major;
constexpr int k_required_gl_minor = Render::GL::ContextRequirements::required.minor;

struct NativeOpenGLProbeResult {
  bool supported = false;
  bool generic_software = false;
  bool used_core_context = false;
  int requested_major = 0;
  int requested_minor = 0;
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

#include "app/audio/audio_resource_loader.h"
#include "app/core/game_engine.h"
#include "app/core/game_speed.h"
#include "app/core/language_manager.h"
#include "app/core/user_settings.h"
#include "app/models/graphics_settings_proxy.h"
#include "app/models/loading_tips.h"
#include "app/models/map_preview_image_provider.h"
#include "app/models/minimap_image_provider.h"
#include "app/viewmodels/match_setup_view_model.h"
#include "app/viewmodels/minimap_view_model.h"
#include "render/graphics_settings.h"
#include "render/horse/horse_source_asset.h"
#include "render/i_render_backend.h"
#include "render/profiling/profiling_hud.h"
#include "ui/brand_fonts.h"
#include "ui/campaign_map_view.h"
#include "ui/commander_portrait_view.h"
#include "ui/edge_scroll.h"
#include "ui/game_speeds.h"
#include "ui/gl_view.h"
#include "ui/hints.h"
#include "ui/icon_art.h"
#include "ui/input_bindings.h"
#include "ui/preferences.h"
#include "ui/theme.h"

namespace {

auto validate_release_campaign_map_resources() -> bool {
  constexpr std::array<const char*, 7> resources{
      ":/assets/campaign_map/campaign_base_color.png",
      ":/assets/campaign_map/campaign_water.png",
      ":/assets/campaign_map/coastlines_uv.json",
      ":/assets/campaign_map/rivers_uv.json",
      ":/assets/campaign_map/land_mesh.bin",
      ":/assets/campaign_map/provinces.json",
      ":/assets/campaign_map/terrain_height.png",
  };

  for (const char* path : resources) {
    QFile file(QString::fromLatin1(path));
    if (!file.open(QIODevice::ReadOnly) || file.size() < 32) {
      qCritical() << "SOI_CAMPAIGN_MAP_SELF_TEST: FAIL - missing or empty" << path;
      return false;
    }
  }
  qInfo() << "SOI_CAMPAIGN_MAP_SELF_TEST: PASS - all campaign map resources are "
             "embedded";
  return true;
}

void capture_screenshot_and_exit(QQuickWindow* window,
                                 const QString& path,
                                 const QString& view,
                                 int delay_ms) {

  window->setWindowState(Qt::WindowNoState);
  window->setWidth(1600);
  window->setHeight(900);

  auto grab_and_exit = [window, path]() {
    const QImage frame = window->grabWindow();
    if (frame.isNull()) {
      qCritical() << "SOI_SCREENSHOT: FAIL - the window produced no frame";
      QGuiApplication::exit(11);
      return;
    }
    if (!frame.save(path)) {
      qCritical() << "SOI_SCREENSHOT: FAIL - could not write" << path;
      QGuiApplication::exit(12);
      return;
    }
    qInfo() << "SOI_SCREENSHOT: PASS -" << path << frame.width() << "x"
            << frame.height();
    QGuiApplication::exit(0);
  };

  if (view.isEmpty()) {
    QTimer::singleShot(delay_ms, window, grab_and_exit);
    return;
  }

  auto* settle = new QTimer(window);
  settle->setInterval(400);
  QObject::connect(
      settle, &QTimer::timeout, window, [window, view, settle, delay_ms]() {
        QMetaObject::invokeMethod(window, "show_view", Q_ARG(QVariant, QVariant(view)));
        if (window->property("capture_view_ready").toBool()) {
          settle->stop();
          QTimer::singleShot(delay_ms / 8, window, [window]() {
            window->setProperty("capture_view_settled", true);
          });
        }
      });
  settle->start();

  auto* deadline = new QTimer(window);
  deadline->setInterval(200);
  QObject::connect(
      deadline, &QTimer::timeout, window, [window, settle, deadline, grab_and_exit]() {
        if (!window->property("capture_view_settled").toBool()) {
          return;
        }
        settle->stop();
        deadline->stop();
        grab_and_exit();
      });
  deadline->start();

  QTimer::singleShot(delay_ms, window, [settle, deadline, grab_and_exit]() {
    if (!deadline->isActive()) {
      return;
    }
    settle->stop();
    deadline->stop();
    grab_and_exit();
  });
}

} // namespace

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
          constexpr std::array probe_versions{
              Render::GL::ContextRequirements::preferred,
              Render::GL::ContextRequirements::Version{4, 4},
              Render::GL::ContextRequirements::Version{4, 3},
              Render::GL::ContextRequirements::apple_maximum,
              Render::GL::ContextRequirements::required,
          };
          for (const auto candidate : probe_versions) {
            const int attribs[] = {WGL_CONTEXT_MAJOR_VERSION_ARB,
                                   candidate.major,
                                   WGL_CONTEXT_MINOR_VERSION_ARB,
                                   candidate.minor,
                                   WGL_CONTEXT_PROFILE_MASK_ARB,
                                   WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                                   0};
            HGLRC core_ctx = create_core_context(hdc, nullptr, attribs);
            if (core_ctx == nullptr) {
              continue;
            }
            wglMakeCurrent(nullptr, nullptr);
            if (wglMakeCurrent(hdc, core_ctx)) {
              result.used_core_context = true;
              result.requested_major = candidate.major;
              result.requested_minor = candidate.minor;
              capture_current_gl_info(result);
            }
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(core_ctx);
            (void)wglMakeCurrent(hdc, hglrc);
            if (result.used_core_context) {
              break;
            }
          }
        }

        QByteArray vendor_bytes = result.vendor.toLocal8Bit();
        QByteArray renderer_bytes = result.renderer.toLocal8Bit();
        QByteArray version_bytes = result.version.toLocal8Bit();
        fprintf(stderr, "[OpenGL Test] Native context created successfully\n");
        fprintf(stderr, "[OpenGL Test] Vendor: %s\n", vendor_bytes.constData());
        fprintf(stderr, "[OpenGL Test] Renderer: %s\n", renderer_bytes.constData());
        fprintf(stderr, "[OpenGL Test] Version: %s\n", version_bytes.constData());
        if (result.used_core_context) {
          fprintf(stderr,
                  "[OpenGL Test] Probe context: %d.%d core\n",
                  result.requested_major,
                  result.requested_minor);
        } else {
          fprintf(stderr, "[OpenGL Test] Probe context: legacy\n");
        }
        if (result.generic_software) {
          fprintf(stderr, "[OpenGL Test] Pixel format is generic software rendering\n");
        }

        const bool microsoft_gdi =
            result.vendor.contains("Microsoft", Qt::CaseInsensitive) ||
            result.renderer.contains("GDI Generic", Qt::CaseInsensitive);
        const bool version_ok = result.used_core_context &&
                                opengl_version_supported(result.major, result.minor);
        result.supported = version_ok && !result.generic_software && !microsoft_gdi;
        if (!version_ok) {
          fprintf(stderr,
                  "[OpenGL Test] Rejected: requires OpenGL %d.%d Core, found %d.%d "
                  "%s\n",
                  k_required_gl_major,
                  k_required_gl_minor,
                  result.major,
                  result.minor,
                  result.used_core_context ? "Core" : "without a Core profile");
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

#if defined(Q_OS_MACOS)
  auto surface_gl_version = Render::GL::ContextRequirements::apple_maximum;
#else
  auto surface_gl_version = Render::GL::ContextRequirements::preferred;
#endif

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
      surface_gl_version = {probe.requested_major, probe.requested_minor};
    }
  } else {
    fprintf(stderr,
            "[Pre-Init] Respecting QT_OPENGL=%s\n",
            requested_qt_opengl.toLocal8Bit().constData());
  }

  if (qEnvironmentVariable("QT_OPENGL").compare("software", Qt::CaseInsensitive) == 0) {
    if (!qEnvironmentVariableIsSet("GALLIUM_DRIVER")) {
      qputenv("GALLIUM_DRIVER", "llvmpipe");
    }
    fprintf(stderr, "[Pre-Init] Software OpenGL fallback enabled\n");
    fprintf(stderr,
            "[Pre-Init] Mesa Gallium driver: %s\n",
            qEnvironmentVariable("GALLIUM_DRIVER").toLocal8Bit().constData());
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
          } else if (msg.contains("OpenGL", Qt::CaseInsensitive) ||
                     msg.contains("scene graph", Qt::CaseInsensitive) ||
                     msg.contains("RHI", Qt::CaseInsensitive) ||
                     msg.contains("graphics", Qt::CaseInsensitive)) {
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
  fmt.setVersion(surface_gl_version.major, surface_gl_version.minor);
  fmt.setProfile(QSurfaceFormat::CoreProfile);
  fmt.setRedBufferSize(8);
  fmt.setGreenBufferSize(8);
  fmt.setBlueBufferSize(8);
  fmt.setAlphaBufferSize(8);
  fmt.setDepthBufferSize(k_depth_buffer_bits);
  fmt.setStencilBufferSize(k_stencil_buffer_bits);
  fmt.setSamples(0);
  fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
  if (qEnvironmentVariableIntValue("SOI_GL_DEBUG") != 0) {
    fmt.setOption(QSurfaceFormat::DebugContext);
    qInfo() << "OpenGL debug context requested by SOI_GL_DEBUG";
  }
  if (qEnvironmentVariableIsSet("SOI_SWAP_INTERVAL")) {
    bool interval_ok = false;
    const int interval =
        qEnvironmentVariableIntValue("SOI_SWAP_INTERVAL", &interval_ok);
    if (interval_ok && interval >= 0) {
      fmt.setSwapInterval(interval);
      qInfo() << "Swap interval overridden by SOI_SWAP_INTERVAL:" << interval;
    }
  } else {
    const int interval = App::Core::UserSettings::load_display_vsync() ? 1 : 0;
    fmt.setSwapInterval(interval);
    qInfo() << "Swap interval from saved VSync preference:" << interval;
  }

  QSurfaceFormat::setDefaultFormat(fmt);
  qInfo() << "Surface format configured: preferred OpenGL" << fmt.majorVersion() << "."
          << fmt.minorVersion() << "Core (portable floor 3.3 Core)";

  qInfo() << "Creating QGuiApplication...";
  QGuiApplication app(argc, argv);
  qInfo() << "QGuiApplication created successfully";

  app.setApplicationVersion(QStringLiteral(SOI_VERSION));
  qInfo() << "Game version:" << app.applicationVersion();

  qInfo() << "Bundled fonts:" << Ui::BrandFonts::register_bundled();
  const bool renderer_self_test =
      QCoreApplication::arguments().contains(QStringLiteral("--renderer-self-test"));
  const bool release_self_test =
      QCoreApplication::arguments().contains(QStringLiteral("--release-self-test"));

  std::unique_ptr<QTemporaryDir> release_settings_dir;
  if (release_self_test) {
    release_settings_dir = std::make_unique<QTemporaryDir>();
    if (!release_settings_dir->isValid()) {
      qCritical() << "SOI_GRAPHICS_DEFAULT_SELF_TEST: FAIL - could not create a "
                     "fresh settings profile";
      return 14;
    }
    QSettings::setPath(
        QSettings::IniFormat, QSettings::UserScope, release_settings_dir->path());
  }

  if (renderer_self_test || release_self_test) {
    const QStringList missing_audio = AudioResourceLoader::missing_asset_ids();
    if (!missing_audio.isEmpty()) {
      qCritical() << "SOI_AUDIO_SELF_TEST: FAIL -" << missing_audio.size()
                  << "manifest entries have no file on disk, first:"
                  << missing_audio.first();
      return 11;
    }
    qInfo() << "SOI_AUDIO_SELF_TEST: PASS - every audio manifest entry resolves";
  }

  App::Core::UserSettings::apply_saved_graphics_quality();

  if (release_self_test) {
    if (Render::GraphicsSettings::instance().quality() !=
        Render::k_default_graphics_quality) {
      qCritical() << "SOI_GRAPHICS_DEFAULT_SELF_TEST: FAIL - fresh profile is not "
                     "the default preset";
      return 14;
    }
    qInfo() << "SOI_GRAPHICS_DEFAULT_SELF_TEST: PASS - fresh profile uses the "
               "default preset";
    if (!validate_release_campaign_map_resources()) {
      return 15;
    }
    const auto& horse_status = Render::Horse::horse_source_asset_status();
    if (!horse_status.loaded) {
      qCritical() << "SOI_CREATURE_ASSET_SELF_TEST: FAIL - horse asset:"
                  << horse_status.error.c_str();
      return 16;
    }
    qInfo() << "SOI_CREATURE_ASSET_SELF_TEST: PASS - packaged horse asset loaded";
  }

  QString direct_campaign_mission;
  QString direct_mission_file;
  QString observe_map_file;
  QString record_replay_path;
  QString replay_path;
  bool replay_verify = false;
  bool skip_briefing = false;
  float direct_game_speed = App::Core::GameSpeed::k_default;
  bool component_gallery_requested = false;
  QString screenshot_path;
  QString screenshot_view;
  int screenshot_delay_ms = 0;
  double runtime_benchmark_seconds = 0.0;
  QString runtime_benchmark_output;

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
    QCommandLineOption const release_self_test_opt(
        "release-self-test",
        "Validate a fresh profile and campaign assets, start a real campaign "
        "mission, present frames, then exit.");
    QCommandLineOption const graphics_preset_opt(
        "graphics-preset",
        "Override the complete graphics preset: low | medium | high | ultra.",
        "preset");
    QCommandLineOption const campaign_mission_opt(
        "campaign-mission",
        "Start a campaign mission directly (campaign_id/mission_id).",
        "path");
    QCommandLineOption const mission_file_opt(
        "mission-file",
        "Start a mission definition file directly for editor testing.",
        "path");
    QCommandLineOption const observe_opt(
        "observe",
        "Start this skirmish map with every slot under computer control and watch it "
        "as a spectator.",
        "map-path");
    QCommandLineOption const record_replay_opt(
        "record-replay",
        "Write every command the match accepts to this file, so the match can be "
        "played back with --replay.",
        "path");
    QCommandLineOption const replay_opt(
        "replay",
        "Launch the match a replay file describes and let the file drive it; local "
        "input and the computer opponent are shut out.",
        "path");
    QCommandLineOption const replay_verify_opt(
        "replay-verify",
        "With --replay: exit when the replay has played through, 0 if the "
        "simulation matched every recorded digest, 12 if it diverged.");
    QCommandLineOption const skip_briefing_opt(
        "skip-briefing",
        "Start a directly launched mission unpaused, without the objectives "
        "briefing (for scripted runs).");
    QCommandLineOption const component_gallery_opt(
        "component-gallery",
        "Open the Iron and Ember component gallery instead of the game.");
    QCommandLineOption const screenshot_opt(
        "screenshot", "Render one frame, write a PNG to this path, then exit.", "path");
    QCommandLineOption const screenshot_view_opt(
        "screenshot-view",
        "Surface to capture: menu | skirmish | missions | campaign | settings | load "
        "| save | briefing | hud | rpg | commander | tutorial.",
        "view",
        "menu");
    QCommandLineOption const screenshot_delay_opt(
        "screenshot-delay",
        "Milliseconds to let the surface settle before capturing.",
        "ms",
        "1200");
    QCommandLineOption const game_speed_opt(
        "game-speed",
        "Start a directly launched mission at this battle speed (0.5, 1, 2, 3 or 4).",
        "multiplier");
    QCommandLineOption const benchmark_seconds_opt(
        "benchmark-seconds",
        "Measure the directly started mission after a two-second warm-up, then exit.",
        "seconds");
    QCommandLineOption const benchmark_output_opt(
        "benchmark-output",
        "Write the runtime benchmark JSON report to this path.",
        "path");
    parser.addOption(force_software_opt);
    parser.addOption(quality_opt);
    parser.addOption(renderer_self_test_opt);
    parser.addOption(release_self_test_opt);
    parser.addOption(graphics_preset_opt);
    parser.addOption(campaign_mission_opt);
    parser.addOption(mission_file_opt);
    parser.addOption(observe_opt);
    parser.addOption(record_replay_opt);
    parser.addOption(replay_opt);
    parser.addOption(replay_verify_opt);
    parser.addOption(skip_briefing_opt);
    parser.addOption(game_speed_opt);
    parser.addOption(component_gallery_opt);
    parser.addOption(screenshot_opt);
    parser.addOption(screenshot_view_opt);
    parser.addOption(screenshot_delay_opt);
    parser.addOption(benchmark_seconds_opt);
    parser.addOption(benchmark_output_opt);
    parser.process(app);

    component_gallery_requested = parser.isSet(component_gallery_opt);
    if (parser.isSet(screenshot_opt)) {
      screenshot_path = parser.value(screenshot_opt).trimmed();
      screenshot_view = parser.value(screenshot_view_opt).trimmed().toLower();
      bool delay_ok = false;
      const int parsed_delay = parser.value(screenshot_delay_opt).toInt(&delay_ok);
      screenshot_delay_ms = (delay_ok && parsed_delay >= 0) ? parsed_delay : 1200;
    }

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
    direct_mission_file = parser.value(mission_file_opt).trimmed();
    observe_map_file = parser.value(observe_opt).trimmed();
    record_replay_path = parser.value(record_replay_opt).trimmed();
    replay_path = parser.value(replay_opt).trimmed();
    replay_verify = parser.isSet(replay_verify_opt);
    skip_briefing =
        parser.isSet(skip_briefing_opt) || replay_verify || !observe_map_file.isEmpty();
    if (parser.isSet(game_speed_opt)) {
      bool speed_ok = false;
      const float requested = parser.value(game_speed_opt).toFloat(&speed_ok);
      if (!speed_ok) {
        qWarning() << "Ignoring unreadable --game-speed value:"
                   << parser.value(game_speed_opt);
      } else {
        direct_game_speed = App::Core::GameSpeed::sanitize(requested);
        if (!qFuzzyCompare(direct_game_speed, requested)) {
          qWarning() << "--game-speed" << requested << "is not offered; using"
                     << direct_game_speed;
        }
      }
    }
    if (release_self_test) {

      direct_campaign_mission.clear();
      direct_mission_file =
          QStringLiteral(":/assets/missions/iron_sepulcher_watch.json");
    }

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

      gfx.set_backend_kind(*requested);
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
  game_engine->set_release_self_test_mode(release_self_test);
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

  auto profiling_hud = std::make_unique<Render::Profiling::ProfilingHud>();
  engine->rootContext()->setContextProperty("profiling_hud", profiling_hud.get());

  auto* minimap_view_model = qobject_cast<App::ViewModels::MinimapViewModel*>(
      game_engine->minimap_view_model());
  QObject::connect(
      minimap_view_model,
      &App::ViewModels::MinimapViewModel::image_changed,
      &app,
      [minimap_provider, minimap_view_model]() {
        minimap_provider->set_minimap_image(minimap_view_model->image());
      },
      Qt::DirectConnection);

  if (!minimap_view_model->image().isNull()) {
    qInfo() << "Setting initial minimap image";
    minimap_provider->set_minimap_image(minimap_view_model->image());
  }

  qInfo() << "Adding import path...";
  engine->addImportPath("qrc:/StandardOfIron/ui/qml");
  engine->addImportPath("qrc:/");
  qInfo() << "Registering QML types...";

  qmlRegisterSingletonType<LoadingTips>(
      "StandardOfIron", 1, 0, "LoadingTips", &LoadingTips::create);

  qmlRegisterSingletonType(QUrl("qrc:/StandardOfIron/ui/qml/StyleGuide.qml"),
                           "StandardOfIron",
                           1,
                           0,
                           "StyleGuide");

  qmlRegisterSingletonType(QUrl("qrc:/StandardOfIron/ui/qml/EconomyGuide.qml"),
                           "StandardOfIron",
                           1,
                           0,
                           "EconomyGuide");

  const QUrl root_qml =
      component_gallery_requested
          ? QUrl(QStringLiteral("qrc:/StandardOfIron/Design/GalleryWindow.qml"))
          : QUrl(QStringLiteral("qrc:/StandardOfIron/ui/qml/Main.qml"));
  qInfo() << "Loading" << root_qml;
  engine->load(root_qml);

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

  QObject::connect(language_manager.get(),
                   &LanguageManager::language_changed,
                   Theme::instance(),
                   &Theme::player_colors_changed);
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

  if (component_gallery_requested) {

    if (!screenshot_path.isEmpty()) {
      capture_screenshot_and_exit(
          window, screenshot_path, QString(), screenshot_delay_ms);
    }
    qInfo() << "Starting event loop (component gallery)...";
    const int gallery_result = QGuiApplication::exec();
    engine.reset();
    game_engine.reset();
    language_manager.reset();
    return gallery_result;
  }

  qInfo() << "Setting window in GameEngine...";
  game_engine->setWindow(window);
  qInfo() << "Window set successfully";

  if (!record_replay_path.isEmpty()) {
    game_engine->set_replay_record_path(record_replay_path);
  }
  if (replay_verify) {
    game_engine->set_replay_verify_exit(true);
  }

  if (!direct_campaign_mission.isEmpty() || !direct_mission_file.isEmpty() ||
      !observe_map_file.isEmpty() || !replay_path.isEmpty()) {

    QTimer::singleShot(
        0,
        &app,
        [root_obj,
         &app,
         game_engine_ptr = game_engine.get(),
         direct_campaign_mission,
         direct_mission_file,
         observe_map_file,
         replay_path,
         skip_briefing,
         direct_game_speed] {
          auto* gl_view = root_obj->findChild<GLView*>();
          if (gl_view == nullptr) {
            qCritical() << "Could not find gameplay GLView for direct campaign mission";
            QCoreApplication::exit(10);
            return;
          }
          auto mission_started = std::make_shared<bool>(false);
          auto start_direct_mission = [game_engine_ptr,
                                       direct_campaign_mission,
                                       direct_mission_file,
                                       observe_map_file,
                                       replay_path,
                                       mission_started,
                                       direct_game_speed]() {
            if (*mission_started) {
              return;
            }
            *mission_started = true;
            game_engine_ptr->set_game_speed(direct_game_speed);
            if (!replay_path.isEmpty()) {
              qInfo() << "Playing replay:" << replay_path;
              if (!game_engine_ptr->start_replay(replay_path)) {
                qCritical() << "Replay could not be started:" << replay_path;
                QCoreApplication::exit(11);
              }
            } else if (!observe_map_file.isEmpty()) {
              qInfo() << "Observing a computer-only skirmish on:" << observe_map_file;
              if (!game_engine_ptr->match_setup()->start_observed_skirmish(
                      observe_map_file)) {
                qCritical() << "Observed skirmish could not be started:"
                            << observe_map_file;
                QCoreApplication::exit(13);
              }
            } else if (!direct_mission_file.isEmpty()) {
              qInfo() << "Starting mission file directly:" << direct_mission_file;
              game_engine_ptr->match_setup()->start_mission_file(direct_mission_file);
            } else {
              qInfo() << "Starting campaign mission directly:"
                      << direct_campaign_mission;
              game_engine_ptr->match_setup()->start_campaign_mission(
                  direct_campaign_mission);
            }
          };

          QObject::connect(game_engine_ptr,
                           &GameEngine::renderer_initialized_changed,
                           &app,
                           start_direct_mission,
                           Qt::QueuedConnection);
          QObject::connect(gl_view,
                           &GLView::renderer_ready,
                           &app,
                           start_direct_mission,
                           Qt::QueuedConnection);
          if (skip_briefing) {

            root_obj->setProperty("suppress_modals", true);
          }
          if (!root_obj->setProperty("game_started", true) ||
              !root_obj->setProperty("menu_visible", false)) {
            qCritical() << "Could not expose GameView for direct campaign mission";
            QCoreApplication::exit(10);
            return;
          }
          if (game_engine_ptr->renderer_initialized() || gl_view->is_renderer_ready()) {
            qInfo() << "Gameplay renderer was ready during direct mission setup";
            QTimer::singleShot(0, &app, start_direct_mission);
          }
        });
    window->show();
    window->update();
  }

  qInfo() << "Connecting scene graph signals...";
  qInfo() << "Connecting scene graph signals...";
  QObject::connect(window,
                   &QQuickWindow::sceneGraphInitialized,
                   window,
                   [window, renderer_self_test, release_self_test]() {
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
                         if (renderer_self_test || release_self_test) {
                           QGuiApplication::exit(10);
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

    auto self_test_settled = std::make_shared<bool>(false);
    QObject::connect(
        gl_view, &GLView::renderer_ready, &app, [window, renderer_ready]() {
          *renderer_ready = true;
          window->update();
        });
    QObject::connect(window,
                     &QQuickWindow::frameSwapped,
                     &app,
                     [renderer_ready, self_test_settled]() {
                       if (!*renderer_ready || *self_test_settled) {
                         return;
                       }
                       *self_test_settled = true;
                       qInfo() << "SOI_RENDERER_SELF_TEST: PASS - gameplay OpenGL "
                                  "frame rendered and presented";
                       QGuiApplication::exit(0);
                     });

    if (!root_obj->setProperty("game_started", true) ||
        !root_obj->setProperty("menu_visible", false)) {
      qCritical() << "SOI_RENDERER_SELF_TEST: FAIL - could not expose GameView";
      return 10;
    }
    window->show();
    window->update();

    QTimer::singleShot(30000, &app, [self_test_settled]() {
      if (*self_test_settled) {
        return;
      }
      *self_test_settled = true;
      qCritical() << "SOI_RENDERER_SELF_TEST: FAIL - no gameplay frame was "
                     "presented within 30 seconds";
      QGuiApplication::exit(10);
    });
  }

  if (release_self_test) {
    auto mission_ready = std::make_shared<bool>(false);
    auto presented_frames = std::make_shared<int>(0);

    auto release_test_settled = std::make_shared<bool>(false);
    auto polls = std::make_shared<int>(0);
    auto* readiness_poll = new QTimer(&app);
    readiness_poll->setInterval(250);
    QObject::connect(readiness_poll,
                     &QTimer::timeout,
                     &app,
                     [game_engine_ptr = game_engine.get(),
                      mission_ready,
                      polls,
                      window,
                      readiness_poll,
                      release_test_settled]() {
                       if (!game_engine_ptr->last_error().isEmpty()) {
                         qCritical() << "SOI_MISSION_SELF_TEST: FAIL -"
                                     << game_engine_ptr->last_error();
                         readiness_poll->stop();
                         *release_test_settled = true;
                         QGuiApplication::exit(17);
                         return;
                       }

                       window->update();
                       if (!game_engine_ptr->release_self_test_mission_ready()) {

                         if (++*polls % 40 == 0) {
                           qInfo().noquote()
                               << "SOI_MISSION_SELF_TEST: waiting -"
                               << game_engine_ptr->release_self_test_pending_reason();
                         }
                         return;
                       }
                       if (!*mission_ready) {
                         *mission_ready = true;
                         qInfo() << "SOI_MISSION_SELF_TEST: mission loaded; verifying "
                                    "presented gameplay frames";
                       }
                     });
    readiness_poll->start();

    QObject::connect(window,
                     &QQuickWindow::frameSwapped,
                     &app,
                     [mission_ready, presented_frames, window, release_test_settled]() {
                       if (!*mission_ready) {
                         return;
                       }
                       ++*presented_frames;
                       if (*presented_frames < 3) {
                         window->update();
                         return;
                       }

                       if (*presented_frames > 3) {
                         return;
                       }
                       *release_test_settled = true;
                       qInfo() << "SOI_MISSION_SELF_TEST: PASS - authored packaged "
                                  "mission loaded with entities";
                       qInfo()
                           << "SOI_RENDERER_SELF_TEST: PASS - three gameplay frames "
                              "rendered and presented after mission load";
                       QGuiApplication::exit(0);
                     });

    QTimer::singleShot(
        1500000,
        &app,
        [game_engine_ptr = game_engine.get(),
         mission_ready,
         presented_frames,
         release_test_settled]() {
          if (*release_test_settled) {
            return;
          }
          *release_test_settled = true;
          qCritical().noquote()
              << "SOI_MISSION_SELF_TEST: FAIL - mission did not load and present "
                 "frames within 1500 seconds; pending:"
              << (*mission_ready
                      ? QStringLiteral("mission ready, only %1 of 3 frames presented")
                            .arg(*presented_frames)
                      : game_engine_ptr->release_self_test_pending_reason());
          QGuiApplication::exit(17);
        });
  }

  if (!screenshot_path.isEmpty()) {
    capture_screenshot_and_exit(
        window, screenshot_path, screenshot_view, screenshot_delay_ms);
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
