#include "strategic_validation.h"

#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <tuple>
#include <utility>

#include "strategic_encounter.h"
#include "strategic_events.h"
#include "strategic_graph.h"
#include "strategic_supply.h"

namespace Game::Strategic {

namespace {

constexpr double k_duplicate_site_km = 3.0;
constexpr double k_sea_sample_km = 12.0;
constexpr double k_border_touch_uv = 0.01;
constexpr double k_coastal_tolerance_factor = 2.0;

auto normalise_resource(QString path) -> QString {
  if (path.startsWith(QStringLiteral(":/"))) {
    path.remove(0, 2);
  } else if (path.startsWith(QStringLiteral("qrc:/"))) {
    path.remove(0, 5);
  }
  return path;
}

struct Checker {
  const WorldDefinition& def;
  const ValidationContext& ctx;
  ValidationReport& report;

  void error(const QString& code,
             const QString& entity,
             const QString& message,
             const QString& related = {}) {
    report.add(Severity::Error, code, entity, message, related);
  }
  void warning(const QString& code,
               const QString& entity,
               const QString& message,
               const QString& related = {}) {
    report.add(Severity::Warning, code, entity, message, related);
  }
  void info(const QString& code,
            const QString& entity,
            const QString& message,
            const QString& related = {}) {
    report.add(Severity::Info, code, entity, message, related);
  }

  [[nodiscard]] auto known_faction(const QString& id) const -> bool {
    return id == QLatin1String(k_neutral_faction) || def.faction(id) != nullptr;
  }

  [[nodiscard]] auto on_land(const LonLat& point, double tolerance) const -> bool {
    const QPointF uv = uv_of(def.projection, point);
    for (const auto& [id, shape] : ctx.geometry->shapes()) {
      if (ctx.geometry->contains(id, uv)) {
        return true;
      }
    }
    for (const auto& [id, shape] : ctx.geometry->shapes()) {
      if (uv.x() < shape.min_u - tolerance || uv.x() > shape.max_u + tolerance ||
          uv.y() < shape.min_v - tolerance || uv.y() > shape.max_v + tolerance) {
        continue;
      }
      if (ctx.geometry->distance_to(id, uv) <= tolerance) {
        return true;
      }
    }
    return false;
  }

  void check_load_issues() {
    for (const auto& issue : def.load_issues) {
      error(QStringLiteral("load_issue"), issue.entity, issue.message);
    }
  }

  void check_ids() {
    static const QRegularExpression k_id_pattern(QStringLiteral("^[a-z][a-z0-9_]*$"));
    std::map<QString, QString> owners;
    const auto claim = [&](const QString& id, const QString& kind) {
      if (id.isEmpty()) {
        error(QStringLiteral("missing_id"),
              {},
              QStringLiteral("a %1 has no id").arg(kind));
        return;
      }
      if (!k_id_pattern.match(id).hasMatch()) {
        warning(QStringLiteral("id_format"),
                id,
                QStringLiteral("%1 id '%2' should be lower_snake_case").arg(kind, id));
      }
      const auto [it, inserted] = owners.emplace(id, kind);
      if (!inserted) {
        error(QStringLiteral("duplicate_id"),
              id,
              QStringLiteral("id '%1' is used by a %2 and a %3")
                  .arg(id, it->second, kind));
      }
    };
    if (def.id.isEmpty()) {
      error(QStringLiteral("missing_id"), {}, QStringLiteral("the world has no id"));
    }
    QSet<QString> faction_ids;
    for (const auto& faction : def.factions) {
      if (faction.id.isEmpty()) {
        error(QStringLiteral("missing_id"), {}, QStringLiteral("a faction has no id"));
      } else if (faction_ids.contains(faction.id)) {
        error(QStringLiteral("duplicate_id"),
              faction.id,
              QStringLiteral("faction '%1' is declared twice").arg(faction.id));
      }
      faction_ids.insert(faction.id);
    }
    for (const auto& region : def.regions) {
      claim(region.id, QStringLiteral("region"));
      for (const auto& district : region.districts) {
        claim(district.id, QStringLiteral("district"));
      }
    }
    for (const auto& site : def.sites) {
      claim(site.id, QStringLiteral("site"));
    }
    for (const auto& route : def.routes) {
      claim(route.id, QStringLiteral("route"));
    }
    for (const auto& commander : def.commanders) {
      claim(commander.id, QStringLiteral("commander"));
    }
    for (const auto& army : def.initial_state.armies) {
      claim(army.id, QStringLiteral("army"));
    }
    for (const auto& event : def.events) {
      claim(event.id, QStringLiteral("event"));
      for (const auto& effect : event.effects) {
        if (effect.op == EffectOp::CreateArmy) {
          claim(effect.target, QStringLiteral("army"));
        }
      }
    }
  }

  void check_factions() {
    if (def.factions.isEmpty()) {
      error(QStringLiteral("no_factions"),
            def.id,
            QStringLiteral("the world declares no factions"));
    }
    if (!def.player_faction.isEmpty() && def.faction(def.player_faction) == nullptr) {
      error(QStringLiteral("unknown_faction"),
            def.id,
            QStringLiteral("player_faction '%1' is not declared")
                .arg(def.player_faction));
    }
    for (const auto& faction : def.factions) {
      if (faction.id == QLatin1String(k_neutral_faction)) {
        error(QStringLiteral("reserved_faction"),
              faction.id,
              QStringLiteral("'neutral' is implicit and must not be declared"));
      }
      if (!faction.capital.isEmpty() && def.site(faction.capital) == nullptr) {
        error(QStringLiteral("faction_capital_unknown"),
              faction.id,
              QStringLiteral("capital '%1' is not a site").arg(faction.capital));
      }
      for (const auto& ally : faction.allies) {
        if (def.faction(ally) == nullptr) {
          error(QStringLiteral("unknown_faction"),
                faction.id,
                QStringLiteral("ally '%1' is not declared").arg(ally));
        }
      }
      static const QRegularExpression k_colour(QStringLiteral("^#[0-9a-fA-F]{6}$"));
      if (!faction.color.isEmpty() && !k_colour.match(faction.color).hasMatch()) {
        warning(QStringLiteral("faction_color_invalid"),
                faction.id,
                QStringLiteral("colour '%1' is not #rrggbb").arg(faction.color));
      }
    }
  }

  void check_regions() {
    for (const auto& region : def.regions) {
      if (region.name.isEmpty()) {
        warning(QStringLiteral("missing_name"),
                region.id,
                QStringLiteral("region has no name"));
      }
      if (region.importance < 1 || region.importance > k_importance_max) {
        error(QStringLiteral("value_out_of_range"),
              region.id,
              QStringLiteral("importance %1 is outside 1..%2")
                  .arg(region.importance)
                  .arg(k_importance_max));
      }
      if (def.sites_in(region.id).isEmpty()) {
        warning(
            QStringLiteral("region_without_sites"),
            region.id,
            QStringLiteral("%1 has no sites, so nothing can be held or supplied there")
                .arg(region.name));
      }
      for (const auto& other_id : region.adjacent) {
        const Region* other = def.region(other_id);
        if (other == nullptr) {
          error(QStringLiteral("region_adjacent_unknown"),
                region.id,
                QStringLiteral("adjacent region '%1' does not exist").arg(other_id));
          continue;
        }
        if (!other->adjacent.contains(region.id)) {
          warning(QStringLiteral("region_adjacency_asymmetric"),
                  region.id,
                  QStringLiteral("%1 lists %2 as adjacent but not the other way round")
                      .arg(region.name, other->name),
                  other_id);
        }
      }
      for (const auto& district : region.districts) {
        if (district.outline.size() < 3) {
          error(QStringLiteral("district_outline"),
                district.id,
                QStringLiteral("district %1 needs an outline of at least three points")
                    .arg(district.name));
        }
      }
      if (ctx.geometry != nullptr && ctx.geometry->shape(region.id) == nullptr) {
        warning(QStringLiteral("region_geometry_missing"),
                region.id,
                QStringLiteral("%1 has no province shape in the campaign map")
                    .arg(region.name));
      }
    }
    if (ctx.geometry != nullptr) {
      for (const auto& id : ctx.geometry->region_ids()) {
        if (def.region(id) == nullptr) {
          warning(QStringLiteral("province_unmodelled"),
                  id,
                  QStringLiteral("campaign map province '%1' has no strategic region")
                      .arg(id));
        }
      }
    }
  }

  void check_region_links() {
    std::map<std::pair<QString, QString>, std::pair<int, int>> crossings;
    for (const auto& route : def.routes) {
      const Site* a = def.site(route.from);
      const Site* b = def.site(route.to);
      if (a == nullptr || b == nullptr || a->region == b->region) {
        continue;
      }
      const auto key = std::minmax(a->region, b->region);
      auto& [land, sea] = crossings[{key.first, key.second}];
      (route.kind == RouteKind::SeaLane ? sea : land) += 1;
      if (route.kind == RouteKind::SeaLane) {
        continue;
      }
      const Region* ra = def.region(a->region);
      if (ra != nullptr && def.region(b->region) != nullptr &&
          !ra->adjacent.contains(b->region)) {
        warning(QStringLiteral("route_regions_not_adjacent"),
                route.id,
                QStringLiteral(
                    "%1 runs overland from %2 to %3, which are not declared adjacent")
                    .arg(def.display_name(route.id), a->region, b->region));
      }
    }
    for (const auto& region : def.regions) {
      for (const auto& other_id : region.adjacent) {
        if (other_id < region.id || def.region(other_id) == nullptr) {
          continue;
        }
        const auto key = std::make_pair(region.id, other_id);
        const auto it = crossings.find(key);
        if (it == crossings.end()) {
          warning(QStringLiteral("adjacent_without_route"),
                  region.id,
                  QStringLiteral("%1 and %2 border each other but no route links them")
                      .arg(region.name, def.display_name(other_id)),
                  other_id);
          continue;
        }
        if (ctx.geometry != nullptr && it->second.first > 0) {
          const double gap = ctx.geometry->border_gap(region.id, other_id);
          if (std::isfinite(gap) && gap > k_border_touch_uv) {
            warning(
                QStringLiteral("adjacency_not_on_map"),
                region.id,
                QStringLiteral(
                    "%1 and %2 are linked overland but their provinces do not touch "
                    "on the campaign map (gap %3)")
                    .arg(region.name, def.display_name(other_id))
                    .arg(gap, 0, 'f', 3),
                other_id);
          }
        }
      }
    }
  }

  void check_site_geometry(const Site& site) {
    const QPointF uv = uv_of(def.projection, site.position);
    if (!ctx.geometry->contains(site.region, uv)) {
      const double tolerance =
          ctx.region_tolerance_uv * ((site.has_port() || site.terrain == Terrain::Coast)
                                         ? k_coastal_tolerance_factor
                                         : 1.0);
      const double distance = ctx.geometry->distance_to(site.region, uv);
      if (distance > tolerance) {
        const QString actual = ctx.geometry->region_at(uv);
        error(QStringLiteral("site_outside_region"),
              site.id,
              actual.isEmpty()
                  ? QStringLiteral("%1 lies %2 map units outside %3, in the sea")
                        .arg(site.name)
                        .arg(distance, 0, 'f', 3)
                        .arg(site.region)
                  : QStringLiteral("%1 lies in %2 on the campaign map, not %3")
                        .arg(site.name, actual, site.region),
              actual);
      }
    }
  }

  void check_sites() {
    QHash<QString, QString> mission_owner;
    for (int i = 0; i < def.sites.size(); ++i) {
      const Site& site = def.sites[i];
      if (site.name.isEmpty()) {
        warning(QStringLiteral("missing_name"),
                site.id,
                QStringLiteral("site has no name"));
      }
      const Region* region = def.region(site.region);
      if (region == nullptr) {
        error(QStringLiteral("site_region_unknown"),
              site.id,
              QStringLiteral("%1 names region '%2', which does not exist")
                  .arg(site.name, site.region));
      }
      if (!site.district.isEmpty()) {
        const District* district = nullptr;
        if (region != nullptr) {
          for (const auto& entry : region->districts) {
            if (entry.id == site.district) {
              district = &entry;
            }
          }
        }
        if (district == nullptr) {
          error(QStringLiteral("site_district_unknown"),
                site.id,
                QStringLiteral("%1 names district '%2', which is not part of %3")
                    .arg(site.name, site.district, site.region));
        } else if (district->outline.size() >= 3 &&
                   !point_in_polygon(district->outline, site.position)) {
          error(QStringLiteral("site_outside_district"),
                site.id,
                QStringLiteral("%1 lies outside the outline of %2")
                    .arg(site.name, district->name));
        }
      }
      if (!def.projection.contains(site.position)) {
        error(QStringLiteral("site_out_of_bounds"),
              site.id,
              QStringLiteral("%1 at %2, %3 is outside the campaign map")
                  .arg(site.name)
                  .arg(site.position.lon)
                  .arg(site.position.lat));
      } else if (ctx.geometry != nullptr && region != nullptr &&
                 ctx.geometry->shape(site.region) != nullptr) {
        check_site_geometry(site);
      }
      if (site.fortification < 0 || site.fortification > k_fortification_max) {
        error(QStringLiteral("value_out_of_range"),
              site.id,
              QStringLiteral("fortification %1 is outside 0..%2")
                  .arg(site.fortification)
                  .arg(k_fortification_max));
      }
      for (const Resource resource : k_all_resources) {
        const int value = site.resources.get(resource);
        if (value < 0 || value > k_resource_max) {
          error(QStringLiteral("value_out_of_range"),
                site.id,
                QStringLiteral("%1 output %2 is outside 0..%3")
                    .arg(to_string(resource))
                    .arg(value)
                    .arg(k_resource_max));
        }
      }
      if (is_settlement(site.kind) && site.resources.total() == 0) {
        warning(
            QStringLiteral("settlement_without_resources"),
            site.id,
            QStringLiteral("%1 is a settlement but produces nothing").arg(site.name));
      }
      for (int j = i + 1; j < def.sites.size(); ++j) {
        const Site& other = def.sites[j];
        const double km = great_circle_km(site.position, other.position);
        if (km < k_duplicate_site_km) {
          warning(QStringLiteral("sites_overlap"),
                  site.id,
                  QStringLiteral("%1 and %2 are only %3 km apart")
                      .arg(site.name, other.name)
                      .arg(km, 0, 'f', 1),
                  other.id);
        }
      }
      check_battlefield(site.id, site.name, site.battlefield);
      if (!site.battlefield.mission.isEmpty()) {
        if (mission_owner.contains(site.battlefield.mission)) {
          error(QStringLiteral("mission_site_duplicate"),
                site.id,
                QStringLiteral("mission '%1' is already placed at %2")
                    .arg(site.battlefield.mission,
                         mission_owner.value(site.battlefield.mission)),
                mission_owner.value(site.battlefield.mission));
        }
        mission_owner.insert(site.battlefield.mission, site.id);
      }
    }
  }

  void check_battlefield(const QString& entity,
                         const QString& name,
                         const BattlefieldLink& link) {
    if (!link.map.isEmpty() && ctx.resource_exists &&
        !ctx.resource_exists(normalise_resource(link.map))) {
      error(QStringLiteral("battlefield_map_missing"),
            entity,
            QStringLiteral("%1 links battlefield map '%2', which does not exist")
                .arg(name, link.map));
    }
    if (!link.mission.isEmpty() && ctx.mission_ids.has_value() &&
        !ctx.mission_ids->contains(link.mission)) {
      error(QStringLiteral("battlefield_mission_unknown"),
            entity,
            QStringLiteral("%1 links mission '%2', which does not exist")
                .arg(name, link.mission));
    }
    if (!link.mission.isEmpty() && !link.map.isEmpty() &&
        ctx.mission_maps.contains(link.mission)) {
      const QString expected = normalise_resource(ctx.mission_maps.value(link.mission));
      if (!expected.isEmpty() && expected != normalise_resource(link.map)) {
        error(QStringLiteral("battlefield_map_mismatch"),
              entity,
              QStringLiteral("%1 links map '%2' but mission '%3' is played on '%4'")
                  .arg(name, link.map, link.mission, expected));
      }
    }
    if (!link.preset.isEmpty() && !known_generator_presets().contains(link.preset)) {
      error(QStringLiteral("battlefield_preset_unknown"),
            entity,
            QStringLiteral("%1 names generator preset '%2', which does not exist")
                .arg(name, link.preset));
    }
  }

  void check_route_over_sea(const Route& route) {
    const QVector<LonLat> line = def.route_polyline(route);
    const double tolerance =
        ctx.region_tolerance_uv *
        ((route.kind == RouteKind::Coastal) ? k_coastal_tolerance_factor : 1.0);
    for (int i = 1; i < line.size(); ++i) {
      const double km = great_circle_km(line[i - 1], line[i]);
      const int steps = std::max(1, static_cast<int>(std::ceil(km / k_sea_sample_km)));
      for (int s = 1; s < steps; ++s) {
        const double t = static_cast<double>(s) / steps;
        const LonLat sample{line[i - 1].lon + ((line[i].lon - line[i - 1].lon) * t),
                            line[i - 1].lat + ((line[i].lat - line[i - 1].lat) * t)};
        if (!on_land(sample, tolerance)) {
          error(
              QStringLiteral("route_crosses_sea"),
              route.id,
              QStringLiteral(
                  "%1 is a %2 but leaves the land near %3, %4 – add waypoints or make "
                  "it a sea lane")
                  .arg(def.display_name(route.id), to_string(route.kind))
                  .arg(sample.lon, 0, 'f', 2)
                  .arg(sample.lat, 0, 'f', 2));
          return;
        }
      }
    }
  }

  void check_routes() {
    std::map<std::tuple<QString, QString, RouteKind>, QString> seen;
    for (const auto& route : def.routes) {
      const Site* a = def.site(route.from);
      const Site* b = def.site(route.to);
      if (a == nullptr) {
        error(QStringLiteral("route_endpoint_unknown"),
              route.id,
              QStringLiteral("route starts at '%1', which is not a site")
                  .arg(route.from));
      }
      if (b == nullptr) {
        error(QStringLiteral("route_endpoint_unknown"),
              route.id,
              QStringLiteral("route ends at '%1', which is not a site").arg(route.to));
      }
      if (route.from == route.to) {
        error(QStringLiteral("route_self_loop"),
              route.id,
              QStringLiteral("route starts and ends at the same site"));
      }
      if (route.throughput < 1 || route.throughput > k_throughput_max) {
        error(QStringLiteral("value_out_of_range"),
              route.id,
              QStringLiteral("throughput %1 is outside 1..%2")
                  .arg(route.throughput)
                  .arg(k_throughput_max));
      }
      for (const auto& point : route.waypoints) {
        if (!def.projection.contains(point)) {
          error(QStringLiteral("site_out_of_bounds"),
                route.id,
                QStringLiteral("a waypoint lies outside the campaign map"));
          break;
        }
      }
      check_battlefield(route.id, def.display_name(route.id), route.battlefield);
      if (a == nullptr || b == nullptr || route.from == route.to) {
        continue;
      }
      const auto ends = std::minmax(route.from, route.to);
      const auto key = std::make_tuple(ends.first, ends.second, route.kind);
      if (const auto it = seen.find(key); it != seen.end()) {
        warning(QStringLiteral("route_duplicate"),
                route.id,
                QStringLiteral("%1 duplicates %2 (same ends, same kind)")
                    .arg(def.display_name(route.id), def.display_name(it->second)),
                it->second);
      } else {
        seen.emplace(key, route.id);
      }
      if (route.kind == RouteKind::SeaLane && (!a->has_port() || !b->has_port())) {
        error(QStringLiteral("sea_lane_without_port"),
              route.id,
              QStringLiteral("sea lane %1 must join two ports; %2 has none")
                  .arg(def.display_name(route.id), a->has_port() ? b->name : a->name));
      }
      const double geodesic = def.route_geodesic_km(route);
      if (route.length_km > 0.0 && route.length_km < geodesic * 0.98) {
        error(QStringLiteral("route_shorter_than_geodesic"),
              route.id,
              QStringLiteral("%1 is authored as %2 km but its ends are %3 km apart")
                  .arg(def.display_name(route.id))
                  .arg(route.length_km, 0, 'f', 0)
                  .arg(geodesic, 0, 'f', 0));
      }
      if (route.length_km > 0.0 && route.length_km > geodesic * 3.0 + 20.0) {
        warning(QStringLiteral("route_length_implausible"),
                route.id,
                QStringLiteral("%1 is authored as %2 km for a %3 km gap")
                    .arg(def.display_name(route.id))
                    .arg(route.length_km, 0, 'f', 0)
                    .arg(geodesic, 0, 'f', 0));
      }
      if (route.kind != RouteKind::SeaLane && ctx.geometry != nullptr) {
        check_route_over_sea(route);
      }
    }
  }

  void check_network() {
    for (const auto& site : def.sites) {
      if (def.routes_at(site.id).isEmpty()) {
        error(QStringLiteral("site_unconnected"),
              site.id,
              QStringLiteral(
                  "%1 has no route; every site must be on the road or sea network")
                  .arg(site.name));
      }
    }
    const StrategicGraph graph(def, def.initial_state);
    const auto parts = graph.components(TraversalRules::anything());
    for (int i = 1; i < parts.size(); ++i) {
      QStringList names;
      for (const auto& id : parts[i]) {
        names.append(def.display_name(id));
      }
      if (parts[i].size() == 1 && def.routes_at(parts[i].first()).isEmpty()) {
        continue;
      }
      error(QStringLiteral("network_disconnected"),
            parts[i].first(),
            QStringLiteral("%1 cannot be reached from the main network: %2")
                .arg(parts[i].size() == 1 ? QStringLiteral("a site")
                                          : QStringLiteral("a group of sites"),
                     names.join(QStringLiteral(", "))));
    }
    const auto land = graph.components(TraversalRules::land_only());
    for (const auto& part : land) {
      bool all_islands = true;
      for (const auto& id : part) {
        const Site* site = def.site(id);
        const Region* region = site != nullptr ? def.region(site->region) : nullptr;
        all_islands = all_islands && region != nullptr &&
                      region->tags.contains(QStringLiteral("island"));
      }
      if (part.size() > 1 && part.size() < 4 && !all_islands && land.size() > 1 &&
          part != land.first()) {
        info(QStringLiteral("land_pocket"),
             part.first(),
             QStringLiteral("%1 site(s) reachable overland only from each other: %2")
                 .arg(part.size())
                 .arg(part.join(QStringLiteral(", "))));
      }
    }
    const QStringList bridges = graph.bridge_routes(TraversalRules::anything());
    for (const auto& id : bridges) {
      const Route* route = def.route(id);
      if (route == nullptr || route->chokepoint || route->kind == RouteKind::SeaLane) {
        continue;
      }
      TraversalRules without = TraversalRules::anything();
      without.excluded_routes = {id};
      const auto split = graph.components(without);
      if (split.size() >= 2 && split[1].size() >= 3) {
        info(QStringLiteral("unmarked_chokepoint"),
             id,
             QStringLiteral(
                 "%1 is the only link to %2 sites but is not marked as a chokepoint")
                 .arg(def.display_name(id))
                 .arg(split[1].size()));
      }
    }
  }

  void check_commanders() {
    for (const auto& commander : def.commanders) {
      if (!known_faction(commander.faction)) {
        error(QStringLiteral("unknown_faction"),
              commander.id,
              QStringLiteral("%1 serves unknown faction '%2'")
                  .arg(commander.name, commander.faction));
      }
      if (!commander.catalog_id.isEmpty() && ctx.commander_catalog_ids.has_value() &&
          !ctx.commander_catalog_ids->contains(commander.catalog_id)) {
        error(QStringLiteral("commander_catalog_unknown"),
              commander.id,
              QStringLiteral(
                  "%1 links battlefield commander '%2', which is not in the catalogue")
                  .arg(commander.name, commander.catalog_id));
      }
    }
  }

  void check_events() {
    int previous_key = std::numeric_limits<int>::min();
    QString previous_id;
    for (const auto& event : def.events) {
      if (event.effects.isEmpty()) {
        warning(QStringLiteral("event_without_effects"),
                event.id,
                QStringLiteral("%1 changes nothing in the world").arg(event.name));
      }
      if (event.chronological_key() < previous_key) {
        error(QStringLiteral("event_out_of_order"),
              event.id,
              QStringLiteral("%1 (%2) is listed after a later event")
                  .arg(event.name, event.date_label()),
              previous_id);
      }
      previous_key = event.chronological_key();
      previous_id = event.id;
      if (!event.site.isEmpty() && def.site(event.site) == nullptr) {
        error(QStringLiteral("event_site_unknown"),
              event.id,
              QStringLiteral("%1 happens at '%2', which is not a site")
                  .arg(event.name, event.site));
      }
      if (!event.mission.isEmpty() && ctx.mission_ids.has_value() &&
          !ctx.mission_ids->contains(event.mission)) {
        error(QStringLiteral("event_mission_unknown"),
              event.id,
              QStringLiteral("%1 links mission '%2', which does not exist")
                  .arg(event.name, event.mission));
      }
    }
    WorldState state = def.initial_state;
    QSet<QString> reported;
    for (const auto& issue : report.issues) {
      reported.insert(issue.code + QLatin1Char('|') + issue.entity);
    }
    for (const auto& event : def.events) {
      if (state.applied_events.contains(event.id)) {
        continue;
      }
      for (int index = 0; index < event.effects.size(); ++index) {
        const Effect& effect = event.effects[index];
        const ArmyState* army =
            effect.op == EffectOp::MoveArmy ? state.army(effect.target) : nullptr;
        if (army != nullptr && def.site(effect.to) != nullptr &&
            def.site(army->location) != nullptr) {
          const StrategicGraph graph(def, state);
          if (!graph
                   .shortest_path(army->location, effect.to, TraversalRules::anything())
                   .found) {
            error(QStringLiteral("event_army_teleports"),
                  event.id,
                  QStringLiteral("%1 moves %2 from %3 to %4, but no route joins them")
                      .arg(event.name,
                           def.display_name(army->id),
                           def.display_name(army->location),
                           def.display_name(effect.to)),
                  effect.target);
          }
        }
        QString message;
        if (!apply_effect(def, state, effect, &message)) {
          error(QStringLiteral("event_effect_failed"),
                event.id,
                QStringLiteral("%1, effect %2: %3")
                    .arg(event.name)
                    .arg(index)
                    .arg(message));
        }
      }
      state.applied_events.append(event.id);
      const ValidationReport after = validate_state(def, state, event.name);
      for (const auto& issue : after.issues) {
        const QString key = issue.code + QLatin1Char('|') + issue.entity;
        if (!reported.contains(key)) {
          reported.insert(key);
          report.issues.append(issue);
        }
      }
    }
  }

  void check_campaign() {
    const ArmyState* player_army = nullptr;
    for (const auto& army : def.initial_state.armies) {
      if (army.faction == def.player_faction) {
        player_army = &army;
        break;
      }
    }
    const StrategicGraph graph(def, def.initial_state);
    for (const auto& mission : ctx.campaign_missions) {
      const Region* region = def.resolve_campaign_region(mission.world_region_id);
      if (region == nullptr) {
        error(QStringLiteral("campaign_region_unknown"),
              mission.mission_id,
              QStringLiteral(
                  "campaign mission %1 names world_region_id '%2', which is neither a "
                  "region nor a district")
                  .arg(mission.mission_id, mission.world_region_id));
      }
      const Site* site = def.site_for_mission(mission.mission_id);
      if (site == nullptr) {
        error(QStringLiteral("campaign_mission_unplaced"),
              mission.mission_id,
              QStringLiteral("campaign mission %1 has no site in the strategic world")
                  .arg(mission.mission_id));
        continue;
      }
      if (region != nullptr && site->region != region->id) {
        error(QStringLiteral("campaign_region_mismatch"),
              site->id,
              QStringLiteral("%1 is in %2 but campaign mission %3 says %4")
                  .arg(site->name,
                       site->region,
                       mission.mission_id,
                       mission.world_region_id));
      } else if (region != nullptr && mission.world_region_id != region->id &&
                 site->district != mission.world_region_id) {
        error(QStringLiteral("campaign_region_mismatch"),
              site->id,
              QStringLiteral("%1 is not in district %2 named by campaign mission %3")
                  .arg(site->name, mission.world_region_id, mission.mission_id));
      }
      if (player_army != nullptr) {
        const Path path = graph.shortest_path(
            player_army->location, site->id, TraversalRules::anything());
        if (!path.found) {
          error(QStringLiteral("campaign_site_unreachable"),
                site->id,
                QStringLiteral("%1 (mission %2) cannot be reached from %3")
                    .arg(site->name,
                         mission.mission_id,
                         def.display_name(player_army->location)));
        }
      }
      if (events_for_mission(def, mission.mission_id).isEmpty()) {
        warning(
            QStringLiteral("mission_without_event"),
            mission.mission_id,
            QStringLiteral(
                "campaign mission %1 has no world event, so its result changes nothing")
                .arg(mission.mission_id));
      }
    }
  }

  void check_province_mapping() {
    for (auto it = ctx.mission_provinces.cbegin(); it != ctx.mission_provinces.cend();
         ++it) {
      const Site* site = def.site_for_mission(it.key());
      if (site != nullptr && site->region != it.value()) {
        error(QStringLiteral("province_mapping_mismatch"),
              site->id,
              QStringLiteral("valid_provinces.json places mission %1 in %2, but its "
                             "site %3 lies in %4")
                  .arg(it.key(), it.value(), site->name, site->region));
      }
    }
  }

  void check_mission_facts() {
    for (int index = 0; index < def.events.size(); ++index) {
      const WorldEvent& event = def.events[index];
      const auto facts = ctx.mission_facts.constFind(event.mission);
      if (event.mission.isEmpty() || facts == ctx.mission_facts.cend()) {
        continue;
      }
      if (facts->first_year != 0) {
        const int latest = std::max(facts->first_year, facts->last_year);
        const int earliest = std::min(facts->first_year, facts->last_year);
        if (event.year < earliest || event.year > latest) {
          warning(QStringLiteral("mission_date_mismatch"),
                  event.id,
                  QStringLiteral("mission %1 is dated %2 but its world event is %3")
                      .arg(event.mission,
                           format_year(facts->first_year),
                           event.date_label()));
        }
      }
      const WorldState before = state_after(def, index);
      for (const auto& side : facts->sides) {
        for (const auto& catalog_id : side.commanders) {
          const CommanderRecord* record = nullptr;
          for (const auto& commander : def.commanders) {
            if (commander.catalog_id == catalog_id) {
              record = &commander;
            }
          }
          if (record == nullptr) {
            warning(QStringLiteral("mission_commander_unmodelled"),
                    event.id,
                    QStringLiteral(
                        "mission %1 fields %2, who is not a commander in the world")
                        .arg(event.mission, catalog_id));
            continue;
          }
          const QString faction = commander_faction(def, before, record->id);
          if (!side.faction.isEmpty() && faction != side.faction &&
              !are_friendly(def, faction, side.faction)) {
            error(QStringLiteral("mission_commander_side"),
                  event.id,
                  QStringLiteral("mission %1 puts %2 on the %3 side (%4), but in the "
                                 "world he serves %5 at %6")
                      .arg(event.mission,
                           record->name,
                           side.side,
                           side.faction,
                           faction,
                           event.date_label()),
                  record->id);
          }
          const CommanderStatus status = commander_status(before, record->id);
          if (status == CommanderStatus::Dead || status == CommanderStatus::Captured) {
            error(QStringLiteral("mission_commander_absent"),
                  event.id,
                  QStringLiteral("mission %1 fields %2, who is %3 by %4")
                      .arg(event.mission,
                           record->name,
                           to_string(status),
                           event.date_label()),
                  record->id);
          }
        }
      }
    }
  }

  void check_initial_state() {
    report.issues.append(
        validate_state(def, def.initial_state, QStringLiteral("initial state")).issues);
    for (const auto& faction : def.factions) {
      if (!faction.capital.isEmpty() && def.site(faction.capital) != nullptr &&
          site_controller(def, def.initial_state, faction.capital) != faction.id) {
        warning(QStringLiteral("capital_not_held"),
                faction.id,
                QStringLiteral("%1 does not hold its own capital at the start")
                    .arg(faction.name),
                faction.capital);
      }
    }
    for (const auto& supply : evaluate_all_supply(def, def.initial_state)) {
      if (supply.level == SupplyLevel::CutOff) {
        warning(QStringLiteral("army_starts_cut_off"),
                supply.army,
                QStringLiteral("%1 starts the campaign cut off: %2")
                    .arg(def.display_name(supply.army), supply.reason));
      }
    }
  }
};

} // namespace

auto to_string(Severity severity) -> QString {
  switch (severity) {
  case Severity::Error:
    return QStringLiteral("error");
  case Severity::Warning:
    return QStringLiteral("warning");
  case Severity::Info:
    return QStringLiteral("info");
  }
  return {};
}

auto ValidationReport::errors() const -> int {
  return static_cast<int>(
      std::count_if(issues.begin(), issues.end(), [](const Issue& issue) {
        return issue.severity == Severity::Error;
      }));
}

auto ValidationReport::warnings() const -> int {
  return static_cast<int>(
      std::count_if(issues.begin(), issues.end(), [](const Issue& issue) {
        return issue.severity == Severity::Warning;
      }));
}

auto ValidationReport::has(const QString& code) const -> bool {
  return std::any_of(issues.begin(), issues.end(), [&code](const Issue& issue) {
    return issue.code == code;
  });
}

auto ValidationReport::for_entity(const QString& entity) const -> QVector<Issue> {
  QVector<Issue> out;
  for (const auto& issue : issues) {
    if (issue.entity == entity || issue.related == entity) {
      out.append(issue);
    }
  }
  return out;
}

auto ValidationReport::codes() const -> QStringList {
  QStringList out;
  for (const auto& issue : issues) {
    if (!out.contains(issue.code)) {
      out.append(issue.code);
    }
  }
  return out;
}

void ValidationReport::add(Severity severity,
                           const QString& code,
                           const QString& entity,
                           const QString& message,
                           const QString& related) {
  issues.append({severity, code, entity, message, related});
}

auto validate_state(const WorldDefinition& def,
                    const WorldState& state,
                    const QString& label) -> ValidationReport {
  ValidationReport report;
  const QString where =
      label.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(label);
  const auto known_faction = [&def](const QString& id) {
    return id.isEmpty() || id == QLatin1String(k_neutral_faction) ||
           def.faction(id) != nullptr;
  };
  for (const auto& [id, region] : state.regions) {
    if (def.region(id) == nullptr) {
      report.add(Severity::Error,
                 QStringLiteral("state_unknown_entity"),
                 id,
                 QStringLiteral("state names region '%1', which does not exist%2")
                     .arg(id, where));
    }
    if (!known_faction(region.controller)) {
      report.add(Severity::Error,
                 QStringLiteral("unknown_faction"),
                 id,
                 QStringLiteral("%1 is held by unknown faction '%2'%3")
                     .arg(id, region.controller, where));
    }
  }
  for (const auto& [id, site] : state.sites) {
    if (def.site(id) == nullptr) {
      report.add(Severity::Error,
                 QStringLiteral("state_unknown_entity"),
                 id,
                 QStringLiteral("state names site '%1', which does not exist%2")
                     .arg(id, where));
    }
    if (!known_faction(site.controller)) {
      report.add(Severity::Error,
                 QStringLiteral("unknown_faction"),
                 id,
                 QStringLiteral("%1 is held by unknown faction '%2'%3")
                     .arg(id, site.controller, where));
    }
    if (site.fortification > k_fortification_max) {
      report.add(Severity::Error,
                 QStringLiteral("value_out_of_range"),
                 id,
                 QStringLiteral("fortification %1 is above %2%3")
                     .arg(site.fortification)
                     .arg(k_fortification_max)
                     .arg(where));
    }
  }
  for (const auto& [id, route] : state.routes) {
    if (def.route(id) == nullptr) {
      report.add(Severity::Error,
                 QStringLiteral("state_unknown_entity"),
                 id,
                 QStringLiteral("state names route '%1', which does not exist%2")
                     .arg(id, where));
    }
  }
  for (const auto& [id, commander] : state.commanders) {
    if (def.commander(id) == nullptr) {
      report.add(Severity::Error,
                 QStringLiteral("state_unknown_entity"),
                 id,
                 QStringLiteral("state names commander '%1', which does not exist%2")
                     .arg(id, where));
    }
    if (!known_faction(commander.faction)) {
      report.add(Severity::Error,
                 QStringLiteral("unknown_faction"),
                 id,
                 QStringLiteral("commander %1 serves unknown faction '%2'%3")
                     .arg(id, commander.faction, where));
    }
  }
  QHash<QString, QString> commanded;
  for (const auto& army : state.armies) {
    if (def.site(army.location) == nullptr) {
      report.add(Severity::Error,
                 QStringLiteral("army_location_unknown"),
                 army.id,
                 QStringLiteral("%1 stands at '%2', which is not a site%3")
                     .arg(army.name, army.location, where));
    }
    if (army.faction.isEmpty() || !known_faction(army.faction) ||
        army.faction == QLatin1String(k_neutral_faction)) {
      report.add(Severity::Error,
                 QStringLiteral("unknown_faction"),
                 army.id,
                 QStringLiteral("%1 belongs to unknown faction '%2'%3")
                     .arg(army.name, army.faction, where));
    }
    if (!army.objective.isEmpty() && def.site(army.objective) == nullptr) {
      report.add(Severity::Error,
                 QStringLiteral("army_objective_unknown"),
                 army.id,
                 QStringLiteral("%1 marches on '%2', which is not a site%3")
                     .arg(army.name, army.objective, where));
    }
    if (army.soldiers() <= 0) {
      report.add(Severity::Warning,
                 QStringLiteral("army_empty"),
                 army.id,
                 QStringLiteral("%1 has no soldiers%2").arg(army.name, where));
    }
    if (army.commander.isEmpty()) {
      continue;
    }
    const CommanderRecord* record = def.commander(army.commander);
    if (record == nullptr) {
      report.add(Severity::Error,
                 QStringLiteral("army_commander_unknown"),
                 army.id,
                 QStringLiteral("%1 is led by unknown commander '%2'%3")
                     .arg(army.name, army.commander, where));
      continue;
    }
    if (commanded.contains(army.commander)) {
      report.add(
          Severity::Error,
          QStringLiteral("commander_in_two_armies"),
          army.id,
          QStringLiteral("%1 leads both %2 and %3%4")
              .arg(record->name, commanded.value(army.commander), army.id, where),
          commanded.value(army.commander));
    }
    commanded.insert(army.commander, army.id);
    const QString faction = commander_faction(def, state, army.commander);
    if (faction != army.faction && !are_friendly(def, faction, army.faction)) {
      report.add(Severity::Warning,
                 QStringLiteral("army_commander_faction"),
                 army.id,
                 QStringLiteral("%1 (%2) leads %3 (%4)%5")
                     .arg(record->name, faction, army.name, army.faction, where));
    }
    const CommanderStatus status = commander_status(state, army.commander);
    if (status == CommanderStatus::Dead || status == CommanderStatus::Captured) {
      report.add(Severity::Error,
                 QStringLiteral("army_led_by_absent_commander"),
                 army.id,
                 QStringLiteral("%1 is led by %2, who is %3%4")
                     .arg(army.name, record->name, to_string(status), where));
    }
  }
  return report;
}

auto validate_world(const WorldDefinition& def,
                    const ValidationContext& context) -> ValidationReport {
  ValidationReport report;
  Checker checker{def, context, report};
  checker.check_load_issues();
  checker.check_ids();
  checker.check_factions();
  checker.check_regions();
  checker.check_sites();
  checker.check_routes();
  checker.check_region_links();
  checker.check_network();
  checker.check_commanders();
  checker.check_initial_state();
  checker.check_events();
  checker.check_campaign();
  checker.check_province_mapping();
  checker.check_mission_facts();
  return report;
}

auto validation_code_help(const QString& code) -> QString {
  static const QHash<QString, QString> k_help = {
      {QStringLiteral("site_unconnected"),
       QStringLiteral("Every site needs at least one route. Draw a road to its nearest "
                      "neighbour.")},
      {QStringLiteral("network_disconnected"),
       QStringLiteral("Part of the world cannot be reached from the rest. Add a road, "
                      "pass or sea lane.")},
      {QStringLiteral("adjacent_without_route"),
       QStringLiteral("Bordering regions should be joined by at least one route so "
                      "armies and supply can cross.")},
      {QStringLiteral("route_crosses_sea"),
       QStringLiteral("An overland route leaves the land. Add waypoints along the "
                      "coast or change it to a sea lane.")},
      {QStringLiteral("site_outside_region"),
       QStringLiteral("The site's coordinates fall in another province on the campaign "
                      "map. Move it or change its region.")},
      {QStringLiteral("sea_lane_without_port"),
       QStringLiteral(
           "Sea lanes join ports. Give both ends a port class or use a coastal road.")},
      {QStringLiteral("campaign_mission_unplaced"),
       QStringLiteral(
           "Each campaign mission needs a site whose battlefield links that mission.")},
      {QStringLiteral("campaign_region_unknown"),
       QStringLiteral(
           "The campaign's world_region_id must name a region or a district of one.")},
      {QStringLiteral("event_army_teleports"),
       QStringLiteral("A move_army effect has no route between where the army stands "
                      "and where it goes.")},
      {QStringLiteral("unmarked_chokepoint"),
       QStringLiteral("Cutting this route splits the network. Mark it as a chokepoint "
                      "so campaign logic treats it as one.")},
      {QStringLiteral("battlefield_map_mismatch"),
       QStringLiteral("The site's battlefield map differs from the map its mission "
                      "actually loads.")},
  };
  return k_help.value(code);
}

} // namespace Game::Strategic
