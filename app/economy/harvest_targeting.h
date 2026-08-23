#pragma once

#include <QPointF>
#include <QString>
#include <QVector3D>

#include <cstdint>
#include <optional>
#include <unordered_set>
#include <vector>

#include "game/core/entity.h"
#include "game/map/terrain_service.h"
#include "game/systems/nav_grid_types.h"

struct ViewportState;

namespace Engine::Core {
class World;
}

namespace Render::GL {
class Camera;
}

namespace App::Economy {

enum class HarvestTargetKind {
  Tree,
  Boulder,
  IronOre,
};

struct HarvestPlacement {
  HarvestTargetKind kind = HarvestTargetKind::Tree;
  std::optional<Game::Map::WorldPropTarget> target;
  std::optional<QVector3D> work_position;
  Engine::Core::EntityID builder_id = 0;
  QString failure_reason;

  [[nodiscard]] auto valid() const -> bool {
    return target.has_value() && work_position.has_value() && builder_id != 0;
  }
};

struct ResolvedHarvestTarget {
  HarvestTargetKind kind = HarvestTargetKind::Tree;
  Game::Map::WorldPropTarget target;
};

struct HarvestResourceCell {
  HarvestTargetKind kind = HarvestTargetKind::Tree;
  Game::Systems::Point grid;
};

using CrewClaims = std::unordered_set<std::uint64_t>;

[[nodiscard]] auto is_collect_item(const QString& item_type) -> bool;
[[nodiscard]] auto is_harvest_construction_item(const QString& item_type) -> bool;
[[nodiscard]] auto generic_collect_failure_reason() -> QString;
[[nodiscard]] auto harvest_product_type(HarvestTargetKind kind) -> const char*;

[[nodiscard]] auto
crew_claims(Engine::Core::World* world,
            const std::vector<Engine::Core::EntityID>& builder_ids) -> CrewClaims;

[[nodiscard]] auto
evaluate_harvest_placement(Engine::Core::World* world,
                           const std::vector<Engine::Core::EntityID>& builder_ids,
                           const QVector3D& world_position,
                           const QString& item_type,
                           std::uint64_t preferred_target_id = 0) -> HarvestPlacement;

[[nodiscard]] auto resolve_harvest_target_at_position(
    Game::Map::TerrainService& terrain_service,
    const QString& item_type,
    const QVector3D& world_position,
    const CrewClaims& claims,
    std::uint64_t preferred_target_id = 0) -> std::optional<ResolvedHarvestTarget>;

[[nodiscard]] auto prop_taken(const Game::Map::TerrainService& terrain_service,
                              std::uint64_t prop_id,
                              const CrewClaims& claims) -> bool;

[[nodiscard]] auto resolve_harvest_target_from_screen(
    Game::Map::TerrainService& terrain_service,
    const QString& item_type,
    const Render::GL::Camera& camera,
    const ViewportState& viewport,
    const QPointF& screen_point,
    const CrewClaims& claims) -> std::optional<ResolvedHarvestTarget>;

} // namespace App::Economy
