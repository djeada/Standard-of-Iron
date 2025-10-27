#include "editor_window.h"
#include <QApplication>
#include <qapplication.h>

auto main(int argc, char *argv[]) -> int {
  QApplication const app(argc, argv);

  MapEditor::EditorWindow window;
  window.show();

  return QApplication::exec();
}