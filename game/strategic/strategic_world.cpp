#include "strategic_world.h"

#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <utility>

namespace Game::Strategic {

namespace {

constexpr std::array<std::pair<EffectOp, const char*>, 13> k_effect_ops = {{
    {EffectOp::SetController, "set_controller"},
    {EffectOp::SetContested, "set_contested"},
    {EffectOp::SetRouteStatus, "set_route_status"},
    {EffectOp::SetPort, "set_port"},
    {EffectOp::SetFortification, "set_fortification"},
    {EffectOp::SetGarrison, "set_garrison"},
    {EffectOp::MoveArmy, "move_army"},
    {EffectOp::SetArmyStrength, "set_army_strength"},
    {EffectOp::ScaleArmy, "scale_army"},
    {EffectOp::CreateArmy, "create_army"},
    {EffectOp::DisbandArmy, "disband_army"},
    {EffectOp::SetCommander, "set_commander"},
    {EffectOp::SetFlag, "set_flag"},
}};

} // namespace

auto to_string(EffectOp op) -> QString {
  for (const auto& [entry, name] : k_effect_ops) {
    if (entry == op) {
      return QString::fromLatin1(name);
    }
  }
  return {};
}

auto effect_op_from_string(const QString& text) -> std::optional<EffectOp> {
  const QString key = text.trimmed().toLower();
  for (const auto& [entry, name] : k_effect_ops) {
    if (key == QLatin1String(name)) {
      return entry;
    }
  }
  return std::nullopt;
}

auto to_string(EntityKind kind) -> QString {
  switch (kind) {
  case EntityKind::None:
    return QStringLiteral("none");
  case EntityKind::Faction:
    return QStringLiteral("faction");
  case EntityKind::Region:
    return QStringLiteral("region");
  case EntityKind::Site:
    return QStringLiteral("site");
  case EntityKind::Route:
    return QStringLiteral("route");
  case EntityKind::Army:
    return QStringLiteral("army");
  case EntityKind::Commander:
    return QStringLiteral("commander");
  case EntityKind::Event:
    return QStringLiteral("event");
  }
  return {};
}

auto ResourceProfile::total() const -> int {
  int sum = 0;
  for (const int value : values) {
    sum += value;
  }
  return sum;
}

auto WorldState::army(const QString& id) const -> const ArmyState* {
  for (const auto& entry : armies) {
    if (entry.id == id) {
      return &entry;
    }
  }
  return nullptr;
}

auto WorldState::army(const QString& id) -> ArmyState* {
  for (auto& entry : armies) {
    if (entry.id == id) {
      return &entry;
    }
  }
  return nullptr;
}

auto WorldState::armies_at(const QString& site) const -> QVector<const ArmyState*> {
  QVector<const ArmyState*> out;
  for (const auto& entry : armies) {
    if (entry.location == site) {
      out.append(&entry);
    }
  }
  return out;
}

auto WorldEvent::date_label() const -> QString {
  QString season_name = to_string(season);
  if (!season_name.isEmpty()) {
    season_name[0] = season_name[0].toUpper();
  }
  return QStringLiteral("%1, %2").arg(format_year(year), season_name);
}

template <typename T>
auto WorldDefinition::find_in(const QVector<T>& items,
                              QHash<QString, int>& index,
                              const QString& id) const -> const T* {
  if (id.isEmpty()) {
    return nullptr;
  }
  const int cached = index.value(id, -1);
  if (cached >= 0 && cached < items.size() && items[cached].id == id) {
    return &items[cached];
  }
  index.clear();
  const T* found = nullptr;
  for (int i = 0; i < items.size(); ++i) {
    if (!index.contains(items[i].id)) {
      index.insert(items[i].id, i);
    }
    if (found == nullptr && items[i].id == id) {
      found = &items[i];
    }
  }
  return found;
}

auto WorldDefinition::faction(const QString& id) const -> const Faction* {
  for (const auto& entry : factions) {
    if (entry.id == id) {
      return &entry;
    }
  }
  return nullptr;
}

auto WorldDefinition::region(const QString& id) const -> const Region* {
  return find_in(regions, m_region_index, id);
}

auto WorldDefinition::site(const QString& id) const -> const Site* {
  return find_in(sites, m_site_index, id);
}

auto WorldDefinition::route(const QString& id) const -> const Route* {
  return find_in(routes, m_route_index, id);
}

auto WorldDefinition::commander(const QString& id) const -> const CommanderRecord* {
  for (const auto& entry : commanders) {
    if (entry.id == id) {
      return &entry;
    }
  }
  return nullptr;
}

auto WorldDefinition::event(const QString& id) const -> const WorldEvent* {
  for (const auto& entry : events) {
    if (entry.id == id) {
      return &entry;
    }
  }
  return nullptr;
}

auto WorldDefinition::site_mut(const QString& id) -> Site* {
  const Site* found = site(id);
  return found == nullptr ? nullptr
                          : &sites[static_cast<qsizetype>(found - sites.constData())];
}

auto WorldDefinition::route_mut(const QString& id) -> Route* {
  const Route* found = route(id);
  return found == nullptr ? nullptr
                          : &routes[static_cast<qsizetype>(found - routes.constData())];
}

auto WorldDefinition::region_mut(const QString& id) -> Region* {
  const Region* found = region(id);
  return found == nullptr
             ? nullptr
             : &regions[static_cast<qsizetype>(found - regions.constData())];
}

auto WorldDefinition::kind_of(const QString& id) const -> EntityKind {
  if (id.isEmpty()) {
    return EntityKind::None;
  }
  if (site(id) != nullptr) {
    return EntityKind::Site;
  }
  if (route(id) != nullptr) {
    return EntityKind::Route;
  }
  if (region(id) != nullptr) {
    return EntityKind::Region;
  }
  if (initial_state.army(id) != nullptr) {
    return EntityKind::Army;
  }
  if (commander(id) != nullptr) {
    return EntityKind::Commander;
  }
  if (faction(id) != nullptr) {
    return EntityKind::Faction;
  }
  if (event(id) != nullptr) {
    return EntityKind::Event;
  }
  for (const auto& entry : events) {
    for (const auto& effect : entry.effects) {
      if (effect.op == EffectOp::CreateArmy && effect.target == id) {
        return EntityKind::Army;
      }
    }
  }
  return EntityKind::None;
}

auto WorldDefinition::display_name(const QString& id) const -> QString {
  if (const auto* entry = site(id)) {
    return entry->name;
  }
  if (const auto* entry = route(id)) {
    if (!entry->name.isEmpty()) {
      return entry->name;
    }
    return QStringLiteral("%1 – %2").arg(display_name(entry->from),
                                         display_name(entry->to));
  }
  if (const auto* entry = region(id)) {
    return entry->name;
  }
  if (const auto* entry = initial_state.army(id)) {
    return entry->name;
  }
  if (const auto* entry = commander(id)) {
    return entry->name;
  }
  if (const auto* entry = faction(id)) {
    return entry->name;
  }
  if (const auto* entry = event(id)) {
    return entry->name;
  }
  for (const auto& entry : events) {
    for (const auto& effect : entry.effects) {
      if (effect.op == EffectOp::CreateArmy && effect.target == id &&
          !effect.name.isEmpty()) {
        return effect.name;
      }
    }
  }
  return id;
}

auto WorldDefinition::routes_at(const QString& site_id) const -> QVector<const Route*> {
  QVector<const Route*> out;
  for (const auto& entry : routes) {
    if (entry.touches(site_id)) {
      out.append(&entry);
    }
  }
  return out;
}

auto WorldDefinition::sites_in(const QString& region_id) const -> QVector<const Site*> {
  QVector<const Site*> out;
  for (const auto& entry : sites) {
    if (entry.region == region_id) {
      out.append(&entry);
    }
  }
  return out;
}

auto WorldDefinition::resolve_campaign_region(const QString& world_region_id) const
    -> const Region* {
  if (const auto* direct = region(world_region_id)) {
    return direct;
  }
  for (const auto& entry : regions) {
    for (const auto& district : entry.districts) {
      if (district.id == world_region_id) {
        return &entry;
      }
    }
  }
  return nullptr;
}

auto WorldDefinition::site_for_mission(const QString& mission) const -> const Site* {
  if (mission.isEmpty()) {
    return nullptr;
  }
  for (const auto& entry : sites) {
    if (entry.battlefield.mission == mission) {
      return &entry;
    }
  }
  return nullptr;
}

auto WorldDefinition::route_polyline(const Route& route) const -> QVector<LonLat> {
  QVector<LonLat> out;
  if (const auto* start = site(route.from)) {
    out.append(start->position);
  }
  out.append(route.waypoints);
  if (const auto* end = site(route.to)) {
    out.append(end->position);
  }
  return out;
}

auto WorldDefinition::route_geodesic_km(const Route& route) const -> double {
  const auto* start = site(route.from);
  const auto* end = site(route.to);
  if (start == nullptr || end == nullptr) {
    return 0.0;
  }
  return great_circle_km(start->position, end->position);
}

auto WorldDefinition::route_length_km(const Route& route) const -> double {
  if (route.length_km > 0.0) {
    return route.length_km;
  }
  const QVector<LonLat> line = route_polyline(route);
  double sum = 0.0;
  for (int i = 1; i < line.size(); ++i) {
    sum += great_circle_km(line[i - 1], line[i]);
  }
  const double factor = route.waypoints.isEmpty()
                            ? route_detour_factor(route.kind)
                            : 1.0 + ((route_detour_factor(route.kind) - 1.0) * 0.5);
  return sum * factor;
}

auto WorldDefinition::route_days(const Route& route) const -> double {
  return route_length_km(route) / march_speed_km_per_day(route.kind);
}

auto WorldDefinition::unique_id(const QString& stem) const -> QString {
  QString base = stem.trimmed().toLower();
  static const QRegularExpression k_non_id(QStringLiteral("[^a-z0-9_]+"));
  base.replace(k_non_id, QStringLiteral("_"));
  if (base.isEmpty()) {
    base = QStringLiteral("entity");
  }
  QString candidate = base;
  int suffix = 2;
  while (kind_of(candidate) != EntityKind::None) {
    candidate = QStringLiteral("%1_%2").arg(base).arg(suffix++);
  }
  return candidate;
}

auto site_controller(const WorldDefinition& def,
                     const WorldState& state,
                     const QString& site) -> QString {
  const auto it = state.sites.find(site);
  if (it != state.sites.end() && !it->second.controller.isEmpty()) {
    return it->second.controller;
  }
  if (const auto* entry = def.site(site)) {
    return region_controller(def, state, entry->region);
  }
  return QString::fromLatin1(k_neutral_faction);
}

auto region_controller(const WorldDefinition& /*def*/,
                       const WorldState& state,
                       const QString& region) -> QString {
  const auto it = state.regions.find(region);
  if (it != state.regions.end() && !it->second.controller.isEmpty()) {
    return it->second.controller;
  }
  return QString::fromLatin1(k_neutral_faction);
}

auto site_fortification(const WorldDefinition& def,
                        const WorldState& state,
                        const QString& site) -> int {
  const auto it = state.sites.find(site);
  if (it != state.sites.end() && it->second.fortification >= 0) {
    return it->second.fortification;
  }
  const auto* entry = def.site(site);
  return entry != nullptr ? entry->fortification : 0;
}

auto port_usable(const WorldDefinition& def,
                 const WorldState& state,
                 const QString& site) -> bool {
  const auto* entry = def.site(site);
  if (entry == nullptr || !entry->has_port()) {
    return false;
  }
  const auto it = state.sites.find(site);
  return it == state.sites.end() || it->second.port_open;
}

auto route_status(const WorldState& state, const QString& route) -> RouteStatus {
  const auto it = state.routes.find(route);
  return it == state.routes.end() ? RouteStatus::Open : it->second.status;
}

auto commander_status(const WorldState& state,
                      const QString& commander) -> CommanderStatus {
  const auto it = state.commanders.find(commander);
  return it == state.commanders.end() ? CommanderStatus::Active : it->second.status;
}

auto commander_faction(const WorldDefinition& def,
                       const WorldState& state,
                       const QString& commander) -> QString {
  const auto it = state.commanders.find(commander);
  if (it != state.commanders.end() && !it->second.faction.isEmpty()) {
    return it->second.faction;
  }
  const auto* record = def.commander(commander);
  return record != nullptr ? record->faction : QString();
}

auto are_hostile(const WorldDefinition& def,
                 const QString& faction_a,
                 const QString& faction_b) -> bool {
  if (faction_a.isEmpty() || faction_b.isEmpty() || faction_a == faction_b) {
    return false;
  }
  if (faction_a == QLatin1String(k_neutral_faction) ||
      faction_b == QLatin1String(k_neutral_faction)) {
    return false;
  }
  const auto* a = def.faction(faction_a);
  const auto* b = def.faction(faction_b);
  if ((a != nullptr && a->allies.contains(faction_b)) ||
      (b != nullptr && b->allies.contains(faction_a))) {
    return false;
  }
  return true;
}

auto are_friendly(const WorldDefinition& def,
                  const QString& faction_a,
                  const QString& faction_b) -> bool {
  if (faction_a.isEmpty() || faction_b.isEmpty()) {
    return false;
  }
  if (faction_a == faction_b) {
    return faction_a != QLatin1String(k_neutral_faction);
  }
  const auto* a = def.faction(faction_a);
  const auto* b = def.faction(faction_b);
  return (a != nullptr && a->allies.contains(faction_b)) ||
         (b != nullptr && b->allies.contains(faction_a));
}

auto region_is_contested(const WorldDefinition& def,
                         const WorldState& state,
                         const QString& region) -> bool {
  const auto it = state.regions.find(region);
  if (it != state.regions.end() && it->second.contested) {
    return true;
  }
  QSet<QString> armed;
  for (const auto& army : state.armies) {
    const auto* where = def.site(army.location);
    if (where != nullptr && where->region == region && army.soldiers() > 0) {
      armed.insert(army.faction);
    }
  }
  const QStringList present(armed.begin(), armed.end());
  for (int i = 0; i < present.size(); ++i) {
    for (int j = i + 1; j < present.size(); ++j) {
      if (are_hostile(def, present[i], present[j])) {
        return true;
      }
    }
  }
  return false;
}

auto army_commanded_by(const WorldState& state,
                       const QString& commander) -> const ArmyState* {
  if (commander.isEmpty()) {
    return nullptr;
  }
  for (const auto& army : state.armies) {
    if (army.commander == commander) {
      return &army;
    }
  }
  return nullptr;
}

} // namespace Game::Strategic
