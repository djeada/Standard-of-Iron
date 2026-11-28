#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include "batch_edit.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_topology_audit.h"
#include "map_data.h"
#include "world_backdrop.h"

namespace {

auto out() -> QTextStream& {
  static QTextStream stream(stdout);
  return stream;
}

auto err() -> QTextStream& {
  static QTextStream stream(stderr);
  return stream;
}

auto usage() -> int {
  err()
      << "usage:\n"
         "  map_cli summary <map.json>\n"
         "  map_cli list <map.json> [kind] [--json]\n"
         "  map_cli audit <map.json>\n"
         "  map_cli backdrop <map.json> <out.png>     the editor's painted world\n"
         "  map_cli roundtrip <map.json> <out.json>\n"
         "  map_cli normalize <map.json> <out.json>   rewrite in the editor's schema\n"
         "  map_cli apply <map.json> <ops.json> [--out <path>] [--dry-run]\n"
         "  map_cli new <out.json> <width> <height> [name]   a blank world-space map\n"
         "\n"
         "kinds: terrain prop linear structure spawn undead_zone wildlife forest\n"
         "\n"
         "ops.json is an array of operations applied in order:\n"
         "  "
         "{\"op\":\"move\",\"kind\":K,\"index\":N|\"id\":S,\"to\":[x,z]|\"by\":[dx,dz]}"
         "\n"
         "  {\"op\":\"set\",\"kind\":K,\"index\":N|\"id\":S,\"fields\":{...}}   null "
         "removes a field\n"
         "  {\"op\":\"replace\",\"kind\":K,\"index\":N|\"id\":S,\"json\":{...}}\n"
         "  {\"op\":\"remove\",\"kind\":K,\"index\":N|\"id\":S}\n"
         "  {\"op\":\"add\",\"kind\":K,\"json\":{...}}\n"
         "  "
         "{\"op\":\"map\",\"name\":S,\"description\":S,\"biome\":{...},\"rain\":{...},"
         "\n"
         "   \"environment\":{...},\"camera\":{...},\"victory\":{...},"
         "\"max_troops_per_player\":N}\n"
         "  {\"op\":\"root\",\"key\":S,\"value\":V | \"append\":[...] | "
         "\"remove_ids\":[...]}\n"
         "     for root data the editor does not model: dressing, landmarks, "
         "settlements\n"
         "  {\"op\":\"terrace\",\"from\":[x,z],\"to\":[x,z],\"facing\":DEG,\"id\":S,"
         "\"skip\":[N,...]}\n"
         "  {\"op\":\"block\",\"origin\":[x,z],\"lots\":[WIDE,DEEP],\"rotation\":DEG,"
         "\"alley\":N,\"id\":S}\n"
         "     houses wall to wall on their own footprint; both take player_id, "
         "nation,\n"
         "     type (home) and style (tenement). A block is four terraces round a "
         "court.\n"
         "  {\"op\":\"paving\",\"rect\":[x0,z0,x1,z1],\"style\":\"stone\"}   a paved "
         "square\n"
         "  {\"op\":\"wall\",\"points\":[[x,z],...],\"closed\":true,"
         "\"tower_spacing\":44,\"id\":S}\n"
         "     an axis-aligned wall circuit, towers on its corners and along "
         "long legs\n"
         "  {\"op\":\"gate\",\"at\":[x,z],\"id\":S}   cut a gate into the wall "
         "run under the point\n"
         "  {\"op\":\"ambience_zone\",\"bed\":S,\"x\":X,\"z\":Z,\"radius\":R,"
         "\"fade\":F,\"night_bed\":S}\n"
         "     or \"polygon\":[[x,z],...] in place of x/z/radius\n"
         "An index refers to the map as it stands when that step runs.\n";
  return 2;
}

auto load(const QString& path, MapEditor::MapData& data) -> bool {
  QString error;
  if (!data.load_from_json(path, &error)) {
    err() << "cannot load " << path << ": " << error << "\n";
    return false;
  }
  return true;
}

auto run_list(const QStringList& args) -> int {
  MapEditor::MapData data;
  if (!load(args[0], data)) {
    return 1;
  }
  int kind = -1;
  bool json = false;
  for (int i = 1; i < args.size(); ++i) {
    if (args[i] == QLatin1String("--json")) {
      json = true;
    } else {
      kind = MapEditor::BatchEdit::kind_from_name(args[i]);
      if (kind < 0) {
        err() << "unknown kind '" << args[i] << "'\n";
        return 2;
      }
    }
  }
  const QJsonArray rows = MapEditor::BatchEdit::inventory(data, kind, json);
  if (json) {
    out() << QJsonDocument(rows).toJson(QJsonDocument::Indented);
    return 0;
  }
  for (const QJsonValue value : rows) {
    const QJsonObject row = value.toObject();
    out() << row.value("kind").toString() << "[" << row.value("index").toInt() << "]\t"
          << row.value("type").toString() << "\t";
    if (row.contains("x")) {
      out() << QString::number(row.value("x").toDouble(), 'f', 1) << ","
            << QString::number(row.value("z").toDouble(), 'f', 1);
    }
    out() << "\t" << row.value("id").toString() << "\t"
          << (row.value("generated").toBool() ? "generated" : "") << "\t"
          << row.value("summary").toString() << "\n";
  }
  return 0;
}

auto run_audit(const QString& path) -> int {
  Game::Map::MapDefinition map;
  QString error;
  if (!Game::Map::MapLoader::load_from_json_file(path, map, &error)) {
    err() << "cannot load " << path << ": " << error << "\n";
    return 1;
  }
  const auto audit = Game::Map::audit_terrain_topology(map);
  out() << "road components: " << audit.road_components << "\n"
        << "river components: " << audit.river_components << "\n"
        << "river ends inside the map: " << audit.invalid_river_endpoints << "\n"
        << "hills without two approaches: " << audit.hills_without_two_approaches
        << "\n"
        << "unanchored lakes: " << audit.tactically_unanchored_lakes << "\n";
  for (const QString& detail : audit.details) {
    out() << "  - " << detail << "\n";
  }
  out() << (audit.passed() ? "PASS" : "FAIL") << "\n";
  return audit.passed() ? 0 : 3;
}

auto run_apply(const QStringList& args) -> int {
  MapEditor::MapData data;
  if (!load(args[0], data)) {
    return 1;
  }
  QString out_path = args[0];
  bool dry_run = false;
  for (int i = 2; i < args.size(); ++i) {
    if (args[i] == QLatin1String("--dry-run")) {
      dry_run = true;
    } else if (args[i] == QLatin1String("--out") && i + 1 < args.size()) {
      out_path = args[++i];
    } else {
      return usage();
    }
  }
  QFile ops_file(args[1]);
  if (!ops_file.open(QIODevice::ReadOnly)) {
    err() << "cannot read " << args[1] << "\n";
    return 1;
  }
  QJsonParseError parse_error;
  const QJsonDocument ops = QJsonDocument::fromJson(ops_file.readAll(), &parse_error);
  if (!ops.isArray()) {
    err() << args[1] << " is not a JSON array: " << parse_error.errorString() << "\n";
    return 1;
  }
  const auto report = MapEditor::BatchEdit::apply(data, ops.array());
  for (const QString& line : report.log) {
    out() << line << "\n";
  }
  for (const QString& line : report.errors) {
    err() << "error: " << line << "\n";
  }
  if (!report.ok()) {
    err() << "nothing written: " << report.errors.size() << " operation(s) failed\n";
    return 1;
  }
  if (dry_run) {
    out() << report.applied << " operation(s) would apply\n";
    return 0;
  }
  QString error;
  if (!data.save_to_json(out_path, &error)) {
    err() << "cannot write " << out_path << ": " << error << "\n";
    return 1;
  }
  out() << report.applied << " operation(s) applied, wrote " << out_path << "\n";
  return 0;
}

auto run_new(const QStringList& args) -> int {
  bool width_ok = false;
  bool height_ok = false;
  const int width = args[1].toInt(&width_ok);
  const int height = args[2].toInt(&height_ok);
  if (!width_ok || !height_ok || width < 16 || height < 16) {
    err() << "width and height must be whole numbers of at least 16\n";
    return 2;
  }
  const QString name = args.size() > 3 ? args[3] : QStringLiteral("Untitled");
  const QJsonObject root{
      {QStringLiteral("name"), name},
      {QStringLiteral("coord_system"), QStringLiteral("world")},
      {QStringLiteral("grid"),
       QJsonObject{{QStringLiteral("width"), width},
                   {QStringLiteral("height"), height},
                   {QStringLiteral("tile_size"), 1}}},
  };
  MapEditor::MapData data;
  QString error;
  if (!data.load_from_bytes(QJsonDocument(root).toJson(QJsonDocument::Indented),
                            &error) ||
      !data.save_to_json(args[0], &error)) {
    err() << "cannot write " << args[0] << ": " << error << "\n";
    return 1;
  }
  out() << "wrote " << args[0] << " (" << width << " x " << height << ")\n";
  return 0;
}

} // namespace

auto main(int argc, char** argv) -> int {
  QCoreApplication app(argc, argv);
  QStringList args = QCoreApplication::arguments();
  args.removeFirst();
  if (args.isEmpty()) {
    return usage();
  }
  const QString command = args.takeFirst();
  int status = 2;
  if (command == QLatin1String("summary") && args.size() == 1) {
    MapEditor::MapData data;
    if (!load(args[0], data)) {
      return 1;
    }
    out() << QJsonDocument(MapEditor::BatchEdit::summary(data))
                 .toJson(QJsonDocument::Indented);
    status = 0;
  } else if (command == QLatin1String("list") && !args.isEmpty()) {
    status = run_list(args);
  } else if (command == QLatin1String("audit") && args.size() == 1) {
    status = run_audit(args[0]);
  } else if (command == QLatin1String("backdrop") && args.size() == 2) {
    MapEditor::MapData data;
    if (!load(args[0], data)) {
      return 1;
    }
    const auto backdrop =
        MapEditor::render_world_backdrop(data.to_canonical_json_bytes());
    if (!backdrop.ok() || !backdrop.image.save(args[1])) {
      err() << "cannot paint " << args[1] << ": " << backdrop.error << "\n";
      return 1;
    }
    out() << "painted " << backdrop.image.width() << "x" << backdrop.image.height()
          << " at " << backdrop.pixels_per_cell << " px per cell\n";
    status = 0;
  } else if (command == QLatin1String("roundtrip") && args.size() == 2) {
    MapEditor::MapData data;
    if (!load(args[0], data)) {
      return 1;
    }
    QString error;
    if (!data.save_to_json(args[1], &error)) {
      err() << "cannot write " << args[1] << ": " << error << "\n";
      return 1;
    }
    status = 0;
  } else if (command == QLatin1String("normalize") && args.size() == 2) {
    MapEditor::MapData data;
    if (!load(args[0], data)) {
      return 1;
    }
    QString error;
    if (!data.save_canonical_json(args[1], &error)) {
      err() << "cannot write " << args[1] << ": " << error << "\n";
      return 1;
    }
    status = 0;
  } else if (command == QLatin1String("apply") && args.size() >= 2) {
    status = run_apply(args);
  } else if (command == QLatin1String("new") && args.size() >= 3) {
    status = run_new(args);
  } else {
    status = usage();
  }
  out().flush();
  err().flush();
  return status;
}
