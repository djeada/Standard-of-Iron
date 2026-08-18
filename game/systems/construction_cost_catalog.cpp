#include "construction_cost_catalog.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>

#include "../units/troop_catalog_loader.h"
#include "wall_network_service.h"

namespace Game::Systems {

namespace {

Q_LOGGING_CATEGORY(logger, "soi.construction.catalog")

struct CatalogEntry {
  bool has_costs = false;
  ResourceAmounts costs;
  bool has_build_time = false;
  float build_time = 0.0F;
  bool has_dismantle = false;
  DismantleInfo dismantle;
};

auto loaded_catalog() -> std::unordered_map<std::string, CatalogEntry>& {
  static std::unordered_map<std::string, CatalogEntry> catalog;
  return catalog;
}

auto builtin_cost_info(std::string_view item_type) -> ConstructionCostInfo;
auto builtin_build_time(std::string_view item_type) -> float;

auto builtin_dismantle_info(std::string_view item_type) -> DismantleInfo {
  DismantleInfo info;
  if (item_type == "barracks") {
    info.allowed = false;
  }
  return info;
}

} // namespace

auto construction_cost_info(std::string_view item_type) -> ConstructionCostInfo {
  const auto& catalog = loaded_catalog();
  if (const auto it = catalog.find(std::string(item_type));
      it != catalog.end() && it->second.has_costs) {
    return ConstructionCostInfo{.resource_costs = it->second.costs};
  }
  return builtin_cost_info(item_type);
}

auto construction_build_time(std::string_view item_type) -> float {
  const auto& catalog = loaded_catalog();
  if (const auto it = catalog.find(std::string(item_type));
      it != catalog.end() && it->second.has_build_time) {
    return it->second.build_time;
  }
  return builtin_build_time(item_type);
}

auto dismantle_info(std::string_view item_type) -> DismantleInfo {
  const auto& catalog = loaded_catalog();
  if (const auto it = catalog.find(std::string(item_type));
      it != catalog.end() && it->second.has_dismantle) {
    return it->second.dismantle;
  }
  return builtin_dismantle_info(item_type);
}

auto dismantle_refund(std::string_view item_type) -> ResourceAmounts {
  const auto info = dismantle_info(item_type);
  ResourceAmounts refund;
  if (!info.allowed) {
    return refund;
  }

  const float fraction = std::clamp(info.refund_fraction, 0.0F, 1.0F);
  const auto costs = construction_cost_info(item_type).resource_costs;
  for (const auto resource_type : k_all_resource_types) {
    const int paid = costs.get(resource_type);
    if (paid <= 0) {
      continue;
    }
    const int given = static_cast<int>(std::floor(static_cast<float>(paid) * fraction));
    refund.set(resource_type, std::clamp(given, 0, paid));
  }
  return refund;
}

auto dismantle_duration(std::string_view item_type) -> float {
  return std::max(0.5F,
                  construction_build_time(item_type) * k_dismantle_speed_multiplier);
}

auto load_construction_catalog(const QString& path) -> bool {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    qCWarning(logger) << "cannot open construction catalog" << path;
    return false;
  }
  QJsonParseError parse_error{};
  const auto document = QJsonDocument::fromJson(file.readAll(), &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
    qCWarning(logger) << "construction catalog" << path
                      << "is not a JSON object:" << parse_error.errorString();
    return false;
  }
  const auto items = document.object().value(QLatin1String("items"));
  if (!items.isObject()) {
    qCWarning(logger) << "construction catalog" << path << "has no 'items' object";
    return false;
  }

  std::unordered_map<std::string, CatalogEntry> parsed;
  const auto items_object = items.toObject();
  for (auto it = items_object.begin(); it != items_object.end(); ++it) {
    if (!it.value().isObject()) {
      qCWarning(logger) << "construction catalog item" << it.key()
                        << "is not an object";
      return false;
    }
    const auto item = it.value().toObject();
    CatalogEntry entry;
    if (const auto costs = item.value(QLatin1String("costs")); costs.isObject()) {
      entry.has_costs = true;
      const auto costs_object = costs.toObject();
      for (auto cost = costs_object.begin(); cost != costs_object.end(); ++cost) {
        ResourceType type{};
        if (!resource_type_from_key(cost.key(), type) || !cost.value().isDouble()) {
          qCWarning(logger) << "construction catalog item" << it.key()
                            << "has an unknown or non-numeric cost" << cost.key();
          return false;
        }
        entry.costs.set(type, cost.value().toInt());
      }
    }
    if (const auto dismantle = item.value(QLatin1String("dismantle"));
        dismantle.isObject()) {
      entry.has_dismantle = true;
      const auto dismantle_object = dismantle.toObject();
      if (const auto allowed = dismantle_object.value(QLatin1String("allowed"));
          allowed.isBool()) {
        entry.dismantle.allowed = allowed.toBool();
      }
      if (const auto refund = dismantle_object.value(QLatin1String("refund"));
          refund.isDouble()) {
        const auto fraction = static_cast<float>(refund.toDouble());
        if (fraction < 0.0F || fraction > 1.0F) {
          qCWarning(logger) << "construction catalog item" << it.key()
                            << "has a dismantle refund outside 0..1";
          return false;
        }
        entry.dismantle.refund_fraction = fraction;
      }
    }
    if (const auto build_time = item.value(QLatin1String("build_time"));
        build_time.isDouble()) {
      entry.has_build_time = true;
      entry.build_time = static_cast<float>(build_time.toDouble());
      if (entry.build_time <= 0.0F) {
        qCWarning(logger) << "construction catalog item" << it.key()
                          << "has a non-positive build_time";
        return false;
      }
    }
    parsed.emplace(it.key().toStdString(), entry);
  }
  loaded_catalog() = std::move(parsed);
  return true;
}

auto load_default_construction_catalog() -> bool {
  const QString path = Game::Units::TroopCatalogLoader::resolve_data_path(
      "assets/data/construction/catalog.json");
  if (path.isEmpty()) {
    qCWarning(logger) << "construction catalog not found; using the built-in table";
    return false;
  }
  return load_construction_catalog(path);
}

void reset_construction_catalog() {
  loaded_catalog().clear();
}

namespace {

auto builtin_cost_info(std::string_view item_type) -> ConstructionCostInfo {
  ConstructionCostInfo info;

  if (item_type == "catapult") {
    info.resource_costs.set(ResourceType::Wood, 60);
    info.resource_costs.set(ResourceType::Iron, 25);
    return info;
  }
  if (item_type == "ballista") {
    info.resource_costs.set(ResourceType::Wood, 50);
    info.resource_costs.set(ResourceType::Iron, 30);
    return info;
  }
  if (item_type == "defense_tower") {
    info.resource_costs.set(ResourceType::Wood, 60);
    info.resource_costs.set(ResourceType::Stone, 80);
    return info;
  }
  if (item_type == "home") {
    info.resource_costs.set(ResourceType::Wood, 50);
    info.resource_costs.set(ResourceType::Stone, 15);
    return info;
  }
  if (item_type == "barracks") {
    info.resource_costs.set(ResourceType::Wood, 100);
    info.resource_costs.set(ResourceType::Stone, 60);
    return info;
  }
  if (item_type == "wall_segment") {
    info.resource_costs.set(ResourceType::Wood,
                            WallNetworkService::k_wall_segment_wood_cost);
    return info;
  }
  if (item_type == "wall_gate") {
    info.resource_costs.set(ResourceType::Wood,
                            WallNetworkService::k_wall_gate_wood_cost);
    info.resource_costs.set(ResourceType::Stone, 20);
    return info;
  }
  if (item_type == "temple") {
    info.resource_costs.set(ResourceType::Wood, 40);
    info.resource_costs.set(ResourceType::Stone, 90);
    info.resource_costs.set(ResourceType::Gold, 30);
    return info;
  }
  if (item_type == "marketplace") {
    info.resource_costs.set(ResourceType::Wood, 60);
    info.resource_costs.set(ResourceType::Stone, 40);
    info.resource_costs.set(ResourceType::Gold, 50);
    return info;
  }

  return info;
}

auto builtin_build_time(std::string_view item_type) -> float {
  if (item_type == "temple") {
    return 18.0F;
  }
  if (item_type == "catapult") {
    return 15.0F;
  }
  if (item_type == "ballista") {
    return 12.0F;
  }
  if (item_type == "defense_tower") {
    return 20.0F;
  }
  if (item_type == "wall_segment") {
    return 8.0F;
  }
  if (item_type == "wall_gate") {
    return 12.0F;
  }
  if (item_type == "cut_tree" || item_type == "collect" ||
      item_type == "collect_stone" || item_type == "collect_iron_ore") {
    return 6.0F;
  }
  return 10.0F;
}

} // namespace

} // namespace Game::Systems
