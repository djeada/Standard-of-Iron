#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
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
#include <QSize>
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

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <string_view>

#include "app/audio/audio_resource_loader.h"
#include "app/audio/audio_status_hud.h"
#include "app/bootstrap/command_line.h"
#include "app/bootstrap/data_paths.h"
#include "app/bootstrap/log_handler.h"
#include "app/bootstrap/screenshot_capture.h"
#include "app/bootstrap/self_test_driver.h"
#include "app/bootstrap/startup_self_test.h"
#include "app/bootstrap/windows_gl_probe.h"
#include "app/core/app_identity.h"
#include "app/core/benchmark_action_fixture.h"
#include "app/core/film_action_dispatch.h"
#include "app/core/film_recorder.h"
#include "app/core/game_engine.h"
#include "app/core/game_speed.h"
#include "app/core/language_manager.h"
#include "app/core/user_settings.h"
#include "app/models/graphics_settings_proxy.h"
#include "app/models/loading_tips.h"
#include "app/models/map_preview_image_provider.h"
#include "app/models/minimap_image_provider.h"
#include "app/viewmodels/camera_view_model.h"
#include "app/viewmodels/match_setup_view_model.h"
#include "app/viewmodels/minimap_view_model.h"
#include "app/viewmodels/orders_view_model.h"
#include "app/viewmodels/production_view_model.h"
#include "game/core/presentation_coverage.h"
#include "game/systems/persistence/save_load_service.h"
#include "render/gl/context_requirements.h"
#include "render/graphics_settings.h"
#include "render/horse/horse_source_asset.h"
#include "render/i_render_backend.h"
#include "render/profiling/frame_swap_clock.h"
#include "render/profiling/presentation_cycle.h"
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

constexpr int k_depth_buffer_bits = 24;
constexpr int k_stencil_buffer_bits = 8;

auto main(int argc, char* argv[]) -> int {

  if (App::Bootstrap::data_paths_requested_from_argv(argc, argv)) {
    return App::Bootstrap::print_data_paths(argc, argv);
  }

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

  App::Bootstrap::install_opengl_crash_handler();

  if (App::Bootstrap::software_requested_from_argv(argc, argv)) {
    fprintf(stderr, "[Pre-Init] Command line requested software OpenGL fallback\n");
    qputenv("QT_OPENGL", "software");
  }

  const QString requested_qt_opengl =
      qEnvironmentVariable("QT_OPENGL").trimmed().toLower();
  const bool explicit_qt_opengl = !requested_qt_opengl.isEmpty();

  if (!explicit_qt_opengl) {
    fprintf(stderr, "[Pre-Init] Testing native OpenGL availability...\n");
    const auto probe = App::Bootstrap::test_native_opengl();
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

  App::Bootstrap::install_log_message_handler();

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

  App::Core::apply_application_identity();
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
    if (const int code = App::Bootstrap::check_audio_manifest(); code >= 0) {
      return code;
    }
  }

  App::Core::UserSettings::apply_saved_graphics_quality();

  if (release_self_test) {
    if (const int code = App::Bootstrap::check_release_defaults(); code >= 0) {
      return code;
    }
  }

  App::Bootstrap::CommandLineOptions opts;
  if (const int exit_code =
          App::Bootstrap::parse_command_line(app, release_self_test, opts);
      exit_code >= 0) {
    return exit_code;
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

  auto audio_hud = std::make_unique<App::Audio::AudioStatusHud>();
  engine->rootContext()->setContextProperty("audio_hud", audio_hud.get());

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

  qmlRegisterSingletonType(QUrl("qrc:/StandardOfIron/ui/qml/DifficultyCatalog.qml"),
                           "StandardOfIron",
                           1,
                           0,
                           "DifficultyCatalog");

  qmlRegisterSingletonType(QUrl("qrc:/StandardOfIron/ui/qml/ScenarioChallenge.qml"),
                           "StandardOfIron",
                           1,
                           0,
                           "ScenarioChallenge");

  const QUrl root_qml =
      opts.component_gallery_requested
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
  if (qEnvironmentVariable("SOI_RUNTIME_BENCHMARK_SECONDS").toDouble() > 0.0) {
    QObject::connect(
        window,
        &QQuickWindow::frameSwapped,
        window,
        [] {
          const auto now = std::chrono::steady_clock::now().time_since_epoch();
          Render::Profiling::frame_swap_clock().observe(
              std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
        },
        Qt::DirectConnection);
  }

  if (opts.component_gallery_requested) {

    if (!opts.screenshot_path.isEmpty()) {
      App::Bootstrap::capture_screenshot_and_exit(window,
                                                  opts.screenshot_path,
                                                  QString(),
                                                  opts.screenshot_delay_ms,
                                                  opts.screenshot_size);
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

  if (!opts.record_replay_path.isEmpty()) {
    game_engine->set_replay_record_path(opts.record_replay_path);
  }
  if (opts.replay_verify) {
    game_engine->set_replay_verify_exit(true);
  }

  if (!opts.direct_campaign_mission.isEmpty() || !opts.direct_mission_file.isEmpty() ||
      !opts.observe_map_file.isEmpty() || !opts.replay_path.isEmpty()) {

    QTimer::singleShot(
        0, &app, [root_obj, &app, game_engine_ptr = game_engine.get(), opts] {
          auto* gl_view = root_obj->findChild<GLView*>();
          if (gl_view == nullptr) {
            qCritical() << "Could not find gameplay GLView for direct campaign mission";
            QCoreApplication::exit(10);
            return;
          }
          auto mission_started = std::make_shared<bool>(false);
          auto start_direct_mission = [game_engine_ptr, opts, mission_started]() {
            if (*mission_started) {
              return;
            }
            *mission_started = true;
            if (!opts.replay_path.isEmpty()) {
              qInfo() << "Playing replay:" << opts.replay_path;
              if (!game_engine_ptr->start_replay(opts.replay_path)) {
                qCritical() << "Replay could not be started:" << opts.replay_path;
                QCoreApplication::exit(11);
              }
            } else if (!opts.observe_map_file.isEmpty()) {
              qInfo() << "Observing a computer-only skirmish on:"
                      << opts.observe_map_file;
              if (!game_engine_ptr->match_setup()->start_observed_skirmish(
                      opts.observe_map_file)) {
                qCritical() << "Observed skirmish could not be started:"
                            << opts.observe_map_file;
                QCoreApplication::exit(13);
              }
            } else if (!opts.direct_mission_file.isEmpty()) {
              qInfo() << "Starting mission file directly:" << opts.direct_mission_file;
              game_engine_ptr->match_setup()->start_mission_file(
                  opts.direct_mission_file);
            } else {
              qInfo() << "Starting campaign mission directly:"
                      << opts.direct_campaign_mission;
              game_engine_ptr->match_setup()->start_campaign_mission(
                  opts.direct_campaign_mission);
            }
            game_engine_ptr->set_game_speed(opts.direct_game_speed);
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
          if (opts.skip_briefing) {

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
    if (const int code =
            App::Bootstrap::start_renderer_self_test(app, root_obj, window);
        code >= 0) {
      return code;
    }
  }

  if (release_self_test) {
    App::Bootstrap::start_release_self_test(app, window, game_engine.get());
  }

  if (!opts.screenshot_path.isEmpty()) {
    App::Bootstrap::capture_screenshot_and_exit(window,
                                                opts.screenshot_path,
                                                opts.screenshot_view,
                                                opts.screenshot_delay_ms,
                                                opts.screenshot_size);
  }

  if (opts.runtime_benchmark_seconds > 0.0 &&
      qEnvironmentVariableIntValue("SOI_BENCHMARK_CAMERA_CYCLE") != 0) {
    auto* cycle_timer = new QTimer(game_engine.get());
    auto elapsed = std::make_shared<QElapsedTimer>();
    auto previous = std::make_shared<Render::Profiling::PresentationCyclePosition>();
    auto origin = std::make_shared<QVector3D>();
    QObject::connect(
        cycle_timer,
        &QTimer::timeout,
        game_engine.get(),
        [game_ptr = game_engine.get(), elapsed, previous, origin]() {
          auto& progress = Render::Profiling::presentation_cycle_progress();
          if (game_ptr->is_loading() || !game_ptr->simulation_thread_running()) {
            elapsed->invalidate();
            *previous = {};
            progress.updates = 0;
            progress.completed_cycles = 0;
            return;
          }
          auto* camera = static_cast<App::ViewModels::CameraViewModel*>(
              game_ptr->camera_view_model());
          if (!elapsed->isValid()) {
            *origin = camera->world_target();
            elapsed->start();
          }
          const double seconds = static_cast<double>(elapsed->elapsed()) / 1000.0;
          const auto position = Render::Profiling::presentation_cycle_position(seconds);
          camera->look_at_world(origin->x() + static_cast<float>(position.x),
                                origin->z() + static_cast<float>(position.z));
          camera->zoom(static_cast<float>(position.zoom - previous->zoom));
          *previous = position;
          ++progress.updates;
          progress.completed_cycles = static_cast<std::uint64_t>(seconds / 20.0);
        });
    cycle_timer->start(16);
  }

  if (opts.runtime_benchmark_seconds > 0.0 && opts.runtime_action_fixture.has_value()) {
    auto* action_timer = new QTimer(game_engine.get());
    auto elapsed = std::make_shared<QElapsedTimer>();
    auto previous_seconds = std::make_shared<double>(0.0);
    auto fixture = std::make_shared<App::Core::BenchmarkActionFixture>(
        *opts.runtime_action_fixture);
    QObject::connect(
        action_timer,
        &QTimer::timeout,
        game_engine.get(),
        [game_ptr = game_engine.get(), window, elapsed, previous_seconds, fixture]() {
          if (game_ptr->is_loading() || !game_ptr->simulation_thread_running()) {
            elapsed->invalidate();
            *previous_seconds = 0.0;
            return;
          }
          if (!elapsed->isValid()) {
            elapsed->start();
            *previous_seconds = 0.0;
            return;
          }
          const double seconds = static_cast<double>(elapsed->elapsed()) / 1000.0;
          const auto due =
              App::Core::actions_between(*fixture, *previous_seconds, seconds);
          *previous_seconds = seconds;
          for (const auto& action : due) {
            App::Core::apply_benchmark_action(game_ptr, window, action);
            Render::Profiling::presentation_cycle_progress().actions_executed.fetch_add(
                1);
          }
        });
    action_timer->start(16);
  }

  std::unique_ptr<App::Core::FilmRecorder> film_recorder;
  if (opts.film_config.has_value()) {
    if (opts.direct_campaign_mission.isEmpty() && opts.direct_mission_file.isEmpty() &&
        opts.observe_map_file.isEmpty() && opts.replay_path.isEmpty()) {
      qCritical() << "--film needs a directly launched match (--mission-file, "
                     "--campaign-mission, --observe or --replay)";
      return 2;
    }
    film_recorder = std::make_unique<App::Core::FilmRecorder>(
        game_engine.get(), window, *opts.film_config, opts.runtime_action_fixture);
    film_recorder->start();
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

  if (App::Bootstrap::opengl_crash_detected()) {
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
