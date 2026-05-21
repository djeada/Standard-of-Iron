#include "nation_loader.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

#include "../units/building_type.h"
#include "../units/troop_catalog.h"
#include "../units/troop_catalog_loader.h"
#include "../units/troop_type.h"
#include "nation_id.h"
#include "nation_registry.h"

namespace {

using Game::Systems::FormationType;
using Game::Systems::Nation;
using Game::Systems::NationLoader;
using Game::Systems::NationTroopVariant;
using Game::Systems::TroopType;
using Game::Units::TroopCatalog;
using Game::Units::TroopClass;

[[nodiscard]] auto ensure_object(const QJsonValue& value) -> QJsonObject {
  if (value.isObject()) {
    return value.toObject();
  }
  return {};
}

[[nodiscard]] auto ensure_array(const QJsonValue& value) -> QJsonArray {
  if (value.isArray()) {
    return value.toArray();
  }
  return {};
}

[[nodiscard]] auto read_string(const QJsonObject& obj,
                               const char* key,
                               const QString& fallback) -> QString {
  const auto value = obj.value(key);
  if (value.isString()) {
    return value.toString();
  }
  return fallback;
}

[[nodiscard]] auto read_float_opt(const QJsonObject& obj,
                                  const char* key) -> std::optional<float> {
  if (!obj.contains(key)) {
    return std::nullopt;
  }
  const auto value = obj.value(key);
  return static_cast<float>(value.toDouble());
}

[[nodiscard]] auto read_int_opt(const QJsonObject& obj,
                                const char* key) -> std::optional<int> {
  if (!obj.contains(key)) {
    return std::nullopt;
  }
  const auto value = obj.value(key);
  if (value.isDouble()) {
    return value.toInt();
  }
  if (value.isString()) {
    bool ok = false;
    const auto str = value.toString();
    const int parsed = str.toInt(&ok);
    if (ok) {
      return parsed;
    }
  }
  return std::nullopt;
}

[[nodiscard]] auto
read_bool(const QJsonObject& obj, const char* key, bool fallback) -> bool {
  if (!obj.contains(key)) {
    return fallback;
  }
  return obj.value(key).toBool(fallback);
}

[[nodiscard]] auto read_bool_opt(const QJsonObject& obj,
                                 const char* key) -> std::optional<bool> {
  if (!obj.contains(key)) {
    return std::nullopt;
  }
  return obj.value(key).toBool();
}

[[nodiscard]] auto read_string_array(const QJsonObject& obj,
                                     const char* key) -> std::vector<std::string> {
  std::vector<std::string> values;
  const QJsonArray array = ensure_array(obj.value(key));
  values.reserve(array.size());
  for (const auto& entry : array) {
    if (entry.isString()) {
      values.push_back(entry.toString().toStdString());
    }
  }
  return values;
}

[[nodiscard]] auto read_resource_amounts(const QJsonObject& obj,
                                         const char* key,
                                         Game::Systems::ResourceAmounts fallback)
    -> Game::Systems::ResourceAmounts {
  if (!obj.contains(key)) {
    return fallback;
  }

  const QJsonObject resource_costs = ensure_object(obj.value(key));
  for (Game::Systems::ResourceType const type : Game::Systems::k_all_resource_types) {
    const char* const resource_key = Game::Systems::resource_type_key(type);
    fallback.set(
        type, read_int_opt(resource_costs, resource_key).value_or(fallback.get(type)));
  }
  return fallback;
}

[[nodiscard]] auto
parse_formation_type(const QString& value) -> std::optional<FormationType> {
  const QString lowered = value.trimmed().toLower();
  if (lowered == QStringLiteral("roman")) {
    return FormationType::Roman;
  }
  if (lowered == QStringLiteral("barbarian")) {
    return FormationType::Barbarian;
  }
  if (lowered == QStringLiteral("carthage")) {
    return FormationType::Carthage;
  }
  return std::nullopt;
}

[[nodiscard]] auto logger() -> QLoggingCategory& {
  static QLoggingCategory category("NationLoader");
  return category;
}

constexpr const char* k_nation_troops_key = "troops";

auto nation_loader_logger() -> QLoggingCategory& {
  return logger();
}

[[nodiscard]] auto build_troop_entry(const QJsonObject& obj, Nation& nation) -> bool {
  const QString troop_id = obj.value("id").toString();
  if (troop_id.isEmpty()) {
    qCWarning(logger()) << "Encountered troop without id in nation"
                        << nation_id_to_qstring(nation.id);
    return false;
  }

  const auto type_opt = Game::Units::try_parse_troop_type(troop_id.toStdString());
  if (!type_opt.has_value()) {
    qCWarning(logger()) << "Unknown troop type" << troop_id << "for nation"
                        << nation_id_to_qstring(nation.id);
    return false;
  }

  const Game::Units::TroopType troop_type = *type_opt;
  const TroopClass& base_class =
      TroopCatalog::instance().get_class_or_fallback(troop_type);

  TroopType entry{};
  entry.unit_type = troop_type;
  entry.display_name =
      read_string(obj, "display_name", QString::fromStdString(base_class.display_name))
          .toStdString();
  entry.is_melee = read_bool(ensure_object(obj.value("production")),
                             "is_melee",
                             base_class.production.is_melee);
  const QJsonObject production = ensure_object(obj.value("production"));
  entry.cost = production.value("cost").toInt(base_class.production.cost);
  entry.resource_costs = read_resource_amounts(
      production, "resource_costs", base_class.production.resource_costs);
  entry.build_time = static_cast<float>(
      production.value("build_time").toDouble(base_class.production.build_time));
  entry.priority = production.value("priority").toInt(base_class.production.priority);

  nation.available_troops.push_back(entry);

  NationTroopVariant variant{};
  variant.unit_type = troop_type;
  bool has_variant = false;

  const QJsonObject combat = ensure_object(obj.value("combat"));
  if (auto value = read_int_opt(combat, "health")) {
    variant.health = value;
    has_variant = true;
  }
  if (auto value = read_int_opt(combat, "max_health")) {
    variant.max_health = value;
    has_variant = true;
  }
  if (auto value = read_float_opt(combat, "speed")) {
    variant.speed = value;
    has_variant = true;
  }
  if (auto value = read_float_opt(combat, "vision_range")) {
    variant.vision_range = value;
    has_variant = true;
  }
  if (auto value = read_int_opt(combat, "ranged_damage")) {
    variant.attack_damage = value;
    has_variant = true;
  }
  if (auto value = read_float_opt(combat, "ranged_range")) {
    variant.attack_range = value;
    has_variant = true;
  }
  if (auto value = read_float_opt(combat, "ranged_cooldown")) {
    variant.attack_cooldown = value;
    has_variant = true;
  }
  if (auto value = read_int_opt(combat, "melee_damage")) {
    variant.melee_damage = value;
    has_variant = true;
  }
  if (auto value = read_float_opt(combat, "melee_range")) {
    variant.melee_range = value;
    has_variant = true;
  }
  if (auto value = read_float_opt(combat, "melee_cooldown")) {
    variant.melee_cooldown = value;
    has_variant = true;
  }
  if (auto value = read_bool_opt(combat, "can_ranged")) {
    variant.can_ranged = value;
    has_variant = true;
  }
  if (auto value = read_bool_opt(combat, "can_melee")) {
    variant.can_melee = value;
    has_variant = true;
  }
  if (auto value = read_float_opt(combat, "max_stamina")) {
    variant.max_stamina = value;
    has_variant = true;
  }
  if (auto value = read_float_opt(combat, "stamina_regen_rate")) {
    variant.stamina_regen_rate = value;
    has_variant = true;
  }
  if (auto value = read_float_opt(combat, "stamina_depletion_rate")) {
    variant.stamina_depletion_rate = value;
    has_variant = true;
  }

  const QJsonObject visuals = ensure_object(obj.value("visuals"));
  if (auto value = read_float_opt(visuals, "selection_ring_size")) {
    variant.selection_ring_size = value;
    has_variant = true;
  }
  if (auto value = read_float_opt(visuals, "selection_ring_ground_offset")) {
    variant.selection_ring_ground_offset = value;
    has_variant = true;
  }
  if (auto value = read_float_opt(visuals, "formation_spacing")) {
    variant.formation_spacing = value;
    has_variant = true;
  }
  if (visuals.contains("renderer_id")) {
    variant.renderer_id = visuals.value("renderer_id").toString().toStdString();
    has_variant = true;
  }
  if (auto value = read_float_opt(visuals, "render_scale")) {
    variant.render_scale = value;
    has_variant = true;
  }

  const QJsonObject formation = ensure_object(obj.value("formation"));
  if (auto value = read_int_opt(formation, "individuals_per_unit")) {
    variant.individuals_per_unit = value;
    has_variant = true;
  }
  if (auto value = read_int_opt(formation, "max_units_per_row")) {
    variant.max_units_per_row = value;
    has_variant = true;
  }

  if (auto formation_override =
          parse_formation_type(obj.value("formation_type").toString())) {
    variant.formation_type = formation_override;
    has_variant = true;
  }
  variant.abilities = read_string_array(obj, "abilities");
  if (!variant.abilities.empty()) {
    has_variant = true;
  }

  if (has_variant) {
    nation.troop_variants[troop_type] = std::move(variant);
  }

  return true;
}

} // namespace

namespace Game::Systems {

auto NationLoader::resolve_data_path(const QString& relative) -> QString {
  const QString direct = QDir::current().filePath(relative);
  if (QFile::exists(direct)) {
    return direct;
  }

  const QString app_dir = QCoreApplication::applicationDirPath();
  if (!app_dir.isEmpty()) {
    const QString from_app = QDir(app_dir).filePath(relative);
    if (QFile::exists(from_app)) {
      return from_app;
    }

    const QString parent = QDir(app_dir).filePath("../" + relative);
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

auto NationLoader::load_default_nations() -> std::vector<Nation> {
  if (!Game::Units::TroopCatalogLoader::load_default_catalog()) {
    qCWarning(nation_loader_logger())
        << "Failed to load troop catalog for nation defaults";
    return {};
  }
  const QString dir = resolve_data_path("assets/data/nations");
  if (dir.isEmpty()) {
    qCWarning(nation_loader_logger())
        << "Failed to locate assets/data/nations directory";
    return {};
  }
  return load_from_directory(dir);
}

auto NationLoader::load_from_directory(const QString& directory)
    -> std::vector<Nation> {
  std::vector<Nation> nations;

  QDir const dir(directory);
  if (!dir.exists()) {
    qCWarning(nation_loader_logger()) << "Nation directory does not exist" << directory;
    return nations;
  }

  QDirIterator it(
      directory, QStringList{QStringLiteral("*.json")}, QDir::Files | QDir::Readable);
  while (it.hasNext()) {
    const QString file_path = it.next();
    if (auto nation = load_from_file(file_path)) {
      nations.push_back(std::move(*nation));
    }
  }

  return nations;
}

auto NationLoader::load_from_file(const QString& path) -> std::optional<Nation> {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    qCWarning(nation_loader_logger())
        << "Unable to open nation definition" << path << ":" << file.errorString();
    return std::nullopt;
  }

  const QByteArray data = file.readAll();
  QJsonParseError parse_error;
  const QJsonDocument doc = QJsonDocument::fromJson(data, &parse_error);
  if (parse_error.error != QJsonParseError::NoError) {
    qCWarning(nation_loader_logger())
        << "Failed to parse nation" << path << ":" << parse_error.errorString();
    return std::nullopt;
  }

  const QJsonObject root = doc.object();
  Nation nation{};
  const QString id_str = root.value("id").toString();
  if (id_str.isEmpty()) {
    qCWarning(nation_loader_logger()) << "Nation file" << path << "is missing 'id'";
    return std::nullopt;
  }

  auto parsed_id = Game::Systems::nation_id_from_string(id_str.toStdString());
  if (!parsed_id) {
    qCWarning(nation_loader_logger())
        << "Nation file" << path << "has unknown nation id:" << id_str;
    return std::nullopt;
  }
  nation.id = *parsed_id;

  nation.display_name = root.value("display_name").toString(id_str).toStdString();

  const QString building_str =
      root.value("primary_building").toString(QStringLiteral("barracks"));
  if (building_str.trimmed().compare(QStringLiteral("none"), Qt::CaseInsensitive) ==
      0) {
    nation.primary_building = std::nullopt;
  } else {
    auto parsed_building =
        Game::Units::building_type_from_string(building_str.toStdString());
    nation.primary_building =
        parsed_building.value_or(Game::Units::BuildingType::Barracks);
  }
  if (auto formation = parse_formation_type(root.value("formation_type").toString())) {
    nation.formation_type = *formation;
  }
  nation.playable = read_bool(root, "playable", nation.playable);
  nation.has_economy = read_bool(root, "has_economy", nation.has_economy);
  nation.ai_profile =
      read_string(root, "ai_profile", QStringLiteral("standard")).toStdString();
  nation.selectable_in_skirmish =
      read_bool(root, "selectable_in_skirmish", nation.selectable_in_skirmish);

  const QJsonArray troops = ensure_array(root.value(k_nation_troops_key));
  for (const auto& value : troops) {
    const QJsonObject troop_obj = ensure_object(value);
    if (!build_troop_entry(troop_obj, nation)) {
      qCWarning(nation_loader_logger())
          << "Failed to load troop entry in nation" << path;
    }
  }

  return nation;
}

} // namespace Game::Systems
