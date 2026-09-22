#pragma once

#include <QVector3D>

#include <optional>
#include <vector>

#include "app/orders/order_markers.h"
#include "game/core/entity_id.h"
#include "game/systems/attack_targeting.h"
#include "game/systems/interaction_targeting.h"
#include "game/systems/target_focus.h"
#include "scene/camera.h"

namespace App::Core {

struct PresentationFrame {
  Render::GL::Camera camera;
  std::vector<Engine::Core::EntityID> selected_ids;
  Game::Systems::AttackTargetingHighlights attack_targeting;
  Game::Systems::InteractionTargetingHighlights interaction_targeting;
  std::vector<Game::Systems::AttackRangeRing> attack_range_rings;
  std::vector<OrderMarker> order_markers;
  std::vector<Game::Systems::TargetFocusMarker> target_focus;
  std::optional<QVector3D> objective_marker;
  std::optional<QVector3D> commander_rally_preview_pos;
  int local_owner_id{0};
  bool spectator_mode{false};
  bool has_camera{false};
};

} // namespace App::Core
