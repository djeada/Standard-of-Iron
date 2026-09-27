#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>

#include "building_collapse.h"

namespace Render::GL {

class ISubmitter;

struct BuildingWorksite {
  QVector3D base{0.0F, 0.0F, 0.0F};
  float yaw_degrees{0.0F};
  BuildingCollapseFootprint footprint{};
  std::uint32_t seed{0U};
};

[[nodiscard]] auto
building_worksite_for(const Engine::Core::World& world,
                      Engine::Core::EntityID entity_id) -> BuildingWorksite;

[[nodiscard]] auto construction_built_fraction(float progress) noexcept -> float;
[[nodiscard]] auto construction_scaffold_fraction(float progress) noexcept -> float;
[[nodiscard]] auto dismantle_standing_fraction(float progress) noexcept -> float;

[[nodiscard]] auto building_standing_model(const QMatrix4x4& model,
                                           const QVector3D& base,
                                           float standing) -> QMatrix4x4;

void submit_foundation_curb(ISubmitter& out, const BuildingWorksite& site);
void submit_scaffolding(ISubmitter& out, const BuildingWorksite& site, float raised);
void submit_salvage_stacks(ISubmitter& out,
                           const BuildingWorksite& site,
                           float progress);

void submit_worksite_dust(ISubmitter& out,
                          const BuildingWorksite& site,
                          float strength,
                          float animation_time);

void submit_structure_work_dressing(ISubmitter& out,
                                    const Engine::Core::World& world,
                                    Engine::Core::EntityID entity_id,
                                    float animation_time);

[[nodiscard]] auto structure_work_model(const QMatrix4x4& model,
                                        const Engine::Core::World& world,
                                        Engine::Core::EntityID entity_id) -> QMatrix4x4;

} // namespace Render::GL
