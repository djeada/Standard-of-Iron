#pragma once

#include <QString>
#include <QStringList>

#include <array>
#include <optional>

namespace Game::Strategic {

inline constexpr int k_definition_schema_version = 1;
inline constexpr int k_state_format_version = 1;
inline constexpr const char* k_state_format_tag = "soi.strategic_state";

inline constexpr const char* k_neutral_faction = "neutral";

struct LonLat {
  double lon = 0.0;
  double lat = 0.0;

  friend auto operator==(const LonLat&, const LonLat&) -> bool = default;
};

enum class SiteKind {
  City,
  Town,
  Colony,
  Fort,
  Camp,
  Pass,
  Crossing,
  Field
};

enum class RouteKind {
  MajorRoad,
  Road,
  Track,
  MountainPass,
  RiverCrossing,
  Coastal,
  SeaLane
};

enum class Terrain {
  Plains,
  Hills,
  Mountains,
  Forest,
  Marsh,
  Coast,
  River,
  Desert
};

enum class PortClass {
  None,
  Minor,
  Major
};

enum class PopulationBand {
  None,
  Hamlet,
  Small,
  Medium,
  Large,
  Metropolis
};

enum class RouteStatus {
  Open,
  Contested,
  Blocked
};

enum class CommanderStatus {
  Active,
  Wounded,
  Recalled,
  Captured,
  Dead
};

enum class Season {
  Spring,
  Summer,
  Autumn,
  Winter
};

enum class SupplyLevel {
  Supplied,
  Stretched,
  Foraging,
  CutOff
};

enum class StrengthBand {
  Broken,
  Depleted,
  Weakened,
  Strong,
  Grand
};

enum class Resource {
  Food,
  Manpower,
  Timber,
  Stone,
  Iron
};

inline constexpr std::array<Resource, 5> k_all_resources = {Resource::Food,
                                                            Resource::Manpower,
                                                            Resource::Timber,
                                                            Resource::Stone,
                                                            Resource::Iron};

inline constexpr int k_resource_max = 5;
inline constexpr int k_fortification_max = 3;
inline constexpr int k_throughput_max = 5;
inline constexpr int k_importance_max = 5;

[[nodiscard]] auto to_string(SiteKind kind) -> QString;
[[nodiscard]] auto to_string(RouteKind kind) -> QString;
[[nodiscard]] auto to_string(Terrain terrain) -> QString;
[[nodiscard]] auto to_string(PortClass port) -> QString;
[[nodiscard]] auto to_string(PopulationBand band) -> QString;
[[nodiscard]] auto to_string(RouteStatus status) -> QString;
[[nodiscard]] auto to_string(CommanderStatus status) -> QString;
[[nodiscard]] auto to_string(Season season) -> QString;
[[nodiscard]] auto to_string(SupplyLevel level) -> QString;
[[nodiscard]] auto to_string(StrengthBand band) -> QString;
[[nodiscard]] auto to_string(Resource resource) -> QString;

[[nodiscard]] auto
site_kind_from_string(const QString& text) -> std::optional<SiteKind>;
[[nodiscard]] auto
route_kind_from_string(const QString& text) -> std::optional<RouteKind>;
[[nodiscard]] auto terrain_from_string(const QString& text) -> std::optional<Terrain>;
[[nodiscard]] auto
port_class_from_string(const QString& text) -> std::optional<PortClass>;
[[nodiscard]] auto
population_from_string(const QString& text) -> std::optional<PopulationBand>;
[[nodiscard]] auto
route_status_from_string(const QString& text) -> std::optional<RouteStatus>;
[[nodiscard]] auto
commander_status_from_string(const QString& text) -> std::optional<CommanderStatus>;
[[nodiscard]] auto season_from_string(const QString& text) -> std::optional<Season>;
[[nodiscard]] auto resource_from_string(const QString& text) -> std::optional<Resource>;

[[nodiscard]] auto all_site_kind_names() -> QStringList;
[[nodiscard]] auto all_route_kind_names() -> QStringList;
[[nodiscard]] auto all_terrain_names() -> QStringList;

[[nodiscard]] auto is_settlement(SiteKind kind) -> bool;
[[nodiscard]] auto is_land_route(RouteKind kind) -> bool;

[[nodiscard]] auto march_speed_km_per_day(RouteKind kind) -> double;

[[nodiscard]] auto route_detour_factor(RouteKind kind) -> double;

[[nodiscard]] auto strength_band_for(int soldiers) -> StrengthBand;

[[nodiscard]] auto great_circle_km(const LonLat& a, const LonLat& b) -> double;

[[nodiscard]] auto format_year(int year) -> QString;

} // namespace Game::Strategic
