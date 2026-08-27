#include "ai_doctrine_catalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLoggingCategory>

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "../../units/troop_catalog_loader.h"

namespace Game::Systems::AI {

namespace {

Q_LOGGING_CATEGORY(logger, "soi.ai.doctrine")

struct Catalog {
  bool loaded = false;
  std::unordered_map<std::string, TownPlan> town_plans;
  std::unordered_map<std::string, AIDoctrine> doctrines;
};

auto catalog() -> Catalog& {
  static Catalog value;
  return value;
}

auto parse_target(const QString& key, DoctrineTarget& out) -> bool {
  const QString lower = key.trimmed().toLower();
  if (lower == QLatin1String("army") || lower == QLatin1String("units")) {
    out = DoctrineTarget::Army;
    return true;
  }
  if (lower == QLatin1String("barracks") || lower == QLatin1String("production")) {
    out = DoctrineTarget::Barracks;
    return true;
  }
  if (lower == QLatin1String("economy") || lower == QLatin1String("builders")) {
    out = DoctrineTarget::Economy;
    return true;
  }
  if (lower == QLatin1String("commander") || lower == QLatin1String("lord")) {
    out = DoctrineTarget::Commander;
    return true;
  }
  if (lower == QLatin1String("any")) {
    out = DoctrineTarget::Any;
    return true;
  }
  return false;
}

auto read_float(const QJsonObject& object, const char* key, float fallback) -> float {
  const auto value = object.value(QLatin1String(key));
  return value.isDouble() ? static_cast<float>(value.toDouble()) : fallback;
}

auto read_int(const QJsonObject& object, const char* key, int fallback) -> int {
  const auto value = object.value(QLatin1String(key));
  return value.isDouble() ? value.toInt() : fallback;
}

auto read_string(const QJsonObject& object,
                 const char* key,
                 const std::string& fallback) -> std::string {
  const auto value = object.value(QLatin1String(key));
  return value.isString() ? value.toString().toStdString() : fallback;
}

auto read_json_object(const QString& path, QJsonObject& out) -> bool {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    qCWarning(logger) << "cannot open AI doctrine data" << path;
    return false;
  }
  QJsonParseError parse_error{};
  const auto document = QJsonDocument::fromJson(file.readAll(), &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
    qCWarning(logger) << "AI doctrine data" << path
                      << "is not a JSON object:" << parse_error.errorString();
    return false;
  }
  out = document.object();
  return true;
}

void parse_town_plans(const QJsonObject& root,
                      std::unordered_map<std::string, TownPlan>& out) {
  const auto plans = root.value(QLatin1String("plans"));
  if (!plans.isObject()) {
    qCWarning(logger) << "town plan file has no 'plans' object";
    return;
  }
  const auto plans_object = plans.toObject();
  for (auto it = plans_object.begin(); it != plans_object.end(); ++it) {
    if (!it.value().isObject()) {
      qCWarning(logger) << "town plan" << it.key() << "is not an object; skipped";
      continue;
    }
    const auto plan_object = it.value().toObject();
    TownPlan plan;
    plan.id = it.key().toStdString();
    plan.display_name = read_string(plan_object, "display_name", plan.id);

    const auto steps = plan_object.value(QLatin1String("steps"));
    if (!steps.isArray()) {
      qCWarning(logger) << "town plan" << it.key() << "has no 'steps' array; skipped";
      continue;
    }
    for (const auto& step_value : steps.toArray()) {
      if (!step_value.isObject()) {
        continue;
      }
      const auto step_object = step_value.toObject();
      TownPlanStep step;
      step.building = read_string(step_object, "building", {});
      if (step.building.empty()) {
        qCWarning(logger) << "town plan" << it.key() << "has a step with no building";
        continue;
      }
      step.x = read_float(step_object, "x", 0.0F);
      step.z = read_float(step_object, "z", 0.0F);
      plan.steps.push_back(std::move(step));
    }
    if (plan.steps.empty()) {
      qCWarning(logger) << "town plan" << it.key() << "has no usable steps; skipped";
      continue;
    }
    out.emplace(plan.id, std::move(plan));
  }
}

void parse_doctrine_body(const QJsonObject& object,
                         const std::unordered_map<std::string, TownPlan>& plans,
                         AIDoctrine& doctrine) {
  doctrine.strategy = read_string(object, "strategy", doctrine.strategy);
  doctrine.posture = read_string(object, "posture", doctrine.posture);

  if (const auto personality = object.value(QLatin1String("personality"));
      personality.isObject()) {
    const auto personality_object = personality.toObject();
    doctrine.aggression = std::clamp(
        read_float(personality_object, "aggression", doctrine.aggression), 0.0F, 1.0F);
    doctrine.defense = std::clamp(
        read_float(personality_object, "defense", doctrine.defense), 0.0F, 1.0F);
    doctrine.harassment = std::clamp(
        read_float(personality_object, "harassment", doctrine.harassment), 0.0F, 1.0F);
  }

  const std::string plan_id = read_string(object, "town_plan", {});
  if (!plan_id.empty()) {
    const auto found = plans.find(plan_id);
    if (found == plans.end()) {
      qCWarning(logger) << "doctrine names unknown town plan"
                        << QString::fromStdString(plan_id)
                        << "; falling back to the built-in layout";
    } else {
      doctrine.town_plan = &found->second;
    }
  }

  if (const auto recruitment = object.value(QLatin1String("recruitment"));
      recruitment.isObject()) {
    doctrine.recruitment.ranged_share = std::clamp(
        read_float(
            recruitment.toObject(), "ranged_share", doctrine.recruitment.ranged_share),
        0.0F,
        1.0F);
  }

  if (const auto wave = object.value(QLatin1String("wave")); wave.isObject()) {
    const auto wave_object = wave.toObject();
    doctrine.wave.size =
        std::clamp(read_int(wave_object, "size", doctrine.wave.size), 1, 60);
    doctrine.wave.regroup_seconds = std::clamp(
        read_float(wave_object, "regroup_seconds", doctrine.wave.regroup_seconds),
        0.0F,
        300.0F);
    doctrine.wave.spent_fraction = std::clamp(
        read_float(wave_object, "spent_fraction", doctrine.wave.spent_fraction),
        0.0F,
        0.95F);

    if (const auto priority = wave_object.value(QLatin1String("target_priority"));
        priority.isArray()) {
      std::vector<DoctrineTarget> parsed;
      for (const auto& entry : priority.toArray()) {
        DoctrineTarget target{};
        if (entry.isString() && parse_target(entry.toString(), target)) {
          parsed.push_back(target);
        } else if (entry.isString()) {
          qCWarning(logger) << "doctrine names unknown wave target" << entry.toString()
                            << "; ignored";
        }
      }
      if (!parsed.empty()) {

        if (std::find(parsed.begin(), parsed.end(), DoctrineTarget::Any) ==
            parsed.end()) {
          parsed.push_back(DoctrineTarget::Any);
        }
        doctrine.wave.target_priority = std::move(parsed);
      }
    }
  }

  if (const auto garrison = object.value(QLatin1String("garrison"));
      garrison.isObject()) {
    const auto garrison_object = garrison.toObject();
    doctrine.garrison.minimum_units = std::clamp(
        read_int(garrison_object, "minimum_units", doctrine.garrison.minimum_units),
        0,
        40);
    doctrine.garrison.fraction =
        std::clamp(read_float(garrison_object, "fraction", doctrine.garrison.fraction),
                   0.0F,
                   0.95F);
  }
}

} // namespace

auto load_ai_doctrine_catalog(const QString& doctrines_path,
                              const QString& town_plans_path) -> bool {
  Catalog parsed;

  QJsonObject town_root;
  if (!town_plans_path.isEmpty() && read_json_object(town_plans_path, town_root)) {
    parse_town_plans(town_root, parsed.town_plans);
  }

  QJsonObject doctrine_root;
  if (!read_json_object(doctrines_path, doctrine_root)) {
    return false;
  }

  AIDoctrine defaults;
  if (const auto defaults_value = doctrine_root.value(QLatin1String("defaults"));
      defaults_value.isObject()) {
    parse_doctrine_body(defaults_value.toObject(), parsed.town_plans, defaults);
  }

  const auto commanders = doctrine_root.value(QLatin1String("commanders"));
  if (!commanders.isObject()) {
    qCWarning(logger) << "AI doctrine file has no 'commanders' object";
    return false;
  }
  const auto commanders_object = commanders.toObject();
  for (auto it = commanders_object.begin(); it != commanders_object.end(); ++it) {
    if (!it.value().isObject()) {
      qCWarning(logger) << "doctrine" << it.key() << "is not an object; skipped";
      continue;
    }
    AIDoctrine doctrine = defaults;
    doctrine.id = it.key().toStdString();
    parse_doctrine_body(it.value().toObject(), parsed.town_plans, doctrine);
    parsed.doctrines.emplace(doctrine.id, std::move(doctrine));
  }

  if (parsed.doctrines.empty()) {
    qCWarning(logger) << "AI doctrine file named no commanders; keeping built-ins";
    return false;
  }

  parsed.loaded = true;
  catalog() = std::move(parsed);

  for (auto& [id, doctrine] : catalog().doctrines) {
    if (doctrine.town_plan == nullptr) {
      continue;
    }
    const auto found = catalog().town_plans.find(doctrine.town_plan->id);
    doctrine.town_plan = found == catalog().town_plans.end() ? nullptr : &found->second;
  }

  qCInfo(logger) << "loaded" << static_cast<int>(catalog().doctrines.size())
                 << "AI doctrines and" << static_cast<int>(catalog().town_plans.size())
                 << "town plans";
  return true;
}

auto load_default_ai_doctrine_catalog() -> bool {
  const QString doctrines = Game::Units::TroopCatalogLoader::resolve_data_path(
      "assets/data/ai/doctrines.json");
  if (doctrines.isEmpty()) {
    qCInfo(logger) << "no AI doctrine data found; using the built-in doctrines";
    return false;
  }
  const QString town_plans = Game::Units::TroopCatalogLoader::resolve_data_path(
      "assets/data/ai/town_plans.json");
  return load_ai_doctrine_catalog(doctrines, town_plans);
}

void ensure_ai_doctrine_catalog_loaded() {
  static std::once_flag once;
  std::call_once(once, [] { load_default_ai_doctrine_catalog(); });
}

void reset_ai_doctrine_catalog() {
  catalog() = Catalog{};
}

auto authored_doctrine(std::string_view commander_id) -> const AIDoctrine* {
  const auto& value = catalog();
  if (!value.loaded) {
    return nullptr;
  }
  const auto found = value.doctrines.find(std::string(commander_id));
  return found == value.doctrines.end() ? nullptr : &found->second;
}

auto authored_town_plan(std::string_view plan_id) -> const TownPlan* {
  const auto& value = catalog();
  const auto found = value.town_plans.find(std::string(plan_id));
  return found == value.town_plans.end() ? nullptr : &found->second;
}

auto ai_doctrine_catalog_loaded() -> bool {
  return catalog().loaded;
}

} // namespace Game::Systems::AI
