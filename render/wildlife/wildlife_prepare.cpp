#include "wildlife_prepare.h"

#include <QMatrix4x4>

#include <cmath>

#include "game/core/component.h"
#include "game/core/entity.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/pipeline/creature_prepared_state.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/pipeline/creature_visual_definition.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/entity/registry.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "scene/camera.h"

namespace Render::Wildlife {

namespace {

namespace RCP = Render::Creature::Pipeline;

auto visual_spec_for(RCP::CreatureKind kind) -> RCP::UnitVisualSpec {
  RCP::UnitVisualSpec spec;
  spec.kind = kind;
  if (kind == RCP::CreatureKind::Wolf) {
    spec.debug_name = "wildlife/wolf";
    spec.creature_definition = &RCP::wolf_creature_visual_definition();
    spec.archetype_id = Render::Creature::ArchetypeRegistry::k_wolf_base;
  } else {
    spec.debug_name = "wildlife/sheep";
    spec.creature_definition = &RCP::sheep_creature_visual_definition();
    spec.archetype_id = Render::Creature::ArchetypeRegistry::k_sheep_base;
  }
  return spec;
}

} // namespace

void submit_wildlife(const Render::GL::DrawContext& ctx,
                     const WildlifeRenderInputs& inputs,
                     Render::GL::ISubmitter& out) {
  Render::GL::AnimationInputs anim{};
  anim.time = ctx.animation_time;

  RCP::CreatureGraphInputs graph_inputs{};
  graph_inputs.ctx = &ctx;
  graph_inputs.anim = &anim;
  graph_inputs.entity = ctx.entity;
  if (ctx.camera != nullptr) {
    graph_inputs.camera_distance = std::sqrt(ctx.distance_sq);
  } else {
    graph_inputs.has_camera = false;
  }

  RCP::CreatureLodDecision lod_decision =
      RCP::evaluate_creature_lod(graph_inputs, RCP::CreatureLodConfig{});
  auto graph_output = RCP::build_base_graph_output(graph_inputs, lod_decision);
  graph_output.spec = visual_spec_for(inputs.kind);

  RCP::PreparedWildlifeBodyState body_state;
  body_state.graph = graph_output;
  body_state.kind = inputs.kind;
  body_state.variant = inputs.variant;
  body_state.animation_state = inputs.state;
  body_state.phase = inputs.phase;

  RCP::CreaturePreparationResult prep;
  prep.bodies.add_quadruped(body_state);
  RCP::submit_preparation(prep, out);
}

} // namespace Render::Wildlife
