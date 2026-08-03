#pragma once

#include <QString>
#include <QVariantList>

#include <cstdint>
#include <vector>

#include "../../game/systems/unit_activity.h"

namespace App::Models {

// One unit's reported activity, before any grouping.
struct ActivityMarkerSource {
  std::uint64_t entity_id = 0;
  Game::Systems::UnitActivity activity;
  bool selected = false;
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
};

// What actually gets drawn above the battlefield: either one unit or a cluster
// of units doing the same thing in the same place.
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
  // Two units doing the same job this close together share one marker.
  float cluster_radius = 6.0F;
  // Above this many markers the display stops being readable, so same-activity
  // clusters are merged further until it fits.
  int max_markers = 14;
};

// A marching soldier does not need a banner over its head; a stalled builder
// does. Selection widens the rule because the player has explicitly asked about
// those units.
[[nodiscard]] auto activity_deserves_marker(const Game::Systems::UnitActivity& activity,
                                            bool selected) -> bool;

[[nodiscard]] auto group_activity_markers(
    const std::vector<ActivityMarkerSource>& sources,
    const ActivityMarkerOptions& options = {}) -> std::vector<ActivityMarker>;

} // namespace App::Models
