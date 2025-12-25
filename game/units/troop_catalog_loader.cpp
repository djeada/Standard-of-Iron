#include "troop_catalog_loader.h"

#include "troop_catalog.h"
#include "troop_config.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QVariant>

namespace {
[[nodiscard]] auto ensure_array(const QJsonValue &value) -> QJsonArray {
  if (value.isArray()) {
    return value.toArray();
  }
  return {};
}

[[nodiscard]] auto ensure_object(const QJsonValue &value) -> QJsonObject {
  if (value.isObject()) {
    return value.toObject();
  }
  return {};
}

[[nodiscard]] auto read_float(const QJsonObject &obj, const char *key,
                              float fallback) -> float {
  if (!obj.contains(key)) {
    return fallback;
  }
  const auto value = obj.value(key);
  return static_cast<float>(value.toDouble(fallback));
}

[[nodiscard]] auto read_int(const QJsonObject &obj, const char *key,
                            int fallback) -> int {
  if (!obj.contains(key)) {
    return fallback;
  }
  const auto value = obj.value(key);
  if (value.isDouble()) {
    return value.toInt(fallback);
  }
  if (value.isString()) {
    bool ok = false;
    const auto str = value.toString();
    const int result = str.toInt(&ok);
    return ok ? result : fallback;
  }
  return fallback;
}

[[nodiscard]] auto read_bool(const QJsonObject &obj, const char *key,
                             bool fallback) -> bool {
  if (!obj.contains(key)) {
    return fallback;
  }
  return obj.value(key).toBool(fallback);
}

} 

namespace Game::Units {

static constexpr const char *k_troop_list_key = "troops";
static bool g_catalog_loaded = false;

static auto logger() -> QLoggingCategory & {
  static QLoggingCategory category("TroopCatalogLoader");
  return category;
}

auto TroopCatalogLoader::resolve_data_path(const QString &relative) -> QString {
  const QString direct = QDir::current().filePath(relative);
  if (QFile::exists(direct)) {
    return direct;
  }

  const QString appDir = QCoreApplication::applicationDirPath();
  if (!appDir.isEmpty()) {
    const QString fromApp = QDir(appDir).filePath(relative);
    if (QFile::exists(fromApp)) {
      return fromApp;
    }

    const QString parent = QDir(appDir).filePath("../" + relative);
    if (QFile::exists(parent)) {
      return QDir(parent).canonicalPath();
    }
  }

  const QString resource_path = QStringLiteral(":/") + relative;
  if (QFile::exists(resource_path)) {
    return resource_path;
  }

  return {};
}

auto TroopCatalogLoader::load_default_catalog() -> bool {
  if (g_catalog_loaded) {
    return true;
  }

  const QString path = resolve_data_path("assets/data/troops/base.json");
  if (path.isEmpty()) {
    qCWarning(logger()) << "Failed to locate base troop catalog at"
                        << "assets/data/troops/base.json";
    return false;
  }
  if (!load_from_file(path)) {
    return false;
  }
  g_catalog_loaded = true;
  return true;
}

auto TroopCatalogLoader::load_from_file(const QString &path) -> bool {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    qCWarning(logger()) << "Unable to open troop catalog" << path << ":"
                        << file.errorString();
    return false;
  }

  const QByteArray data = file.readAll();
  QJsonParseError parseError;
  const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
  if (parseError.error != QJsonParseError::NoError) {
    qCWarning(logger()) << "Failed to parse troop catalog" << path << ":"
                        << parseError.errorString();
    return false;
  }

  const QJsonObject root = doc.object();
  const QJsonArray troops = ensure_array(root.value(k_troop_list_key));
  if (troops.isEmpty()) {
    qCWarning(logger()) << "Troop catalog" << path
                        << "does not contain a 'troops' array";
    return false;
  }

  auto &catalog = TroopCatalog::instance();
  catalog.clear();

  for (const QJsonValue &value : troops) {
    const QJsonObject troop_obj = ensure_object(value);
    const QString troop_id = troop_obj.value("id").toString();
    if (troop_id.isEmpty()) {
      qCWarning(logger()) << "Encountered troop without id in" << path;
      continue;
    }

    const auto type_opt = tryParseTroopType(troop_id.toStdString());
    if (!type_opt.has_value()) {
      qCWarning(logger()) << "Unknown troop type" << troop_id << "in" << path;
      continue;
    }

    TroopClass troop_class{};
    troop_class.unit_type = *type_opt;
    troop_class.display_name =
        troop_obj.value("display_name").toString(troop_id).toStdString();

    const QJsonObject production = ensure_object(troop_obj.value("production"));
    troop_class.production.cost =
        read_int(production, "cost", troop_class.production.cost);
    troop_class.production.build_time =
        read_float(production, "build_time", troop_class.production.build_time);
    troop_class.production.priority =
        read_int(production, "priority", troop_class.production.priority);
    troop_class.production.is_melee =
        read_bool(production, "is_melee", troop_class.production.is_melee);

    const QJsonObject combat = ensure_object(troop_obj.value("combat"));
    troop_class.combat.health =
        read_int(combat, "health", troop_class.combat.health);
    troop_class.combat.max_health =
        read_int(combat, "max_health", troop_class.combat.max_health);
    troop_class.combat.speed =
        read_float(combat, "speed", troop_class.combat.speed);
    troop_class.combat.vision_range =
        read_float(combat, "vision_range", troop_class.combat.vision_range);
    troop_class.combat.ranged_range =
        read_float(combat, "ranged_range", troop_class.combat.ranged_range);
    troop_class.combat.ranged_damage =
        read_int(combat, "ranged_damage", troop_class.combat.ranged_damage);
    troop_class.combat.ranged_cooldown = read_float(
        combat, "ranged_cooldown", troop_class.combat.ranged_cooldown);
    troop_class.combat.melee_range =
        read_float(combat, "melee_range", troop_class.combat.melee_range);
    troop_class.combat.melee_damage =
        read_int(combat, "melee_damage", troop_class.combat.melee_damage);
    troop_class.combat.melee_cooldown =
        read_float(combat, "melee_cooldown", troop_class.combat.melee_cooldown);
    troop_class.combat.can_ranged =
        read_bool(combat, "can_ranged", troop_class.combat.can_ranged);
    troop_class.combat.can_melee =
        read_bool(combat, "can_melee", troop_class.combat.can_melee);
    troop_class.combat.max_stamina =
        read_float(combat, "max_stamina", troop_class.combat.max_stamina);
    troop_class.combat.stamina_regen_rate = read_float(
        combat, "stamina_regen_rate", troop_class.combat.stamina_regen_rate);
    troop_class.combat.stamina_depletion_rate =
        read_float(combat, "stamina_depletion_rate",
                   troop_class.combat.stamina_depletion_rate);

    const QJsonObject visuals = ensure_object(troop_obj.value("visuals"));
    troop_class.visuals.render_scale =
        read_float(visuals, "render_scale", troop_class.visuals.render_scale);
    troop_class.visuals.selection_ring_size =
        read_float(visuals, "selection_ring_size",
                   troop_class.visuals.selection_ring_size);
    troop_class.visuals.selection_ring_y_offset =
        read_float(visuals, "selection_ring_y_offset",
                   troop_class.visuals.selection_ring_y_offset);
    troop_class.visuals.selection_ring_ground_offset =
        read_float(visuals, "selection_ring_ground_offset",
                   troop_class.visuals.selection_ring_ground_offset);
    const QString default_renderer = QStringLiteral("troops/") + troop_id;
    troop_class.visuals.renderer_id =
        visuals.value("renderer_id").toString(default_renderer).toStdString();

    const QJsonObject formation = ensure_object(troop_obj.value("formation"));
    troop_class.individuals_per_unit = read_int(
        formation, "individuals_per_unit", troop_class.individuals_per_unit);
    troop_class.max_units_per_row =
        read_int(formation, "max_units_per_row", troop_class.max_units_per_row);

    catalog.register_class(std::move(troop_class));
  }

  TroopConfig::instance().refresh_from_catalog();
  return true;
}

} 
