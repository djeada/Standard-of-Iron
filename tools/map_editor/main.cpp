#include "editor_window.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>

auto main(int argc, char *argv[]) -> int {
  QApplication app(argc, argv);
  QApplication::setApplicationName("Standard of Iron Map Editor");
  QApplication::setApplicationVersion("1.0");

  QCommandLineParser parser;
  parser.setApplicationDescription("Map editor for Standard of Iron game");
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument("file", "Map file to open (optional)");

  parser.process(app);

  MapEditor::EditorWindow window;
  window.show();

  // Open file if provided as argument (after window is shown)
  const QStringList args = parser.positionalArguments();
  if (!args.isEmpty()) {
    const QString &filePath = args.first();
    QTimer::singleShot(0, [&window, filePath]() { window.loadFile(filePath); });
  }

  return QApplication::exec();
}