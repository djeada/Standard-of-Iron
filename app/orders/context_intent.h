#pragma once

#include <QString>
#include <QVariantMap>
#include <QVector3D>

#include <cstdint>
#include <vector>

#include "app/input/cursor_mode.h"
#include "game/core/entity_id.h"

namespace Engine::Core {
class World;
}

namespace App::Core {

enum class ContextIntent : std::uint8_t {
  Invalid,
  Move,
  Attack,
  Interact,
  SetRally
};

[[nodiscard]] auto context_intent_name(ContextIntent intent) -> const char*;

struct ContextIntentRequest {
  Engine::Core::World* world = nullptr;
  const std::vector<Engine::Core::EntityID>* selection = nullptr;
  int local_owner_id = 0;
  CursorMode cursor_mode = CursorMode::Normal;
  bool spectator_mode = false;
  bool placing_construction = false;
  bool placing_formation = false;

  Engine::Core::EntityID hovered_entity_id = 0;
  bool hovered_is_enemy_unit = false;

  bool interaction_available = false;
  QString interaction_product_type;

  bool has_ground = false;
  QVector3D ground;
  bool ground_is_walkable = true;
};

struct ContextIntentResolution {
  ContextIntent intent = ContextIntent::Invalid;
  Engine::Core::EntityID target = 0;
  bool has_position = false;
  QVector3D position;
  QString reason;

  [[nodiscard]] auto valid() const -> bool { return intent != ContextIntent::Invalid; }
};

[[nodiscard]] auto
resolve_context_intent(const ContextIntentRequest& request) -> ContextIntentResolution;

[[nodiscard]] auto
to_variant_map(const ContextIntentResolution& resolution) -> QVariantMap;

} // namespace App::Core
