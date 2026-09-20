#include "civilian_actor.h"

#include <array>
#include <cstdlib>
#include <string>

#include "registry.h"
#include "render/creature/pipeline/creature_pipeline.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/pipeline/humanoid_animation_selection.h"
#include "render/gl/humanoid/humanoid_types.h"

namespace Render::GL {
namespace {
auto rigs() -> std::array<NationCivilianRig, 2>& {
  static std::array<NationCivilianRig, 2> registered{};
  return registered;
}

auto batch() -> Render::Creature::Pipeline::CreatureRenderBatch& {
  static thread_local Render::Creature::Pipeline::CreatureRenderBatch shared;
  return shared;
}
} // namespace

auto ambient_hash(std::uint32_t value) noexcept -> std::uint32_t {
  value ^= value >> 16U;
  value *= 0x7feb352dU;
  value ^= value >> 15U;
  value *= 0x846ca68bU;
  return value ^ (value >> 16U);
}

auto ambient_review_interval(const char* environment_variable) -> float {
  const char* value = std::getenv(environment_variable);
  if (value == nullptr) {
    return 0.0F;
  }
  const float seconds = static_cast<float>(std::atof(value));
  return seconds > 0.0F ? seconds : 0.0F;
}

void register_nation_civilian_rig(bool carthage, NationCivilianRig rig) {
  rigs()[carthage ? 1U : 0U] = rig;
}

auto nation_civilian_rig(bool carthage) -> const NationCivilianRig& {
  return rigs()[carthage ? 1U : 0U];
}

void begin_civilian_actors() {
  batch().clear();
}

void add_civilian_actor(const DrawContext& ctx,
                        const NationCivilianRig& rig,
                        const CivilianActor& actor) {
  using Render::Creature::AnimationStateId;
  using Render::Creature::CreatureLOD;
  namespace Pipeline = Render::Creature::Pipeline;

  if (!rig.valid() || actor.archetype == Render::Creature::k_invalid_archetype) {
    return;
  }
  Pipeline::CreatureGraphOutput output{};

  output.lod = actor.distant ? CreatureLOD::Minimal : CreatureLOD::Full;
  output.pass_intent = Pipeline::RenderPassIntent::Main;
  output.seed = actor.seed;
  output.world_matrix = actor.world;
  output.world_already_grounded = true;
  output.entity_id = static_cast<Pipeline::EntityId>(actor.owner_id);
  output.instance_index = actor.instance;
  output.spec = rig.spec;
  Pipeline::HumanoidAnimationSelection selection{};
  selection.requested_archetype = actor.archetype;
  selection.resolved_archetype = actor.archetype;
  selection.state = AnimationStateId::Idle;
  selection.phase = actor.phase;
  selection.clip_id = actor.clip;
  if (actor.blend_weight > 0.0F && actor.blend_clip != 0xFFFFU) {
    Pipeline::blend_out_interrupted_clip(
        selection, actor.blend_clip, actor.blend_phase, actor.blend_weight);
  }
  output.humanoid_selection = selection;
  const Render::GL::HumanoidPose pose{};
  const Render::GL::HumanoidAnimationContext anim{};
  Render::GL::HumanoidVariant variant{};
  rig.fill_variant(ctx, actor.seed, variant);
  batch().add_humanoid(output, pose, variant, anim);
}

void submit_civilian_actors(ISubmitter& out) {
  static thread_local Render::Creature::Pipeline::CreaturePipeline pipeline;
  if (batch().empty()) {
    return;
  }
  (void)pipeline.submit_requests(batch().requests(), out);
  batch().clear();
}

} // namespace Render::GL
