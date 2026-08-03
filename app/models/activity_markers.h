#pragma once

#include <QString>
#include <QVariantList>

#include <cstdint>
#include <vector>

#include "../../game/systems/unit_activity.h"

namespace App::Models {

struct ActivityMarkerSource {
  std::uint64_t entity_id = 0;
  Game::Systems::UnitActivity activity;
  bool selected = false;
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
};

struct ActivityMarker {
  QString activity;
  QString state;
  int count = 1;
  std::uint64_t lead_entity_id = 0;
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
};

struct ActivityMarkerOptions {

  float cluster_radius = 6.0F;

  int max_markers = 14;
};

[[nodiscard]] auto activity_deserves_marker(const Game::Systems::UnitActivity& activity,
                                            bool selected) -> bool;

[[nodiscard]] auto group_activity_markers(
    const std::vector<ActivityMarkerSource>& sources,
    const ActivityMarkerOptions& options = {}) -> std::vector<ActivityMarker>;

} // namespace App::Models
