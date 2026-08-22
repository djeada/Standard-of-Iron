#include "widget_shell.h"

#include <QApplication>
#include <QDebug>
#include <QMainWindow>
#include <QMenuBar>
#include <QPixmap>
#include <QStatusBar>
#include <QTimer>
#include <QWidget>

#include "brand_fonts.h"
#include "preferences.h"
#include "theme.h"

namespace UiShell {

void apply(QApplication& app) {

  Ui::BrandFonts::register_bundled();

  app.setStyleSheet(Theme::widgetStyleSheet());

  auto* prefs = UiPreferences::instance();
  const auto restyle = [&app]() {
    app.setStyleSheet(Theme::widgetStyleSheet());
  };

  QObject::connect(prefs, &UiPreferences::ui_scale_changed, &app, restyle);
  QObject::connect(prefs, &UiPreferences::high_contrast_changed, &app, restyle);
}

void prepare_tool_window(QMainWindow& window) {

  window.menuBar();
  window.statusBar();
  window.setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks);

  if (UiPreferences::instance()->reduced_motion()) {
    window.setDockOptions(window.dockOptions() & ~QMainWindow::AnimatedDocks);
  }
}

void capture_and_exit(QWidget& window, const QString& path, int delay_ms) {
  QWidget* target = &window;
  QTimer::singleShot(delay_ms, target, [target, path]() {
    const QPixmap frame = target->grab();
    if (frame.isNull() || !frame.save(path)) {
      qCritical() << "SOI_SCREENSHOT: FAIL -" << path;
      QCoreApplication::exit(11);
      return;
    }
    qInfo() << "SOI_SCREENSHOT: PASS -" << path << frame.width() << "x"
            << frame.height();
    QCoreApplication::exit(0);
  });
}

} // namespace UiShell
