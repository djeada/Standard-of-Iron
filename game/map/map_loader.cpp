#include "map_loader.h"

#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

#include "map/map_definition.h"
#include "map_loader_internal.h"

namespace Game::Map {

namespace {

auto read_root_object(const QString& path,
                      QJsonObject& root,
                      QString* out_error) -> bool {
  QFile map_file(path);
  if (!map_file.open(QIODevice::ReadOnly)) {
    if (out_error != nullptr) {
      *out_error = QString("Failed to open map file: %1").arg(path);
    }
    return false;
  }
  auto data = map_file.readAll();
  map_file.close();

  QJsonParseError perr;
  auto doc = QJsonDocument::fromJson(data, &perr);
  if (perr.error != QJsonParseError::NoError) {
    if (out_error != nullptr) {
      *out_error = QString("JSON parse error at %1: %2")
                       .arg(perr.offset)
                       .arg(perr.errorString());
    }
    return false;
  }
  if (!doc.isObject()) {
    if (out_error != nullptr) {
      *out_error = "Map JSON root must be an object";
    }
    return false;
  }
  root = doc.object();
  return true;
}

auto reject_retired_keys(const QJsonObject& root, QString* out_error) -> bool {
  if (root.contains(QStringLiteral("buildings")) ||
      root.contains(QStringLiteral("walls"))) {
    if (out_error != nullptr) {
      *out_error =
          "Retired map keys 'buildings'/'walls' are not supported; use 'structures'";
    }
    return false;
  }
  return true;
}

} // namespace

auto MapLoader::load_from_json_file(const QString& path,
                                    MapDefinition& out_map,
                                    QString* out_error) -> bool {
  using namespace loader_detail;

  out_map = MapDefinition{};
  QJsonObject root;
  if (!read_root_object(path, root, out_error) ||
      !reject_retired_keys(root, out_error)) {
    return false;
  }

  if (!read_map_header(root, out_map, out_error) ||
      !read_map_units(root, out_map, out_error)) {
    return false;
  }
  read_map_scenery(root, out_map);
  read_map_terrain(root, out_map);
  read_map_fog(root, out_map);
  read_map_appearance(root, out_map);
  const bool wildlife_authored = read_map_wildlife(root, out_map);
  read_map_clock_and_resources(root, out_map);
  finish_map_wildlife(out_map, wildlife_authored);
  return true;
}

} // namespace Game::Map
