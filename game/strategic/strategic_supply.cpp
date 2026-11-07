#include "strategic_supply.h"

#include <algorithm>

namespace Game::Strategic {

namespace {

auto besieged(const WorldDefinition& def,
              const WorldState& state,
              const QString& site) -> bool {
  const QString owner = site_controller(def, state, site);
  for (const auto* army : state.armies_at(site)) {
    if (are_hostile(def, army->faction, owner) && army->soldiers() > 0) {
      return true;
    }
  }
  return false;
}

auto region_can_feed(const WorldDefinition& def, const QString& region) -> bool {
  for (const auto* site : def.sites_in(region)) {
    if (site->resources.get(Resource::Food) >= k_forage_food_threshold) {
      return true;
    }
  }
  return false;
}

auto downgrade(SupplyLevel level) -> SupplyLevel {
  switch (level) {
  case SupplyLevel::Supplied:
    return SupplyLevel::Stretched;
  case SupplyLevel::Stretched:
  case SupplyLevel::Foraging:
  case SupplyLevel::CutOff:
    return level;
  }
  return level;
}

} // namespace

auto supply_sources(const WorldDefinition& def,
                    const WorldState& state,
                    const QString& faction) -> QStringList {
  QStringList out;
  for (const auto& site : def.sites) {
    if (!is_settlement(site.kind) && site.kind != SiteKind::Camp) {
      continue;
    }
    if (site.resources.get(Resource::Food) < 2) {
      continue;
    }
    if (!are_friendly(def, faction, site_controller(def, state, site.id))) {
      continue;
    }
    if (besieged(def, state, site.id)) {
      continue;
    }
    out.append(site.id);
  }
  return out;
}

auto evaluate_supply(const WorldDefinition& def,
                     const WorldState& state,
                     const QString& army_id) -> SupplyReport {
  SupplyReport report;
  report.army = army_id;
  const ArmyState* army = state.army(army_id);
  if (army == nullptr) {
    report.reason = QStringLiteral("unknown army");
    return report;
  }
  report.demand = army->supply_demand();
  const QStringList sources = supply_sources(def, state, army->faction);
  const StrategicGraph graph(def, state);
  const TraversalRules rules = TraversalRules::safe_march(army->faction);
  const Path best = graph.nearest_of(army->location, sources, rules);

  const auto reachable = graph.reachable(army->location, rules, k_stretched_days);
  QVector<std::pair<double, QString>> ranked;
  for (const auto& source : sources) {
    const auto it = reachable.find(source);
    if (it != reachable.end()) {
      ranked.append({it->second, source});
    }
  }
  std::sort(ranked.begin(), ranked.end());
  for (const auto& [days, source] : ranked) {
    if (source != (best.found ? best.sites.constLast() : QString())) {
      report.alternative_sources.append(source);
    }
  }

  if (best.found) {
    report.source = best.sites.constLast();
    report.path = best;
    report.capacity = best.sites.size() == 1
                          ? report.demand
                          : best.min_throughput * k_men_per_throughput;
    if (best.days <= k_supplied_days) {
      report.level = SupplyLevel::Supplied;
      report.reason = best.sites.size() == 1
                          ? QStringLiteral("quartered in a friendly depot")
                          : QStringLiteral("%1 days from %2")
                                .arg(best.days, 0, 'f', 1)
                                .arg(def.display_name(report.source));
    } else if (best.days <= k_stretched_days) {
      report.level = SupplyLevel::Stretched;
      report.reason = QStringLiteral("long line: %1 days to %2")
                          .arg(best.days, 0, 'f', 1)
                          .arg(def.display_name(report.source));
    }
    if (report.level != SupplyLevel::CutOff && report.capacity < report.demand) {
      report.level = downgrade(report.level);
      report.reason += QStringLiteral("; the road carries %1 rations for %2 mouths")
                           .arg(report.capacity)
                           .arg(report.demand);
    }
    if (report.level != SupplyLevel::CutOff) {
      return report;
    }
  }

  const Site* here = def.site(army->location);
  if (here != nullptr && region_can_feed(def, here->region)) {
    report.level = SupplyLevel::Foraging;
    report.reason = best.found
                        ? QStringLiteral("too far from friendly depots; living off %1")
                              .arg(def.display_name(here->region))
                        : QStringLiteral("no friendly depot reachable; living off %1")
                              .arg(def.display_name(here->region));
    return report;
  }
  report.level = SupplyLevel::CutOff;
  report.reason = QStringLiteral("no friendly depot reachable and the country is bare");
  return report;
}

auto evaluate_all_supply(const WorldDefinition& def,
                         const WorldState& state) -> QVector<SupplyReport> {
  QVector<SupplyReport> out;
  for (const auto& army : state.armies) {
    out.append(evaluate_supply(def, state, army.id));
  }
  return out;
}

auto isolated_sites(const WorldDefinition& def,
                    const WorldState& state) -> QVector<IsolatedSite> {
  QVector<IsolatedSite> out;
  const StrategicGraph graph(def, state);
  for (const auto& faction : def.factions) {
    if (faction.capital.isEmpty() || def.site(faction.capital) == nullptr) {
      continue;
    }
    if (site_controller(def, state, faction.capital) != faction.id) {
      continue;
    }
    const auto reachable =
        graph.reachable(faction.capital, TraversalRules::safe_march(faction.id));
    for (const auto& site : def.sites) {
      if (!is_settlement(site.kind) ||
          site_controller(def, state, site.id) != faction.id) {
        continue;
      }
      if (reachable.find(site.id) == reachable.end()) {
        out.append({site.id, faction.id, faction.capital});
      }
    }
  }
  return out;
}

auto threatened_routes(const WorldDefinition& def,
                       const WorldState& state,
                       double within_days) -> QVector<RouteThreat> {
  QVector<RouteThreat> out;
  const StrategicGraph graph(def, state);
  std::map<QString, std::map<QString, double>> reach;
  for (const auto& army : state.armies) {
    if (army.soldiers() <= 0) {
      continue;
    }
    reach[army.id] = graph.reachable(
        army.location, TraversalRules::march(army.faction), within_days);
  }
  for (const auto& route : def.routes) {
    const QString owner_a = site_controller(def, state, route.from);
    const QString owner_b = site_controller(def, state, route.to);
    if (owner_a != owner_b || owner_a == QLatin1String(k_neutral_faction)) {
      continue;
    }
    RouteThreat best;
    best.days = within_days + 1.0;
    for (const auto& army : state.armies) {
      if (!are_hostile(def, army.faction, owner_a)) {
        continue;
      }
      const auto it = reach.find(army.id);
      if (it == reach.end()) {
        continue;
      }
      for (const QString& end : {route.from, route.to}) {
        const auto hit = it->second.find(end);
        if (hit != it->second.end() && hit->second < best.days) {
          best = {route.id, owner_a, army.id, hit->second};
        }
      }
    }
    if (!best.route.isEmpty()) {
      out.append(best);
    }
  }
  std::sort(out.begin(), out.end(), [](const RouteThreat& a, const RouteThreat& b) {
    return a.days != b.days ? a.days < b.days : a.route < b.route;
  });
  return out;
}

auto armies_supplied_by(const WorldDefinition& def,
                        const WorldState& state,
                        const QString& site) -> QStringList {
  QStringList out;
  for (const auto& army : state.armies) {
    const SupplyReport report = evaluate_supply(def, state, army.id);
    if (report.source == site) {
      out.append(army.id);
    }
  }
  return out;
}

} // namespace Game::Strategic
