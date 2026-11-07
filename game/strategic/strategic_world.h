#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <array>
#include <cstddef>
#include <map>
#include <optional>

#include "strategic_types.h"

namespace Game::Strategic {

struct Projection {
  double lon_min = -10.0;
  double lon_max = 18.0;
  double lat_min = 30.0;
  double lat_max = 47.5;

  [[nodiscard]] auto to_uv_u(const LonLat& p) const -> double {
    return (p.lon - lon_min) / (lon_max - lon_min);
  }
  [[nodiscard]] auto to_uv_v(const LonLat& p) const -> double {
    return (p.lat - lat_min) / (lat_max - lat_min);
  }
  [[nodiscard]] auto from_uv(double u, double v) const -> LonLat {
    return {lon_min + u * (lon_max - lon_min), lat_min + v * (lat_max - lat_min)};
  }
  [[nodiscard]] auto contains(const LonLat& p) const -> bool {
    return p.lon >= lon_min && p.lon <= lon_max && p.lat >= lat_min && p.lat <= lat_max;
  }
};

struct Faction {
  QString id;
  QString name;
  QString color;
  QString capital;
  QStringList allies;
};

struct District {
  QString id;
  QString name;
  QVector<LonLat> outline;
};

struct Region {
  QString id;
  QString name;
  QString historical_name;
  Terrain terrain = Terrain::Plains;
  int importance = 1;
  QStringList adjacent;
  QVector<District> districts;
  QStringList tags;
};

struct BattlefieldLink {
  QString map;
  QString mission;
  QString preset;

  [[nodiscard]] auto empty() const -> bool {
    return map.isEmpty() && mission.isEmpty() && preset.isEmpty();
  }
};

struct ResourceProfile {
  std::array<int, k_all_resources.size()> values{};

  [[nodiscard]] auto get(Resource resource) const -> int {
    return values[static_cast<std::size_t>(resource)];
  }
  void set(Resource resource, int value) {
    values[static_cast<std::size_t>(resource)] = value;
  }
  [[nodiscard]] auto total() const -> int;
};

struct Site {
  QString id;
  QString name;
  QString latin_name;
  QString region;
  QString district;
  SiteKind kind = SiteKind::Town;
  LonLat position;
  PopulationBand population = PopulationBand::Small;
  int fortification = 0;
  ResourceProfile resources;
  PortClass port = PortClass::None;
  Terrain terrain = Terrain::Plains;
  QStringList tags;
  BattlefieldLink battlefield;

  [[nodiscard]] auto has_port() const -> bool { return port != PortClass::None; }
};

struct Route {
  QString id;
  QString name;
  QString from;
  QString to;
  RouteKind kind = RouteKind::Road;
  double length_km = 0.0;
  int throughput = 3;
  bool chokepoint = false;
  QVector<LonLat> waypoints;
  QStringList tags;
  BattlefieldLink battlefield;

  [[nodiscard]] auto other_end(const QString& site) const -> QString {
    return site == from ? to : from;
  }
  [[nodiscard]] auto touches(const QString& site) const -> bool {
    return site == from || site == to;
  }
};

struct CommanderRecord {
  QString id;
  QString name;
  QString faction;
  QString catalog_id;
  QString title;
};

struct RegionState {
  QString controller;
  bool contested = false;

  friend auto operator==(const RegionState&, const RegionState&) -> bool = default;
};

struct SiteState {
  QString controller;
  int fortification = -1;
  bool port_open = true;
  int garrison = 0;

  friend auto operator==(const SiteState&, const SiteState&) -> bool = default;
};

struct RouteState {
  RouteStatus status = RouteStatus::Open;

  friend auto operator==(const RouteState&, const RouteState&) -> bool = default;
};

struct CommanderState {
  CommanderStatus status = CommanderStatus::Active;
  QString faction;

  friend auto operator==(const CommanderState&,
                         const CommanderState&) -> bool = default;
};

struct ArmyState {
  QString id;
  QString name;
  QString faction;
  QString commander;
  QString location;
  QString objective;
  int infantry = 0;
  int cavalry = 0;
  int elephants = 0;
  QStringList flags;

  [[nodiscard]] auto soldiers() const -> int { return infantry + cavalry; }
  [[nodiscard]] auto band() const -> StrengthBand {
    return strength_band_for(soldiers());
  }
  [[nodiscard]] auto supply_demand() const -> int {
    return infantry + (cavalry * 2) + (elephants * 12);
  }

  friend auto operator==(const ArmyState&, const ArmyState&) -> bool = default;
};

struct WorldState {
  QString world_id;
  QStringList applied_events;
  std::map<QString, RegionState> regions;
  std::map<QString, SiteState> sites;
  std::map<QString, RouteState> routes;
  std::map<QString, CommanderState> commanders;
  QVector<ArmyState> armies;
  std::map<QString, QString> flags;

  [[nodiscard]] auto army(const QString& id) const -> const ArmyState*;
  [[nodiscard]] auto army(const QString& id) -> ArmyState*;
  [[nodiscard]] auto armies_at(const QString& site) const -> QVector<const ArmyState*>;

  friend auto operator==(const WorldState&, const WorldState&) -> bool = default;
};

enum class EffectOp {
  SetController,
  SetContested,
  SetRouteStatus,
  SetPort,
  SetFortification,
  SetGarrison,
  MoveArmy,
  SetArmyStrength,
  ScaleArmy,
  CreateArmy,
  DisbandArmy,
  SetCommander,
  SetFlag
};

[[nodiscard]] auto to_string(EffectOp op) -> QString;
[[nodiscard]] auto
effect_op_from_string(const QString& text) -> std::optional<EffectOp>;

struct Effect {
  EffectOp op = EffectOp::SetFlag;
  QString target;
  QString faction;
  QString to;
  QString commander;
  QString army;
  QString name;
  QString value;
  std::optional<RouteStatus> route_status;
  std::optional<CommanderStatus> commander_status;
  std::optional<bool> enabled;
  std::optional<int> level;
  std::optional<int> infantry;
  std::optional<int> cavalry;
  std::optional<int> elephants;
  std::optional<double> factor;
};

struct WorldEvent {
  QString id;
  QString name;
  int year = 0;
  Season season = Season::Spring;
  QString mission;
  QString summary;
  QString site;
  QVector<Effect> effects;

  [[nodiscard]] auto date_label() const -> QString;
  [[nodiscard]] auto chronological_key() const -> int {
    return (year * 4) + static_cast<int>(season);
  }
};

struct LoadIssue {
  QString entity;
  QString message;
};

enum class EntityKind {
  None,
  Faction,
  Region,
  Site,
  Route,
  Army,
  Commander,
  Event
};

[[nodiscard]] auto to_string(EntityKind kind) -> QString;

struct WorldDefinition {
  int schema_version = k_definition_schema_version;
  QString id;
  QString name;
  QString campaign;
  QString start_label;
  Projection projection;
  QString geometry_source;
  QString player_faction;
  QVector<Faction> factions;
  QVector<Region> regions;
  QVector<Site> sites;
  QVector<Route> routes;
  QVector<CommanderRecord> commanders;
  QVector<WorldEvent> events;
  WorldState initial_state;
  QVector<LoadIssue> load_issues;

  [[nodiscard]] auto faction(const QString& id) const -> const Faction*;
  [[nodiscard]] auto region(const QString& id) const -> const Region*;
  [[nodiscard]] auto site(const QString& id) const -> const Site*;
  [[nodiscard]] auto route(const QString& id) const -> const Route*;
  [[nodiscard]] auto commander(const QString& id) const -> const CommanderRecord*;
  [[nodiscard]] auto event(const QString& id) const -> const WorldEvent*;

  [[nodiscard]] auto site_mut(const QString& id) -> Site*;
  [[nodiscard]] auto route_mut(const QString& id) -> Route*;
  [[nodiscard]] auto region_mut(const QString& id) -> Region*;

  [[nodiscard]] auto kind_of(const QString& id) const -> EntityKind;
  [[nodiscard]] auto display_name(const QString& id) const -> QString;
  [[nodiscard]] auto routes_at(const QString& site) const -> QVector<const Route*>;
  [[nodiscard]] auto sites_in(const QString& region) const -> QVector<const Site*>;
  [[nodiscard]] auto
  resolve_campaign_region(const QString& world_region_id) const -> const Region*;
  [[nodiscard]] auto site_for_mission(const QString& mission) const -> const Site*;
  [[nodiscard]] auto route_length_km(const Route& route) const -> double;
  [[nodiscard]] auto route_geodesic_km(const Route& route) const -> double;
  [[nodiscard]] auto route_polyline(const Route& route) const -> QVector<LonLat>;
  [[nodiscard]] auto route_days(const Route& route) const -> double;

  [[nodiscard]] auto unique_id(const QString& stem) const -> QString;

private:
  template <typename T>
  [[nodiscard]] auto find_in(const QVector<T>& items,
                             QHash<QString, int>& index,
                             const QString& id) const -> const T*;

  mutable QHash<QString, int> m_site_index;
  mutable QHash<QString, int> m_route_index;
  mutable QHash<QString, int> m_region_index;
};

[[nodiscard]] auto site_controller(const WorldDefinition& def,
                                   const WorldState& state,
                                   const QString& site) -> QString;
[[nodiscard]] auto region_controller(const WorldDefinition& def,
                                     const WorldState& state,
                                     const QString& region) -> QString;
[[nodiscard]] auto site_fortification(const WorldDefinition& def,
                                      const WorldState& state,
                                      const QString& site) -> int;
[[nodiscard]] auto port_usable(const WorldDefinition& def,
                               const WorldState& state,
                               const QString& site) -> bool;
[[nodiscard]] auto route_status(const WorldState& state,
                                const QString& route) -> RouteStatus;
[[nodiscard]] auto commander_status(const WorldState& state,
                                    const QString& commander) -> CommanderStatus;
[[nodiscard]] auto commander_faction(const WorldDefinition& def,
                                     const WorldState& state,
                                     const QString& commander) -> QString;
[[nodiscard]] auto are_hostile(const WorldDefinition& def,
                               const QString& faction_a,
                               const QString& faction_b) -> bool;
[[nodiscard]] auto are_friendly(const WorldDefinition& def,
                                const QString& faction_a,
                                const QString& faction_b) -> bool;
[[nodiscard]] auto region_is_contested(const WorldDefinition& def,
                                       const WorldState& state,
                                       const QString& region) -> bool;
[[nodiscard]] auto army_commanded_by(const WorldState& state,
                                     const QString& commander) -> const ArmyState*;

} // namespace Game::Strategic
