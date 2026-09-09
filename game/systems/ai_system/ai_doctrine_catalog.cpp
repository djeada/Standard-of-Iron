#include "ai_doctrine_catalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLoggingCategory>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
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
    plan.silhouette_steps = std::max(
        0, static_cast<int>(read_float(plan_object, "silhouette_steps", 0.0F)));

    const auto steps = plan_object.value(QLatin1String("steps"));
    if (!steps.isArray()) {
      qCWarning(logger) << "town plan" << it.key() << "has no 'steps' array; skipped";
      continue;
    }
    for (const auto step_value : steps.toArray()) {
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
      step.rotation = read_float(step_object, "rotation", 0.0F);
      if (std::hypot(step.x, step.z) < TownPlan::k_anchor_clearance) {

        qCWarning(logger) << "town plan" << it.key() << "puts a"
                          << QString::fromStdString(step.building) << "at" << step.x
                          << step.z << "inside the base anchor; it will never be built";
      }
      plan.steps.push_back(std::move(step));
    }
    if (plan.steps.empty()) {
      qCWarning(logger) << "town plan" << it.key() << "has no usable steps; skipped";
      continue;
    }
    plan.silhouette_steps =
        std::min(plan.silhouette_steps, static_cast<int>(plan.steps.size()));
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

  doctrine.formation = read_string(object, "formation", doctrine.formation);

  if (const auto recruitment = object.value(QLatin1String("recruitment"));
      recruitment.isObject()) {
    const auto recruitment_object = recruitment.toObject();
    doctrine.recruitment.ranged_share = std::clamp(
        read_float(
            recruitment_object, "ranged_share", doctrine.recruitment.ranged_share),
        0.0F,
        1.0F);
    doctrine.recruitment.cavalry_share = std::clamp(
        read_float(
            recruitment_object, "cavalry_share", doctrine.recruitment.cavalry_share),
        0.0F,
        1.0F);
    doctrine.recruitment.siege_share = std::clamp(
        read_float(recruitment_object, "siege_share", doctrine.recruitment.siege_share),
        0.0F,
        1.0F);

    if (const auto preferred = recruitment_object.value(QLatin1String("preferred"));
        preferred.isArray()) {
      doctrine.recruitment.preferred.clear();
      for (const auto entry : preferred.toArray()) {
        if (entry.isString()) {
          doctrine.recruitment.preferred.push_back(entry.toString().toStdString());
        }
      }
    }
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
      for (const auto entry : priority.toArray()) {
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

auto TownPlan::step_count(std::string_view building) const -> int {
  int count = 0;
  for (const auto& step : steps) {
    if (step.building == building) {
      ++count;
    }
  }
  return count;
}

auto TownPlan::engine_step_count() const -> int {
  return step_count("catapult") + step_count("ballista");
}

auto TownPlan::wall_step_count() const -> int {
  int count = 0;
  for (const auto& step : steps) {
    if (step.building == "wall_segment" || step.building == "wall_gate") {
      ++count;
    }
  }
  return count;
}

auto TownPlan::tower_step_count() const -> int {
  return step_count("defense_tower");
}

auto TownPlan::gate_step_count() const -> int {
  return step_count("wall_gate");
}

namespace {

constexpr int k_compass_sectors = 24;

auto is_wall_step(const TownPlanStep& step) -> bool {
  return step.building == "wall_segment" || step.building == "wall_gate";
}

auto is_fortification_step(const TownPlanStep& step) -> bool {
  return is_wall_step(step) || step.building == "defense_tower";
}

auto sector_of(float x, float z) -> int {
  const float angle = std::atan2(x, -z);
  const float turn = (angle + 3.14159265F) / (2.0F * 3.14159265F);
  return std::clamp(
      static_cast<int>(turn * k_compass_sectors), 0, k_compass_sectors - 1);
}

} // namespace

auto settlement_form_name(SettlementForm form) -> const char* {
  switch (form) {
  case SettlementForm::OpenCamp:
    return "open_camp";
  case SettlementForm::ClosedFort:
    return "closed_fort";
  case SettlementForm::RingTown:
    return "ring_town";
  }
  return "open_camp";
}

auto TownPlan::compass_coverage(const std::vector<TownPlanOffset>& offsets) -> float {
  std::array<bool, k_compass_sectors> covered{};
  for (const auto& offset : offsets) {
    covered[static_cast<std::size_t>(sector_of(offset.x, offset.z))] = true;
  }
  int count = 0;
  for (const bool hit : covered) {
    count += hit ? 1 : 0;
  }
  return static_cast<float>(count) / static_cast<float>(k_compass_sectors);
}

namespace {

auto outer_wall_radii(const std::vector<TownPlanStep>& steps)
    -> std::array<float, k_compass_sectors> {
  std::array<float, k_compass_sectors> outer_radius{};
  for (const auto& step : steps) {
    if (!is_wall_step(step)) {
      continue;
    }
    auto& radius = outer_radius[static_cast<std::size_t>(sector_of(step.x, step.z))];
    radius = std::max(radius, std::hypot(step.x, step.z));
  }
  return outer_radius;
}

} // namespace

auto TownPlan::form() const -> SettlementForm {

  const auto outer_radius = outer_wall_radii(steps);
  std::vector<TownPlanOffset> walls;
  for (const auto& step : steps) {
    if (is_wall_step(step)) {
      walls.push_back({step.x, step.z});
    }
  }
  constexpr float k_closed_coverage = 0.85F;
  if (walls.empty() || compass_coverage(walls) < k_closed_coverage) {
    return SettlementForm::OpenCamp;
  }
  float nearest = std::numeric_limits<float>::infinity();
  float farthest = 0.0F;
  float sum = 0.0F;
  int sectors = 0;
  for (const float radius : outer_radius) {
    if (radius <= 0.0F) {
      continue;
    }
    nearest = std::min(nearest, radius);
    farthest = std::max(farthest, radius);
    sum += radius;
    ++sectors;
  }

  constexpr float k_ring_radial_swing = 0.14F;
  const float mean = sum / static_cast<float>(std::max(1, sectors));
  const bool round = mean > 0.0F && (farthest - nearest) / mean < k_ring_radial_swing;
  return round ? SettlementForm::RingTown : SettlementForm::ClosedFort;
}

auto TownPlan::front_gate() const -> const TownPlanStep* {
  const TownPlanStep* gate = nullptr;
  for (const auto& step : steps) {
    if (step.building == "wall_gate" && (gate == nullptr || step.z < gate->z)) {
      gate = &step;
    }
  }
  return gate;
}

auto TownPlan::front_line_z() const -> float {
  if (const auto* gate = front_gate(); gate != nullptr) {
    return gate->z;
  }
  float fortified = std::numeric_limits<float>::infinity();
  float any = std::numeric_limits<float>::infinity();
  for (const auto& step : steps) {
    any = std::min(any, step.z);
    if (is_fortification_step(step)) {
      fortified = std::min(fortified, step.z);
    }
  }
  if (std::isfinite(fortified)) {
    return fortified;
  }
  return std::isfinite(any) ? any : -k_anchor_clearance;
}

auto TownPlan::muster_offset(MusterSide side) const -> TownPlanOffset {
  const auto* gate = front_gate();
  const float axis_x = gate != nullptr ? gate->x : 0.0F;
  const float front = front_line_z();

  constexpr float k_outside_reach = 7.0F;
  if (side == MusterSide::Outside) {
    return {axis_x, front - k_outside_reach};
  }

  constexpr float k_behind_front = 3.0F;
  constexpr float k_inner_margin = 1.5F;
  constexpr float k_scan_step = 0.5F;
  const float innermost = -(k_anchor_clearance + k_inner_margin);
  const float outermost = std::min(front + k_behind_front, innermost);

  TownPlanOffset best{axis_x, innermost};
  float best_clearance = -1.0F;

  for (const float side : {0.0F, -3.0F, 3.0F, -6.0F, 6.0F}) {
    const float x = axis_x + side;
    for (float z = outermost; z <= innermost + 1.0e-3F; z += k_scan_step) {
      if (std::hypot(x, z) < k_anchor_clearance + k_inner_margin) {
        continue;
      }
      float clearance = std::numeric_limits<float>::infinity();
      for (const auto& step : steps) {
        clearance = std::min(clearance, std::hypot(step.x - x, step.z - z));
      }

      const float scored = clearance - std::abs(side) * 0.15F;
      if (scored > best_clearance + 1.0e-3F) {
        best_clearance = scored;
        best = {x, z};
      }
    }
  }

  constexpr float k_yard_clearance = 2.5F;
  if (best_clearance >= k_yard_clearance) {
    return best;
  }
  const auto outer_radius = outer_wall_radii(steps);
  float wall_front = 0.0F;
  for (const auto& step : steps) {
    if (is_wall_step(step)) {
      wall_front = std::min(wall_front, step.z);
    }
  }
  constexpr float k_inside_wall = 2.5F;
  constexpr float k_grid = 1.0F;
  for (float z = wall_front + k_inside_wall; z <= innermost + 1.0e-3F; z += k_grid) {
    for (float x = -30.0F; x <= 30.0F; x += k_grid) {
      const float radius = std::hypot(x, z);
      if (radius < k_anchor_clearance + k_inner_margin) {
        continue;
      }
      const float trace = outer_radius[static_cast<std::size_t>(sector_of(x, z))];
      if (trace <= 0.0F || radius > trace - k_inside_wall) {
        continue;
      }
      float clearance = std::numeric_limits<float>::infinity();
      for (const auto& step : steps) {
        clearance = std::min(clearance, std::hypot(step.x - x, step.z - z));
      }
      const float scored = std::min(clearance, 5.0F) - std::abs(x - axis_x) * 0.05F;
      if (scored > best_clearance + 1.0e-3F) {
        best_clearance = scored;
        best = {x, z};
      }
    }
  }
  return best;
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
