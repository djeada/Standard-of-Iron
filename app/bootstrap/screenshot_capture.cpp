#include "app/bootstrap/screenshot_capture.h"

#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <QMetaObject>
#include <QQuickWindow>
#include <QTimer>
#include <QVariant>

namespace App::Bootstrap {

void capture_screenshot_and_exit(QQuickWindow* window,
                                 const QString& path,
                                 const QString& view,
                                 int delay_ms,
                                 QSize size) {

  window->setWindowState(Qt::WindowNoState);
  window->setWidth(size.width());
  window->setHeight(size.height());

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

} // namespace App::Bootstrap
