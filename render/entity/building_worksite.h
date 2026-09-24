#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>

#include "building_collapse.h"

namespace Engine::Core {
class Entity;
}

namespace Render::GL {

class ISubmitter;

// Where a structure stands and how big it is, in world units. Construction,
// repair and dismantling all dress the same footprint, each in its own way so
// the three read apart at gameplay zoom:
//   construction  a stone curb, then scaffolding while the building rises in it;
//   repair        scaffolding around an intact building, dust as courses go in;
//   dismantling   no scaffolding: the building is taken down from the top and
//                 its timber and stone are stacked neatly beside it.
struct BuildingWorksite {
  QVector3D base{0.0F, 0.0F, 0.0F};
  float yaw_degrees{0.0F};
  BuildingCollapseFootprint footprint{};
  std::uint32_t seed{0U};
};

[[nodiscard]] auto
building_worksite_for(const Engine::Core::Entity& entity) -> BuildingWorksite;

// How much of the building stands while it is built. The lowest courses go in
// before any scaffolding rises; the last ones as it comes down.
[[nodiscard]] auto construction_built_fraction(float progress) noexcept -> float;
[[nodiscard]] auto construction_scaffold_fraction(float progress) noexcept -> float;
[[nodiscard]] auto dismantle_standing_fraction(float progress) noexcept -> float;

// Presses the drawn building down onto its base to `standing` of its height.
[[nodiscard]] auto building_standing_model(const QMatrix4x4& model,
                                           const QVector3D& base,
                                           float standing) -> QMatrix4x4;

void submit_foundation_curb(ISubmitter& out, const BuildingWorksite& site);
void submit_scaffolding(ISubmitter& out, const BuildingWorksite& site, float raised);
void submit_salvage_stacks(ISubmitter& out,
                           const BuildingWorksite& site,
                           float progress);

// Dust thrown off by a crew at work: a restored course, a stripped roof.
void submit_worksite_dust(ISubmitter& out,
                          const BuildingWorksite& site,
                          float strength,
                          float animation_time);

// Everything a live structure shows for the work being done on it this frame:
// repair scaffolding, dismantling stacks, and the puff that covers a swap
// between damage states. Called once per structure per frame.
void submit_structure_work_dressing(ISubmitter& out,
                                    const Engine::Core::Entity& entity,
                                    float animation_time);

// The model a live structure is drawn with: pressed down while it is being
// dismantled, otherwise unchanged.
[[nodiscard]] auto
structure_work_model(const QMatrix4x4& model,
                     const Engine::Core::Entity& entity) -> QMatrix4x4;

} // namespace Render::GL
