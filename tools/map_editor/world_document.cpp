#include "world_document.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>

#include <algorithm>
#include <limits>
#include <optional>
#include <tuple>
#include <utility>

#include "game/strategic/strategic_events.h"
#include "game/strategic/strategic_json.h"

namespace MapEditor {

namespace {

namespace S = Game::Strategic;

constexpr int k_undo_limit = 200;

auto default_name(S::SiteKind kind) -> QString {
  switch (kind) {
  case S::SiteKind::City:
    return QStringLiteral("New City");
  case S::SiteKind::Town:
    return QStringLiteral("New Town");
  case S::SiteKind::Colony:
    return QStringLiteral("New Colony");
  case S::SiteKind::Fort:
    return QStringLiteral("New Fort");
  case S::SiteKind::Camp:
    return QStringLiteral("New Camp");
  case S::SiteKind::Pass:
    return QStringLiteral("New Pass");
  case S::SiteKind::Crossing:
    return QStringLiteral("New Crossing");
  case S::SiteKind::Field:
    return QStringLiteral("New Battlefield");
  }
  return QStringLiteral("New Site");
}

} // namespace

WorldDocument::WorldDocument(QObject* parent)
    : QObject(parent) {
}

auto WorldDocument::load(const QString& path, QString* error) -> bool {
  QString root = S::find_repository_root(path);
  if (root.isEmpty()) {
    root = S::find_repository_root(QDir::currentPath());
  }
  if (root.isEmpty()) {
    root = S::find_repository_root(QCoreApplication::applicationDirPath());
  }
  auto bundle = S::load_world_bundle(path, root, error);
  if (!bundle.has_value()) {
    return false;
  }
  m_root = bundle->root;
  adopt(std::move(bundle->definition), bundle->geometry, bundle->context, path);
  return true;
}

void WorldDocument::adopt(S::WorldDefinition definition,
                          std::shared_ptr<S::RegionGeometry> geometry,
                          S::ValidationContext context,
                          const QString& path) {
  m_def = std::move(definition);
  m_geometry = std::move(geometry);
  m_context = std::move(context);
  m_context.geometry = m_geometry.get();
  m_path = path;
  m_undo.clear();
  m_redo.clear();
  m_selection.clear();
  m_timeline = 0;
  refresh_state();
  revalidate();
  set_modified(false);
  emit loaded();
  emit changed();
  emit timeline_changed(m_timeline);
  emit selection_changed(m_selection);
}

auto WorldDocument::save(QString* error) -> bool {
  return save_as(m_path, error);
}

auto WorldDocument::save_as(const QString& path, QString* error) -> bool {
  if (path.isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("no file name");
    }
    return false;
  }
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    if (error != nullptr) {
      *error = file.errorString();
    }
    return false;
  }
  file.write(S::definition_to_bytes(m_def));
  if (!file.commit()) {
    if (error != nullptr) {
      *error = file.errorString();
    }
    return false;
  }
  m_path = path;
  set_modified(false);
  return true;
}

auto WorldDocument::supply_for(const QString& army) const -> const S::SupplyReport* {
  for (const auto& report : m_supply) {
    if (report.army == army) {
      return &report;
    }
  }
  return nullptr;
}

void WorldDocument::set_timeline_index(int events_applied) {
  const int clamped =
      std::clamp(events_applied, 0, static_cast<int>(m_def.events.size()));
  if (clamped == m_timeline) {
    return;
  }
  m_timeline = clamped;
  refresh_state();
  emit timeline_changed(m_timeline);
  emit changed();
}

auto WorldDocument::current_event() const -> const S::WorldEvent* {
  if (m_timeline <= 0 || m_timeline > m_def.events.size()) {
    return nullptr;
  }
  return &m_def.events[m_timeline - 1];
}

void WorldDocument::select(const QString& id) {
  if (id == m_selection) {
    return;
  }
  m_selection = id;
  emit selection_changed(id);
}

void WorldDocument::undo() {
  if (m_undo.isEmpty()) {
    return;
  }
  m_redo.append(m_def);
  m_def = m_undo.takeLast();
  commit();
}

void WorldDocument::redo() {
  if (m_redo.isEmpty()) {
    return;
  }
  m_undo.append(m_def);
  m_def = m_redo.takeLast();
  commit();
}

void WorldDocument::checkpoint() {
  m_undo.append(m_def);
  if (m_undo.size() > k_undo_limit) {
    m_undo.removeFirst();
  }
  m_redo.clear();
}

void WorldDocument::commit() {
  if (m_def.kind_of(m_selection) == S::EntityKind::None) {
    m_selection.clear();
    emit selection_changed(m_selection);
  }
  m_timeline = std::min(m_timeline, static_cast<int>(m_def.events.size()));
  refresh_state();
  revalidate();
  set_modified(true);
  emit changed();
}

void WorldDocument::refresh_state() {
  m_state = S::state_after(m_def, m_timeline);
  m_previous_state = S::state_after(m_def, std::max(0, m_timeline - 1));
  m_supply = S::evaluate_all_supply(m_def, m_state);
}

void WorldDocument::revalidate() {
  m_report = S::validate_world(m_def, m_context);
}

void WorldDocument::set_modified(bool modified) {
  if (m_modified == modified) {
    return;
  }
  m_modified = modified;
  emit modified_changed(modified);
}

auto WorldDocument::move_site(const QString& id, const S::LonLat& position) -> bool {
  const S::Site* site = m_def.site(id);
  if (site == nullptr || site->position == position) {
    return false;
  }
  checkpoint();
  S::Site* editable = m_def.site_mut(id);
  editable->position = position;
  const QString region = region_for(position);
  if (!region.isEmpty() && region != editable->region) {
    editable->region = region;
    editable->district.clear();
  }
  commit();
  return true;
}

auto WorldDocument::add_site(const S::LonLat& position, S::SiteKind kind) -> QString {
  S::Site site;
  site.name = default_name(kind);
  site.id = m_def.unique_id(site.name);
  site.kind = kind;
  site.position = position;
  site.region = region_for(position);
  if (site.region.isEmpty()) {
    const QString nearest = nearest_site(position);
    if (const S::Site* other = m_def.site(nearest)) {
      site.region = other->region;
    }
  }
  site.population =
      S::is_settlement(kind) ? S::PopulationBand::Small : S::PopulationBand::None;
  site.fortification = (kind == S::SiteKind::Fort || kind == S::SiteKind::City) ? 2 : 0;
  if (S::is_settlement(kind)) {
    site.resources.set(S::Resource::Food, 2);
    site.resources.set(S::Resource::Manpower, 1);
  }
  site.terrain = kind == S::SiteKind::Pass       ? S::Terrain::Mountains
                 : kind == S::SiteKind::Crossing ? S::Terrain::River
                                                 : S::Terrain::Plains;
  checkpoint();
  m_def.sites.append(site);
  commit();
  select(site.id);
  return site.id;
}

auto WorldDocument::add_route(const QString& from,
                              const QString& to,
                              S::RouteKind kind) -> QString {
  if (from == to || m_def.site(from) == nullptr || m_def.site(to) == nullptr) {
    return {};
  }
  S::Route route;
  route.id = m_def.unique_id(QStringLiteral("%1_%2").arg(from, to));
  route.from = from;
  route.to = to;
  route.kind = kind;
  route.throughput =
      kind == S::RouteKind::MajorRoad                                       ? 4
      : (kind == S::RouteKind::Track || kind == S::RouteKind::MountainPass) ? 1
                                                                            : 3;
  route.chokepoint =
      kind == S::RouteKind::MountainPass || kind == S::RouteKind::RiverCrossing;
  checkpoint();
  m_def.routes.append(route);
  commit();
  select(route.id);
  return route.id;
}

auto WorldDocument::add_waypoint(const QString& route_id,
                                 const S::LonLat& position) -> bool {
  const S::Route* route = m_def.route(route_id);
  if (route == nullptr) {
    return false;
  }
  const QVector<S::LonLat> line = m_def.route_polyline(*route);
  int best_segment = 0;
  double best = std::numeric_limits<double>::infinity();
  for (int i = 1; i < line.size(); ++i) {
    const double detour = S::great_circle_km(line[i - 1], position) +
                          S::great_circle_km(position, line[i]) -
                          S::great_circle_km(line[i - 1], line[i]);
    if (detour < best) {
      best = detour;
      best_segment = i - 1;
    }
  }
  checkpoint();
  m_def.route_mut(route_id)->waypoints.insert(best_segment, position);
  commit();
  return true;
}

auto WorldDocument::clear_waypoints(const QString& route_id) -> bool {
  const S::Route* route = m_def.route(route_id);
  if (route == nullptr || route->waypoints.isEmpty()) {
    return false;
  }
  checkpoint();
  m_def.route_mut(route_id)->waypoints.clear();
  commit();
  return true;
}

auto WorldDocument::move_waypoint(const QString& route_id,
                                  int index,
                                  const S::LonLat& position) -> bool {
  const S::Route* route = m_def.route(route_id);
  if (route == nullptr || index < 0 || index >= route->waypoints.size() ||
      route->waypoints[index] == position) {
    return false;
  }
  checkpoint();
  m_def.route_mut(route_id)->waypoints[index] = position;
  commit();
  return true;
}

auto WorldDocument::remove_waypoint(const QString& route_id, int index) -> bool {
  const S::Route* route = m_def.route(route_id);
  if (route == nullptr || index < 0 || index >= route->waypoints.size()) {
    return false;
  }
  checkpoint();
  m_def.route_mut(route_id)->waypoints.remove(index);
  commit();
  return true;
}

auto WorldDocument::set_initial_route_status(const QString& route_id,
                                             S::RouteStatus status) -> bool {
  if (m_def.route(route_id) == nullptr ||
      S::route_status(m_def.initial_state, route_id) == status) {
    return false;
  }
  checkpoint();
  if (status == S::RouteStatus::Open) {
    m_def.initial_state.routes.erase(route_id);
  } else {
    m_def.initial_state.routes[route_id].status = status;
  }
  commit();
  return true;
}

auto WorldDocument::remove(const QString& id) -> bool {
  const S::EntityKind kind = m_def.kind_of(id);
  if (kind == S::EntityKind::Site) {
    checkpoint();
    m_def.sites.erase(
        std::remove_if(m_def.sites.begin(),
                       m_def.sites.end(),
                       [&id](const S::Site& site) { return site.id == id; }),
        m_def.sites.end());
    m_def.routes.erase(
        std::remove_if(m_def.routes.begin(),
                       m_def.routes.end(),
                       [&id](const S::Route& route) { return route.touches(id); }),
        m_def.routes.end());
    m_def.initial_state.sites.erase(id);
    commit();
    return true;
  }
  if (kind == S::EntityKind::Route) {
    checkpoint();
    m_def.routes.erase(
        std::remove_if(m_def.routes.begin(),
                       m_def.routes.end(),
                       [&id](const S::Route& route) { return route.id == id; }),
        m_def.routes.end());
    m_def.initial_state.routes.erase(id);
    commit();
    return true;
  }
  return false;
}

auto WorldDocument::update_site(const S::Site& site) -> bool {
  S::Site* existing = m_def.site_mut(site.id);
  if (existing == nullptr) {
    return false;
  }
  if (existing->name == site.name && existing->kind == site.kind &&
      existing->region == site.region && existing->district == site.district &&
      existing->population == site.population &&
      existing->fortification == site.fortification &&
      existing->resources.values == site.resources.values &&
      existing->port == site.port && existing->terrain == site.terrain &&
      existing->tags == site.tags && existing->latin_name == site.latin_name &&
      existing->battlefield.map == site.battlefield.map &&
      existing->battlefield.mission == site.battlefield.mission &&
      existing->battlefield.preset == site.battlefield.preset &&
      existing->position == site.position) {
    return false;
  }
  checkpoint();
  *m_def.site_mut(site.id) = site;
  commit();
  return true;
}

auto WorldDocument::update_route(const S::Route& route) -> bool {
  S::Route* existing = m_def.route_mut(route.id);
  if (existing == nullptr) {
    return false;
  }
  if (existing->name == route.name && existing->kind == route.kind &&
      existing->throughput == route.throughput &&
      existing->chokepoint == route.chokepoint &&
      existing->length_km == route.length_km && existing->from == route.from &&
      existing->to == route.to && existing->waypoints == route.waypoints) {
    return false;
  }
  checkpoint();
  *m_def.route_mut(route.id) = route;
  commit();
  return true;
}

auto WorldDocument::update_region(const S::Region& region) -> bool {
  S::Region* existing = m_def.region_mut(region.id);
  if (existing == nullptr) {
    return false;
  }
  checkpoint();
  *m_def.region_mut(region.id) = region;
  commit();
  return true;
}

auto WorldDocument::set_initial_controller(const QString& id,
                                           const QString& faction) -> bool {
  const S::EntityKind kind = m_def.kind_of(id);
  if (kind != S::EntityKind::Site && kind != S::EntityKind::Region) {
    return false;
  }
  checkpoint();
  if (kind == S::EntityKind::Site) {
    m_def.initial_state.sites[id].controller = faction;
  } else {
    m_def.initial_state.regions[id].controller = faction;
  }
  commit();
  return true;
}

auto WorldDocument::set_initial_army_location(const QString& army,
                                              const QString& site) -> bool {
  S::ArmyState* entry = m_def.initial_state.army(army);
  if (entry == nullptr || m_def.site(site) == nullptr || entry->location == site) {
    return false;
  }
  checkpoint();
  m_def.initial_state.army(army)->location = site;
  commit();
  return true;
}

auto WorldDocument::complete_network() -> int {
  const auto link_kind = [this](const S::Site& a,
                                const S::Site& b) -> std::optional<S::RouteKind> {
    const S::Region* region = m_def.region(a.region);
    const bool overland = a.region == b.region ||
                          (region != nullptr && region->adjacent.contains(b.region));
    if (overland) {
      return S::RouteKind::Road;
    }
    if (a.has_port() && b.has_port()) {
      return S::RouteKind::SeaLane;
    }
    return std::nullopt;
  };
  const auto best_link = [&](const QStringList& from, const QStringList& to)
      -> std::tuple<const S::Site*, const S::Site*, S::RouteKind> {
    double best = std::numeric_limits<double>::infinity();
    std::tuple<const S::Site*, const S::Site*, S::RouteKind> choice{
        nullptr, nullptr, S::RouteKind::Track};
    for (int pass = 0; pass < 2 && std::get<0>(choice) == nullptr; ++pass) {
      for (const auto& a_id : from) {
        for (const auto& b_id : to) {
          const S::Site* a = m_def.site(a_id);
          const S::Site* b = m_def.site(b_id);
          if (a == nullptr || b == nullptr || a == b) {
            continue;
          }
          const auto kind =
              pass == 0 ? link_kind(*a, *b) : std::optional(S::RouteKind::Track);
          if (!kind.has_value()) {
            continue;
          }
          const double km = S::great_circle_km(a->position, b->position) *
                            (*kind == S::RouteKind::SeaLane ? 1.1 : 1.0);
          if (km < best) {
            best = km;
            choice = {a, b, *kind};
          }
        }
      }
    }
    return choice;
  };
  checkpoint();
  int added = 0;
  const auto add = [this,
                    &added](const S::Site& a, const S::Site& b, S::RouteKind kind) {
    S::Route route;
    route.id = m_def.unique_id(QStringLiteral("%1_%2").arg(a.id, b.id));
    route.from = a.id;
    route.to = b.id;
    route.kind = kind;
    route.throughput = 2;
    route.tags = {QStringLiteral("auto")};
    m_def.routes.append(route);
    ++added;
  };
  QStringList everyone;
  for (const auto& site : m_def.sites) {
    everyone.append(site.id);
  }
  for (const auto& id : everyone) {
    if (!m_def.routes_at(id).isEmpty()) {
      continue;
    }
    const auto [a, b, kind] = best_link({id}, everyone);
    if (a != nullptr) {
      add(*a, *b, kind);
    }
  }
  for (int guard = 0; guard < m_def.sites.size(); ++guard) {
    const S::StrategicGraph graph(m_def, m_def.initial_state);
    const auto parts = graph.components(S::TraversalRules::anything());
    if (parts.size() <= 1) {
      break;
    }
    const auto [a, b, kind] = best_link(parts[1], parts[0]);
    if (a == nullptr) {
      break;
    }
    add(*a, *b, kind);
  }
  if (added == 0) {
    m_undo.removeLast();
    return 0;
  }
  commit();
  return added;
}

auto WorldDocument::region_for(const S::LonLat& position) const -> QString {
  if (m_geometry == nullptr) {
    return {};
  }
  return m_geometry->region_at(S::uv_of(m_def.projection, position));
}

auto WorldDocument::nearest_site(const S::LonLat& position,
                                 const QStringList& exclude) const -> QString {
  double best = std::numeric_limits<double>::infinity();
  QString id;
  for (const auto& site : m_def.sites) {
    if (exclude.contains(site.id)) {
      continue;
    }
    const double km = S::great_circle_km(site.position, position);
    if (km < best) {
      best = km;
      id = site.id;
    }
  }
  return id;
}

} // namespace MapEditor
