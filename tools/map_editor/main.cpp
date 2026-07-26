#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>

#include "editor_window.h"
#include "ui/preferences.h"
#include "ui/theme.h"
#include "ui/widget_shell.h"

auto main(int argc, char* argv[]) -> int {

  QApplication app(argc, argv);
  QApplication::setApplicationName("Standard of Iron Map Editor");
  QApplication::setApplicationVersion("1.0");
  UiShell::apply(app);

  QCommandLineParser parser;
  parser.setApplicationDescription("Map editor for Standard of Iron game");
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument("file", "Map file to open (optional)");
  QCommandLineOption const screenshot_opt(
      "screenshot", "Render one frame, write a PNG to this path, then exit.", "path");
  QCommandLineOption const screenshot_delay_opt(
      "screenshot-delay", "Milliseconds to let the window settle.", "ms", "1200");
  parser.addOption(screenshot_opt);
  parser.addOption(screenshot_delay_opt);

  parser.process(app);

  MapEditor::EditorWindow window;
  UiShell::prepare_tool_window(window);
  if (parser.isSet(screenshot_opt)) {
    window.resize(1600, 900);
  }
  window.show();
  if (parser.isSet(screenshot_opt)) {
    bool delay_ok = false;
    const int delay = parser.value(screenshot_delay_opt).toInt(&delay_ok);
    UiShell::capture_and_exit(
        window, parser.value(screenshot_opt), delay_ok ? delay : 1200);
  }

  const QStringList args = parser.positionalArguments();
  if (!args.isEmpty()) {
    const QString& file_path = args.first();
    QTimer::singleShot(0, [&window, file_path]() { window.load_file(file_path); });
  }

  return QApplication::exec();
}
