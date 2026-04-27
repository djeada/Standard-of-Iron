#include <QApplication>
#include <QSurfaceFormat>

#include "arena_window.h"

auto main(int argc, char **argv) -> int {
  QSurfaceFormat fmt;
  fmt.setVersion(3, 3);
  fmt.setProfile(QSurfaceFormat::CoreProfile);
  fmt.setDepthBufferSize(24);
  fmt.setStencilBufferSize(8);
  QSurfaceFormat::setDefaultFormat(fmt);

  QApplication app(argc, argv);
  QApplication::setApplicationName("Standard of Iron Arena");
  QApplication::setApplicationVersion("1.0");

  ArenaWindow window;
  window.resize(1600, 900);
  window.show();

  return app.exec();
}
