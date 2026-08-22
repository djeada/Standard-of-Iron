#include "quadruped_prepare.h"

#include <QtMath>

#include <cmath>

#include "render/creature/pipeline/creature_prepared_state.h"
#include "render/creature/pipeline/preparation_common.h"
#include "render/entity/registry.h"
#include "scene/camera.h"

namespace Render::Creature::Quadruped {

namespace RCP = Render::Creature::Pipeline;

auto build_quadruped_body(const QuadrupedRuntimeInput& input) -> QuadrupedPreparedBody {
  QuadrupedPreparedBody body;

  RCP::CreatureGraphInputs graph_inputs{};
  graph_inputs.ctx = input.ctx;
  graph_inputs.anim = input.anim;
  graph_inputs.entity = input.ctx != nullptr ? input.ctx->entity : nullptr;

  RCP::CreatureLodDecision lod_decision{};
  lod_decision.lod = input.lod;

  body.graph = RCP::build_base_graph_output(graph_inputs, lod_decision);
  if (input.spec != nullptr) {
    body.graph.spec = *input.spec;
  }
  body.graph.seed = input.seed;

  body.world_position = RCP::model_world_origin(input.world);
  if (input.ctx != nullptr && input.ctx->camera != nullptr) {
    body.camera_distance =
        (body.world_position - input.ctx->camera->get_position()).length();
  }
  return body;
}

void add_quadruped_shadow(const QuadrupedRuntimeInput& input,
                          const QuadrupedPreparedBody& body,
                          RCP::CreaturePreparationResult& out) {
  RCP::QuadrupedShadowStateInputs shadow_inputs{};
  shadow_inputs.ctx = input.ctx;
  shadow_inputs.graph = &body.graph;
  shadow_inputs.world_pos = body.world_position;
  shadow_inputs.kind = input.kind;
  shadow_inputs.lod = input.lod;
  shadow_inputs.camera_distance = body.camera_distance;
  {
    const QVector3D forward = input.world.mapVector(QVector3D(0.0F, 0.0F, 1.0F));
    shadow_inputs.facing_yaw_degrees =
        qRadiansToDegrees(std::atan2(double(forward.x()), double(forward.z())));
  }
  shadow_inputs.intensity_scale = input.shadow_intensity_scale;
  shadow_inputs.surface_world_y = input.surface_world_y;
  shadow_inputs.surface_height_valid = input.surface_height_valid;

  const auto shadow_state = RCP::prepare_quadruped_shadow_state(shadow_inputs);
  if (!shadow_state.enabled) {
    return;
  }
  if (out.shadow_batch.empty()) {
    out.shadow_batch.init(
        shadow_state.shader, shadow_state.mesh, shadow_state.light_dir);
  }
  out.shadow_batch.add(shadow_state.model, shadow_state.alpha, shadow_state.pass);
}

} // namespace Render::Creature::Quadruped
