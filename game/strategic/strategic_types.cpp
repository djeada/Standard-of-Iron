#include "strategic_types.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>

namespace Game::Strategic {

namespace {

template <typename Enum, std::size_t N>
using NameTable = std::array<std::pair<Enum, const char*>, N>;

constexpr NameTable<SiteKind, 8> k_site_kinds = {{
    {SiteKind::City, "city"},
    {SiteKind::Town, "town"},
    {SiteKind::Colony, "colony"},
    {SiteKind::Fort, "fort"},
    {SiteKind::Camp, "camp"},
    {SiteKind::Pass, "pass"},
    {SiteKind::Crossing, "crossing"},
    {SiteKind::Field, "field"},
}};

constexpr NameTable<RouteKind, 7> k_route_kinds = {{
    {RouteKind::MajorRoad, "major_road"},
    {RouteKind::Road, "road"},
    {RouteKind::Track, "track"},
    {RouteKind::MountainPass, "mountain_pass"},
    {RouteKind::RiverCrossing, "river_crossing"},
    {RouteKind::Coastal, "coastal"},
    {RouteKind::SeaLane, "sea_lane"},
}};

constexpr NameTable<Terrain, 8> k_terrains = {{
    {Terrain::Plains, "plains"},
    {Terrain::Hills, "hills"},
    {Terrain::Mountains, "mountains"},
    {Terrain::Forest, "forest"},
    {Terrain::Marsh, "marsh"},
    {Terrain::Coast, "coast"},
    {Terrain::River, "river"},
    {Terrain::Desert, "desert"},
}};

constexpr NameTable<PortClass, 3> k_ports = {{
    {PortClass::None, "none"},
    {PortClass::Minor, "minor"},
    {PortClass::Major, "major"},
}};

constexpr NameTable<PopulationBand, 6> k_populations = {{
    {PopulationBand::None, "none"},
    {PopulationBand::Hamlet, "hamlet"},
    {PopulationBand::Small, "small"},
    {PopulationBand::Medium, "medium"},
    {PopulationBand::Large, "large"},
    {PopulationBand::Metropolis, "metropolis"},
}};

constexpr NameTable<RouteStatus, 3> k_route_statuses = {{
    {RouteStatus::Open, "open"},
    {RouteStatus::Contested, "contested"},
    {RouteStatus::Blocked, "blocked"},
}};

constexpr NameTable<CommanderStatus, 5> k_commander_statuses = {{
    {CommanderStatus::Active, "active"},
    {CommanderStatus::Wounded, "wounded"},
    {CommanderStatus::Recalled, "recalled"},
    {CommanderStatus::Captured, "captured"},
    {CommanderStatus::Dead, "dead"},
}};

constexpr NameTable<Season, 4> k_seasons = {{
    {Season::Spring, "spring"},
    {Season::Summer, "summer"},
    {Season::Autumn, "autumn"},
    {Season::Winter, "winter"},
}};

constexpr NameTable<SupplyLevel, 4> k_supply_levels = {{
    {SupplyLevel::Supplied, "supplied"},
    {SupplyLevel::Stretched, "stretched"},
    {SupplyLevel::Foraging, "foraging"},
    {SupplyLevel::CutOff, "cut_off"},
}};

constexpr NameTable<StrengthBand, 5> k_strength_bands = {{
    {StrengthBand::Broken, "broken"},
    {StrengthBand::Depleted, "depleted"},
    {StrengthBand::Weakened, "weakened"},
    {StrengthBand::Strong, "strong"},
    {StrengthBand::Grand, "grand"},
}};

constexpr NameTable<Resource, 5> k_resources = {{
    {Resource::Food, "food"},
    {Resource::Manpower, "manpower"},
    {Resource::Timber, "timber"},
    {Resource::Stone, "stone"},
    {Resource::Iron, "iron"},
}};

template <typename Enum, std::size_t N>
auto name_of(const NameTable<Enum, N>& table, Enum value) -> QString {
  for (const auto& [entry, name] : table) {
    if (entry == value) {
      return QString::fromLatin1(name);
    }
  }
  return {};
}

template <typename Enum, std::size_t N>
auto parse(const NameTable<Enum, N>& table,
           const QString& text) -> std::optional<Enum> {
  const QString key = text.trimmed().toLower();
  for (const auto& [entry, name] : table) {
    if (key == QLatin1String(name)) {
      return entry;
    }
  }
  return std::nullopt;
}

template <typename Enum, std::size_t N>
auto names(const NameTable<Enum, N>& table) -> QStringList {
  QStringList out;
  for (const auto& [entry, name] : table) {
    out.append(QString::fromLatin1(name));
  }
  return out;
}

} // namespace

auto to_string(SiteKind kind) -> QString {
  return name_of(k_site_kinds, kind);
}
auto to_string(RouteKind kind) -> QString {
  return name_of(k_route_kinds, kind);
}
auto to_string(Terrain terrain) -> QString {
  return name_of(k_terrains, terrain);
}
auto to_string(PortClass port) -> QString {
  return name_of(k_ports, port);
}
auto to_string(PopulationBand band) -> QString {
  return name_of(k_populations, band);
}
auto to_string(RouteStatus status) -> QString {
  return name_of(k_route_statuses, status);
}
auto to_string(CommanderStatus status) -> QString {
  return name_of(k_commander_statuses, status);
}
auto to_string(Season season) -> QString {
  return name_of(k_seasons, season);
}
auto to_string(SupplyLevel level) -> QString {
  return name_of(k_supply_levels, level);
}
auto to_string(StrengthBand band) -> QString {
  return name_of(k_strength_bands, band);
}
auto to_string(Resource resource) -> QString {
  return name_of(k_resources, resource);
}

auto site_kind_from_string(const QString& text) -> std::optional<SiteKind> {
  return parse(k_site_kinds, text);
}
auto route_kind_from_string(const QString& text) -> std::optional<RouteKind> {
  return parse(k_route_kinds, text);
}
auto terrain_from_string(const QString& text) -> std::optional<Terrain> {
  return parse(k_terrains, text);
}
auto port_class_from_string(const QString& text) -> std::optional<PortClass> {
  return parse(k_ports, text);
}
auto population_from_string(const QString& text) -> std::optional<PopulationBand> {
  return parse(k_populations, text);
}
auto route_status_from_string(const QString& text) -> std::optional<RouteStatus> {
  return parse(k_route_statuses, text);
}
auto commander_status_from_string(const QString& text)
    -> std::optional<CommanderStatus> {
  return parse(k_commander_statuses, text);
}
auto season_from_string(const QString& text) -> std::optional<Season> {
  return parse(k_seasons, text);
}
auto resource_from_string(const QString& text) -> std::optional<Resource> {
  return parse(k_resources, text);
}

auto all_site_kind_names() -> QStringList {
  return names(k_site_kinds);
}
auto all_route_kind_names() -> QStringList {
  return names(k_route_kinds);
}
auto all_terrain_names() -> QStringList {
  return names(k_terrains);
}

auto is_settlement(SiteKind kind) -> bool {
  switch (kind) {
  case SiteKind::City:
  case SiteKind::Town:
  case SiteKind::Colony:
  case SiteKind::Fort:
    return true;
  case SiteKind::Camp:
  case SiteKind::Pass:
  case SiteKind::Crossing:
  case SiteKind::Field:
    return false;
  }
  return false;
}

auto is_land_route(RouteKind kind) -> bool {
  return kind != RouteKind::SeaLane;
}

auto march_speed_km_per_day(RouteKind kind) -> double {
  switch (kind) {
  case RouteKind::MajorRoad:
    return 26.0;
  case RouteKind::Road:
    return 20.0;
  case RouteKind::Track:
    return 15.0;
  case RouteKind::MountainPass:
    return 12.0;
  case RouteKind::RiverCrossing:
    return 12.0;
  case RouteKind::Coastal:
    return 18.0;
  case RouteKind::SeaLane:
    return 90.0;
  }
  return 15.0;
}

auto route_detour_factor(RouteKind kind) -> double {
  switch (kind) {
  case RouteKind::MajorRoad:
    return 1.15;
  case RouteKind::Road:
  case RouteKind::RiverCrossing:
    return 1.25;
  case RouteKind::Track:
  case RouteKind::Coastal:
    return 1.3;
  case RouteKind::MountainPass:
    return 1.35;
  case RouteKind::SeaLane:
    return 1.1;
  }
  return 1.25;
}

auto strength_band_for(int soldiers) -> StrengthBand {
  if (soldiers >= 40000) {
    return StrengthBand::Grand;
  }
  if (soldiers >= 15000) {
    return StrengthBand::Strong;
  }
  if (soldiers >= 6000) {
    return StrengthBand::Weakened;
  }
  if (soldiers >= 1500) {
    return StrengthBand::Depleted;
  }
  return StrengthBand::Broken;
}

auto great_circle_km(const LonLat& a, const LonLat& b) -> double {
  constexpr double k_earth_radius_km = 6371.0;
  constexpr double k_deg = std::numbers::pi / 180.0;
  const double lat1 = a.lat * k_deg;
  const double lat2 = b.lat * k_deg;
  const double dlat = (b.lat - a.lat) * k_deg;
  const double dlon = (b.lon - a.lon) * k_deg;
  const double h =
      std::sin(dlat / 2.0) * std::sin(dlat / 2.0) +
      std::cos(lat1) * std::cos(lat2) * std::sin(dlon / 2.0) * std::sin(dlon / 2.0);
  return 2.0 * k_earth_radius_km * std::asin(std::min(1.0, std::sqrt(h)));
}

auto format_year(int year) -> QString {
  if (year < 0) {
    return QStringLiteral("%1 BC").arg(-year);
  }
  return QStringLiteral("AD %1").arg(year);
}

} // namespace Game::Strategic
