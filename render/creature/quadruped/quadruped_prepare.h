#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>

#include "render/creature/part_graph.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/render_request.h"

namespace Render::GL {
struct DrawContext;
struct AnimationInputs;
} // namespace Render::GL

namespace Render::Creature::Pipeline {
struct UnitVisualSpec;
}

namespace Render::Creature::Quadruped {

struct QuadrupedRuntimeInput {
  const Render::GL::DrawContext* ctx{nullptr};
  const Render::GL::AnimationInputs* anim{nullptr};
  const Render::Creature::Pipeline::UnitVisualSpec* spec{nullptr};

  Render::Creature::Pipeline::CreatureKind kind{
      Render::Creature::Pipeline::CreatureKind::Horse};
  Render::Creature::CreatureLOD lod{Render::Creature::CreatureLOD::Full};

  Render::Creature::AnimationStateId animation{
      Render::Creature::AnimationStateId::Idle};
  float phase{0.0F};

  QMatrix4x4 world{};
  std::uint32_t seed{0U};

  float surface_world_y{0.0F};
  bool surface_height_valid{true};

  float shadow_intensity_scale{1.0F};
};

struct QuadrupedPreparedBody {
  Render::Creature::Pipeline::CreatureGraphOutput graph{};
  float camera_distance{0.0F};
  QVector3D world_position{};
};

[[nodiscard]] auto
build_quadruped_body(const QuadrupedRuntimeInput& input) -> QuadrupedPreparedBody;

void add_quadruped_shadow(const QuadrupedRuntimeInput& input,
                          const QuadrupedPreparedBody& body,
                          Render::Creature::Pipeline::CreaturePreparationResult& out);

} // namespace Render::Creature::Quadruped
