#include "strategic_assets.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include "strategic_json.h"

namespace Game::Strategic {

namespace {

auto read_json_object(const QString& path) -> std::optional<QJsonObject> {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return std::nullopt;
  }
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  if (!document.isObject()) {
    return std::nullopt;
  }
  return document.object();
}

} // namespace

auto find_repository_root(const QString& start) -> QString {
  QDir dir(QFileInfo(start).isDir() ? start : QFileInfo(start).absolutePath());
  for (int depth = 0; depth < 8; ++depth) {
    if (QFileInfo::exists(dir.filePath(QStringLiteral("assets/campaign_map"))) &&
        QFileInfo::exists(dir.filePath(QStringLiteral("assets/missions")))) {
      return dir.absolutePath();
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  return {};
}

auto resolve_asset_path(const QString& root, const QString& path) -> QString {
  QString relative = path;
  if (relative.startsWith(QStringLiteral(":/"))) {
    relative.remove(0, 2);
  } else if (relative.startsWith(QStringLiteral("qrc:/"))) {
    relative.remove(0, 5);
  }
  if (QFileInfo(relative).isAbsolute()) {
    return relative;
  }
  return QDir(root).filePath(relative);
}

auto build_validation_context(const QString& root,
                              const WorldDefinition& def,
                              const RegionGeometry* geometry) -> ValidationContext {
  ValidationContext context;
  context.geometry = geometry;
  context.resource_exists = [root](const QString& path) {
    return QFileInfo::exists(resolve_asset_path(root, path));
  };
  const QDir missions(QDir(root).filePath(QStringLiteral("assets/missions")));
  if (missions.exists()) {
    QSet<QString> ids;
    for (const auto& file :
         missions.entryList({QStringLiteral("*.json")}, QDir::Files)) {
      const auto mission = read_json_object(missions.filePath(file));
      if (!mission.has_value()) {
        continue;
      }
      const QString id = mission->value(QStringLiteral("id")).toString();
      if (id.isEmpty()) {
        continue;
      }
      ids.insert(id);
      MissionFacts facts;
      const auto side_of = [&facts](const QString& side, const QJsonObject& setup) {
        const QString nation = setup.value(QStringLiteral("nation")).toString();
        MissionSideRef ref;
        ref.side = side;
        ref.faction =
            nation == QLatin1String("roman_republic") ? QStringLiteral("rome") : nation;
        for (const auto value :
             setup.value(QStringLiteral("historical_commanders")).toArray()) {
          ref.commanders.append(value.toString());
        }
        facts.sides.append(ref);
      };
      side_of(QStringLiteral("player"),
              mission->value(QStringLiteral("player_setup")).toObject());
      for (const auto value : mission->value(QStringLiteral("ai_setups")).toArray()) {
        const QJsonObject setup = value.toObject();
        side_of(setup.value(QStringLiteral("id")).toString(), setup);
      }
      static const QRegularExpression k_years(
          QStringLiteral("(\\d{3})(?:\\s*[-–]\\s*(\\d{3}))?\\s*BC"));
      const auto match = k_years.match(
          mission->value(QStringLiteral("historical_context")).toString());
      if (match.hasMatch()) {
        facts.first_year = -match.captured(1).toInt();
        facts.last_year =
            match.captured(2).isEmpty() ? facts.first_year : -match.captured(2).toInt();
      }
      context.mission_facts.insert(id, facts);
      const QString map = mission->value(QStringLiteral("map_path")).toString();
      if (!map.isEmpty()) {
        context.mission_maps.insert(id, map);
      }
    }
    context.mission_ids = ids;
  }
  if (const auto provinces = read_json_object(QDir(root).filePath(
          QStringLiteral("assets/campaign_map/valid_provinces.json")))) {
    const QJsonObject mapping =
        provinces->value(QStringLiteral("mission_region_mapping")).toObject();
    for (auto it = mapping.begin(); it != mapping.end(); ++it) {
      context.mission_provinces.insert(it.key(), it.value().toString());
    }
  }
  if (!def.campaign.isEmpty()) {
    const auto campaign = read_json_object(QDir(root).filePath(
        QStringLiteral("assets/campaigns/%1.json").arg(def.campaign)));
    if (campaign.has_value()) {
      for (const auto value : campaign->value(QStringLiteral("missions")).toArray()) {
        const QJsonObject mission = value.toObject();
        context.campaign_missions.append(
            {mission.value(QStringLiteral("mission_id")).toString(),
             mission.value(QStringLiteral("world_region_id")).toString(),
             mission.value(QStringLiteral("order_index")).toInt()});
      }
    }
  }
  return context;
}

auto load_world_bundle(const QString& world_path,
                       const QString& root,
                       QString* error) -> std::optional<WorldBundle> {
  auto def = load_definition_file(world_path, error);
  if (!def.has_value()) {
    return std::nullopt;
  }
  WorldBundle bundle;
  bundle.definition = std::move(*def);
  bundle.root = root.isEmpty() ? find_repository_root(world_path) : root;
  bundle.path = world_path;
  if (!bundle.definition.geometry_source.isEmpty() && !bundle.root.isEmpty()) {
    QFile file(resolve_asset_path(bundle.root, bundle.definition.geometry_source));
    if (file.open(QIODevice::ReadOnly)) {
      if (auto geometry = RegionGeometry::from_provinces_json(file.readAll())) {
        bundle.geometry = std::make_shared<RegionGeometry>(std::move(*geometry));
      }
    }
  }
  if (!bundle.root.isEmpty()) {
    bundle.context =
        build_validation_context(bundle.root, bundle.definition, bundle.geometry.get());
  } else {
    bundle.context.geometry = bundle.geometry.get();
  }
  return bundle;
}

} // namespace Game::Strategic
