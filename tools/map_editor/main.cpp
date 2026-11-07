#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QFile>
#include <QTimer>

#include <memory>

#include "campaign_workspace.h"
#include "editor_window.h"
#include "game/session/session_context.h"
#include "generation_preview_dialog.h"
#include "generator_client.h"
#include "ui/preferences.h"
#include "ui/theme.h"
#include "ui/widget_shell.h"
#include "world_canvas.h"

auto main(int argc, char* argv[]) -> int {

  QApplication app(argc, argv);

  Game::Session::SessionContext session;
  Game::Session::ScopedSession const active_session(session);
  QApplication::setApplicationName("Standard of Iron Map Editor");
  QApplication::setApplicationVersion("1.0");

  QApplication::setOrganizationName("djeada");
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
  QCommandLineOption const screenshot_tab_opt(
      "screenshot-tab", "Open this sidebar tab before the screenshot.", "title");
  QCommandLineOption const preview_candidate_opt(
      "preview-candidate",
      "Show the generation preview for this candidate map (with --preview-report).",
      "path");
  QCommandLineOption const preview_report_opt(
      "preview-report", "Generation report shown by --preview-candidate.", "path");
  parser.addOption(screenshot_opt);
  parser.addOption(screenshot_delay_opt);
  QCommandLineOption const screenshot_size_opt(
      "screenshot-size", "Window size for the screenshot.", "WxH", "1600x900");
  parser.addOption(screenshot_tab_opt);
  parser.addOption(screenshot_size_opt);
  parser.addOption(preview_candidate_opt);
  parser.addOption(preview_report_opt);
  QCommandLineOption const world_opt("world", "Start in the Campaign workspace.");
  QCommandLineOption const world_file_opt(
      "world-file", "Strategic world file to open (default assets/world/).", "path");
  QCommandLineOption const world_select_opt(
      "world-select", "Select and focus this strategic entity.", "id");
  QCommandLineOption const world_at_opt(
      "world-at", "Replay the campaign chronology up to this event.", "event");
  QCommandLineOption const world_path_opt(
      "world-path", "Show the cheapest march between two sites.", "from,to");
  QCommandLineOption const world_layers_opt(
      "world-layers",
      "Extra strategic layers to switch on (supply,resources,districts).",
      "list");
  parser.addOption(world_opt);
  parser.addOption(world_file_opt);
  parser.addOption(world_select_opt);
  parser.addOption(world_at_opt);
  parser.addOption(world_path_opt);
  parser.addOption(world_layers_opt);

  parser.process(app);

  if (parser.isSet(preview_candidate_opt)) {
    const auto read_all = [](const QString& path) {
      QFile file(path);
      return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    };
    QString error;
    const auto report = MapEditor::Generator::parse_report(
        read_all(parser.value(preview_report_opt)), &error);
    if (!report.has_value()) {
      qCritical().noquote() << "Unreadable generation report:" << error;
      return 2;
    }
    auto dialog = std::make_unique<MapEditor::GenerationPreviewDialog>(
        read_all(parser.value(preview_candidate_opt)),
        *report,
        QStringLiteral("Generated map preview"));
    dialog->show();
    if (parser.isSet(screenshot_opt)) {
      bool delay_ok = false;
      const int delay = parser.value(screenshot_delay_opt).toInt(&delay_ok);
      UiShell::capture_and_exit(
          *dialog, parser.value(screenshot_opt), delay_ok ? delay : 1200);
    }
    return QApplication::exec();
  }

  const bool world_requested = parser.isSet(world_opt) || parser.isSet(world_file_opt);
  if (world_requested) {
    MapEditor::EditorWindow window;
    UiShell::prepare_tool_window(window);
    if (parser.isSet(screenshot_opt)) {
      const QStringList size =
          parser.value(screenshot_size_opt).split(QLatin1Char('x'));
      window.resize(size.value(0).toInt() > 0 ? size.value(0).toInt() : 1600,
                    size.value(1).toInt() > 0 ? size.value(1).toInt() : 900);
    }
    window.show();
    MapEditor::CampaignWorkspace* world =
        window.open_campaign(parser.value(world_file_opt));
    for (const auto& layer :
         parser.value(world_layers_opt).split(QLatin1Char(','), Qt::SkipEmptyParts)) {
      if (layer == QLatin1String("supply")) {
        world->canvas()->set_layer(MapEditor::WorldCanvas::Supply, true);
      } else if (layer == QLatin1String("resources")) {
        world->canvas()->set_layer(MapEditor::WorldCanvas::Resources, true);
      } else if (layer == QLatin1String("districts")) {
        world->canvas()->set_layer(MapEditor::WorldCanvas::Districts, true);
      } else if (layer == QLatin1String("legend")) {
        world->canvas()->set_layer(MapEditor::WorldCanvas::Legend, true);
      }
    }
    if (parser.isSet(world_at_opt)) {
      world->set_timeline(parser.value(world_at_opt));
    }
    if (parser.isSet(world_path_opt)) {
      const QStringList ends = parser.value(world_path_opt).split(QLatin1Char(','));
      if (ends.size() == 2) {
        world->show_path(ends[0], ends[1]);
      }
    }
    if (parser.isSet(world_select_opt)) {
      const QString id = parser.value(world_select_opt);
      QTimer::singleShot(80, world, [world, id]() { world->select_entity(id); });
    }
    if (parser.isSet(screenshot_tab_opt)) {
      window.select_sidebar_tab(parser.value(screenshot_tab_opt));
    }
    if (parser.isSet(screenshot_opt)) {
      bool delay_ok = false;
      const int delay = parser.value(screenshot_delay_opt).toInt(&delay_ok);
      UiShell::capture_and_exit(
          window, parser.value(screenshot_opt), delay_ok ? delay : 1200);
    }
    return QApplication::exec();
  }

  MapEditor::EditorWindow window;
  UiShell::prepare_tool_window(window);
  if (parser.isSet(screenshot_opt)) {
    const QStringList size = parser.value(screenshot_size_opt).split(QLatin1Char('x'));
    const int width = size.value(0).toInt();
    const int height = size.value(1).toInt();
    window.resize(width > 0 ? width : 1600, height > 0 ? height : 900);
  }
  window.show();
  if (parser.isSet(screenshot_tab_opt)) {
    window.select_sidebar_tab(parser.value(screenshot_tab_opt));
  }
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
