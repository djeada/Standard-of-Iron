#include "balance_fixture.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include <algorithm>

namespace Balance {

namespace {

auto read_float(const QJsonObject& obj, const char* key, float fallback) -> float {
  return obj.contains(key) ? static_cast<float>(obj.value(key).toDouble(fallback))
                           : fallback;
}

auto read_int(const QJsonObject& obj, const char* key, int fallback) -> int {
  return obj.contains(key) ? obj.value(key).toInt(fallback) : fallback;
}

auto read_bool(const QJsonObject& obj, const char* key, bool fallback) -> bool {
  return obj.contains(key) ? obj.value(key).toBool(fallback) : fallback;
}

auto read_optional_float(const QJsonObject& obj,
                         const char* key) -> std::optional<float> {
  if (!obj.contains(key)) {
    return std::nullopt;
  }
  return static_cast<float>(obj.value(key).toDouble());
}

auto parse_stance(const QString& value, Stance& out) -> bool {
  const QString lowered = value.trimmed().toLower();
  if (lowered == QStringLiteral("attack")) {
    out = Stance::Attack;
    return true;
  }
  if (lowered == QStringLiteral("hold")) {
    out = Stance::Hold;
    return true;
  }
  if (lowered == QStringLiteral("stand")) {
    out = Stance::Stand;
    return true;
  }
  if (lowered == QStringLiteral("charge")) {
    out = Stance::Charge;
    return true;
  }
  return false;
}

auto parse_side(const QJsonObject& obj,
                const QString& field,
                std::vector<FixtureLoadError>& errors,
                FixtureSide& out) -> bool {
  out.label = obj.value("label").toString(field);

  const QString nation_text =
      obj.value("nation").toString(QStringLiteral("roman_republic"));
  if (!Game::Systems::try_parse_nation_id(nation_text, out.nation)) {
    errors.push_back({field + ".nation", QStringLiteral("unknown nation '%1'").arg(nation_text)});
    return false;
  }

  const QString stance_text = obj.value("stance").toString(QStringLiteral("attack"));
  if (!parse_stance(stance_text, out.stance)) {
    errors.push_back(
        {field + ".stance", QStringLiteral("unknown stance '%1'").arg(stance_text)});
    return false;
  }

  const QJsonArray groups = obj.value("groups").toArray();
  if (groups.isEmpty()) {
    errors.push_back({field + ".groups", QStringLiteral("at least one group required")});
    return false;
  }

  for (const QJsonValue& value : groups) {
    const QJsonObject group_obj = value.toObject();
    FixtureGroup group;
    const QString troop_text = group_obj.value("troop").toString();
    if (!Game::Units::try_parse_troop_type(troop_text, group.troop)) {
      errors.push_back({field + ".groups.troop",
                        QStringLiteral("unknown troop '%1'").arg(troop_text)});
      return false;
    }
    group.count = std::max(1, read_int(group_obj, "count", 1));
    out.groups.push_back(group);
  }
  return true;
}

} // namespace

auto stance_name(Stance stance) -> QString {
  switch (stance) {
  case Stance::Attack:
    return QStringLiteral("attack");
  case Stance::Hold:
    return QStringLiteral("hold");
  case Stance::Stand:
    return QStringLiteral("stand");
  case Stance::Charge:
    return QStringLiteral("charge");
  }
  return QStringLiteral("attack");
}

auto load_fixture_file(const QString& path,
                       std::vector<FixtureLoadError>& errors) -> std::optional<Fixture> {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    errors.push_back({path, QStringLiteral("cannot open: %1").arg(file.errorString())});
    return std::nullopt;
  }

  QJsonParseError parse_error{};
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse_error);
  if (parse_error.error != QJsonParseError::NoError) {
    errors.push_back({path, QStringLiteral("parse error: %1").arg(parse_error.errorString())});
    return std::nullopt;
  }

  const QJsonObject root = doc.object();
  Fixture fixture;
  fixture.id = root.value("id").toString(QFileInfo(path).baseName());
  fixture.label = root.value("label").toString(fixture.id);
  fixture.description = root.value("description").toString();

  fixture.duration_seconds = read_float(root, "duration_seconds", fixture.duration_seconds);
  fixture.timestep = read_float(root, "timestep", fixture.timestep);
  fixture.seeds = std::max(1, read_int(root, "seeds", fixture.seeds));
  fixture.mirror_sides = read_bool(root, "mirror_sides", fixture.mirror_sides);
  fixture.grid_width = read_int(root, "grid_width", fixture.grid_width);
  fixture.grid_height = read_int(root, "grid_height", fixture.grid_height);
  fixture.separation = read_float(root, "separation", fixture.separation);
  fixture.spawn_jitter = read_float(root, "spawn_jitter", fixture.spawn_jitter);

  const std::size_t error_count = errors.size();
  if (!parse_side(root.value("side_a").toObject(), QStringLiteral("side_a"), errors,
                  fixture.side_a)) {
    return std::nullopt;
  }
  if (!parse_side(root.value("side_b").toObject(), QStringLiteral("side_b"), errors,
                  fixture.side_b)) {
    return std::nullopt;
  }
  if (errors.size() != error_count) {
    return std::nullopt;
  }

  const QJsonObject expect = root.value("expect").toObject();
  fixture.expect.a_win_rate_min = read_optional_float(expect, "a_win_rate_min");
  fixture.expect.a_win_rate_max = read_optional_float(expect, "a_win_rate_max");
  fixture.expect.max_timeout_rate = read_optional_float(expect, "max_timeout_rate");
  fixture.expect.max_spawn_side_bias = read_optional_float(expect, "max_spawn_side_bias");

  return fixture;
}

auto load_fixture_directory(const QString& directory,
                            std::vector<FixtureLoadError>& errors)
    -> std::vector<Fixture> {
  std::vector<Fixture> fixtures;
  QDir dir(directory);
  if (!dir.exists()) {
    errors.push_back({directory, QStringLiteral("directory does not exist")});
    return fixtures;
  }

  const QStringList files =
      dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
  for (const QString& name : files) {
    if (auto fixture = load_fixture_file(dir.filePath(name), errors)) {
      fixtures.push_back(std::move(*fixture));
    }
  }
  return fixtures;
}

} // namespace Balance
