#include "app/bootstrap/self_test_driver.h"

#include <QDebug>
#include <QGuiApplication>
#include <QObject>
#include <QQuickWindow>
#include <QString>
#include <QTimer>

#include <memory>

#include "app/core/game_engine.h"
#include "ui/gl_view.h"

namespace App::Bootstrap {

auto start_renderer_self_test(QGuiApplication& app,
                              QObject* root_obj,
                              QQuickWindow* window) -> int {
  auto* gl_view = root_obj->findChild<GLView*>();
  if (gl_view == nullptr) {
    qCritical() << "SOI_RENDERER_SELF_TEST: FAIL - GLView was not created";
    return 10;
  }

  auto renderer_ready = std::make_shared<bool>(false);

  auto self_test_settled = std::make_shared<bool>(false);
  QObject::connect(gl_view, &GLView::renderer_ready, &app, [window, renderer_ready]() {
    *renderer_ready = true;
    window->update();
  });
  QObject::connect(
      window, &QQuickWindow::frameSwapped, &app, [renderer_ready, self_test_settled]() {
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
  return -1;
}

void start_release_self_test(QGuiApplication& app,
                             QQuickWindow* window,
                             GameEngine* game_engine) {
  auto mission_ready = std::make_shared<bool>(false);
  auto presented_frames = std::make_shared<int>(0);

  auto release_test_settled = std::make_shared<bool>(false);
  auto polls = std::make_shared<int>(0);
  auto* readiness_poll = new QTimer(&app);
  readiness_poll->setInterval(250);
  QObject::connect(readiness_poll,
                   &QTimer::timeout,
                   &app,
                   [game_engine_ptr = game_engine,
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
                     qInfo() << "SOI_RENDERER_SELF_TEST: PASS - three gameplay frames "
                                "rendered and presented after mission load";
                     QGuiApplication::exit(0);
                   });

  QTimer::singleShot(
      1500000,
      &app,
      [game_engine_ptr = game_engine,
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

} // namespace App::Bootstrap
