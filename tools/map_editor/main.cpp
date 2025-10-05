#include "editor_window.h"
#include <QApplication>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  MapEditor::EditorWindow window;
  window.show();

  return app.exec();
}