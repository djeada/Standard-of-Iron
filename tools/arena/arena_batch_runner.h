#pragma once

#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

#include "game/map/environment_lighting.h"
#include "render/graphics_settings.h"

class ArenaViewport;

namespace Arena::Batch {

struct TerrainReviewEntry {
  QString id;
  QString map_path;
};

struct TerrainReviewSettings {
  QString artifact_root;
  int fps = 60;
  float duration = 0.0F;
  float capture_interval = 1.0F;
  float promo_distance_scale = 1.0F;
  float promo_tilt_deg = 0.0F;
  std::optional<Game::Map::TimeOfDay> forced_time_of_day;
};

struct ScenarioBatchSettings {
  QString artifact_root;
  int fps = 60;
  int seed = 1337;
  float duration = 0.0F;
  float capture_interval = 1.0F;
  float watchdog_multiplier = 3.0F;
  bool detailed_profiling = false;
  float environment_hour = 13.0F;
  bool environment_hour_forced = false;
  QString lighting_profile;
  std::optional<Render::GraphicsQuality> graphics_quality_override;
};

[[nodiscard]] auto resolve_terrain_review_path(const QString& path) -> QString;
[[nodiscard]] auto
campaign_terrain_review_entries(QString* error) -> std::vector<TerrainReviewEntry>;

[[nodiscard]] auto graphics_quality_name(Render::GraphicsQuality quality) -> QString;

void start_terrain_review(ArenaViewport& viewport,
                          std::vector<TerrainReviewEntry> entries,
                          const TerrainReviewSettings& settings);

void start_scenario_batch(ArenaViewport& viewport,
                          const QStringList& scenario_ids,
                          const ScenarioBatchSettings& settings);

} // namespace Arena::Batch
