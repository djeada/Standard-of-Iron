

#pragma once

#include "creature_frame.h"
#include "equipment_registry.h"
#include "unit_visual_spec.h"

#include <cstdint>
#include <span>

namespace Engine::Core {
class World;
}

namespace Render::GL {
class ISubmitter;
}

namespace Render::Creature::Pipeline {

struct FrameContext {
  std::uint32_t frame_index{0};
  float view_distance_full{20.0F};
  float view_distance_reduced{40.0F};
  float view_distance_minimal{80.0F};
};

struct SubmitStats {
  std::uint32_t entities_submitted{0};
  std::uint32_t equipment_submitted{0};
  std::uint32_t lod_full{0};
  std::uint32_t lod_reduced{0};
  std::uint32_t lod_minimal{0};
  std::uint32_t lod_billboard{0};

  void reset() noexcept { *this = SubmitStats{}; }

  void operator+=(const SubmitStats &other) noexcept {
    entities_submitted += other.entities_submitted;
    equipment_submitted += other.equipment_submitted;
    lod_full += other.lod_full;
    lod_reduced += other.lod_reduced;
    lod_minimal += other.lod_minimal;
    lod_billboard += other.lod_billboard;
  }
};

class CreaturePipeline {
public:
  CreaturePipeline() = default;

  void gather(const Engine::Core::World &world, const FrameContext &ctx,
              std::span<const UnitVisualSpec> specs, CreatureFrame &out) const;

  auto process(const FrameContext &ctx, std::span<const UnitVisualSpec> specs,
               CreatureFrame &frame) const -> SubmitStats;

  auto submit(const FrameContext &ctx, std::span<const UnitVisualSpec> specs,
              const CreatureFrame &frame,
              Render::GL::ISubmitter &out) const -> SubmitStats;
};

[[nodiscard]] auto pick_lod(const FrameContext &ctx,
                            float distance) noexcept -> CreatureLOD;

[[nodiscard]] auto compose_equipment_world(
    const QMatrix4x4 &unit_world, const EquipmentRecord &record,
    const Render::Creature::SkeletonTopology *topology = nullptr,
    std::span<const QMatrix4x4> bone_palette = {}) noexcept -> QMatrix4x4;

} // namespace Render::Creature::Pipeline
