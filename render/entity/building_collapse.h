#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>

#include "game/core/component_gameplay.h"
#include "game/units/spawn_type.h"

namespace Engine::Core {
class Entity;
}

namespace Render::GL {

class ISubmitter;

// World-space size of what comes down when a structure dies. Only drives the
// debris and dust around the ruin; the ruin itself is the renderer's own
// Destroyed-state mesh, pressed flat by building_collapse_model.
struct BuildingCollapseFootprint {
  float half_width{2.0F};
  float half_depth{2.0F};
  float height{2.6F};
};

// A structure at 0 HP plays the Structure death sequence (see
// game/core/death_sequence.h): Dying is the collapse, DeadHold the rubble
// resting, Sinking the rubble settling into the ground before removal.
struct BuildingCollapse {
  bool active{false};
  Engine::Core::DeathSequenceState state{Engine::Core::DeathSequenceState::Dying};
  float elapsed{0.0F};
  float duration{1.0F};
  float settled_for{0.0F};
  float shudder{0.0F};
  float fall{0.0F};
  float sink{0.0F};
  float heading{0.0F};
  float yaw_degrees{0.0F};
  std::uint32_t seed{0U};
  QVector3D base{0.0F, 0.0F, 0.0F};
  BuildingCollapseFootprint footprint{};
};

inline constexpr float k_collapse_shudder_fraction = 0.16F;
inline constexpr float k_collapse_remaining_height = 0.24F;

[[nodiscard]] auto building_collapse_footprint(Game::Units::SpawnType type) noexcept
    -> BuildingCollapseFootprint;

[[nodiscard]] auto
resolve_building_collapse(const Engine::Core::Entity& entity) -> BuildingCollapse;

[[nodiscard]] auto
building_collapse_model(const QMatrix4x4& model,
                        const BuildingCollapse& collapse) -> QMatrix4x4;

[[nodiscard]] auto
building_collapse_sink_depth(const BuildingCollapse& collapse) noexcept -> float;

void submit_building_collapse_rubble(ISubmitter& out, const BuildingCollapse& collapse);

void submit_building_collapse_effects(ISubmitter& out,
                                      const BuildingCollapse& collapse,
                                      float animation_time);

} // namespace Render::GL
