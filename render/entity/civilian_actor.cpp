#include "civilian_actor.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string>

#include "registry.h"
#include "render/creature/pipeline/creature_pipeline.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/pipeline/humanoid_animation_selection.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/world_view.h"

namespace Render::GL {
namespace {
auto rigs() -> std::array<NationCivilianRig, 2>& {
  static std::array<NationCivilianRig, 2> registered{};
  return registered;
}

auto priest_rigs() -> std::array<NationCivilianRig, 2>& {
  static std::array<NationCivilianRig, 2> registered{};
  return registered;
}

auto crew_rigs() -> std::array<NationCivilianRig, 2>& {
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

auto civilian_render_scale(const DrawContext& ctx,
                           const NationCivilianRig& rig) -> float {
  constexpr float k_fallback_civilian_scale = 0.48F;
  constexpr float k_fallback_builder_scale = 0.5F;
  const auto type = rig.builder  ? Game::Units::TroopType::Builder
                    : rig.healer ? Game::Units::TroopType::Healer
                                 : Game::Units::TroopType::Civilian;
  return ctx.world_view.troop_render_scale(
      rig.carthage ? Game::Systems::NationID::Carthage
                   : Game::Systems::NationID::RomanRepublic,
      type,
      rig.builder ? k_fallback_builder_scale : k_fallback_civilian_scale);
}

void register_nation_priest_rig(bool carthage, NationCivilianRig rig) {
  rig.carthage = carthage;
  rig.healer = true;
  priest_rigs()[carthage ? 1U : 0U] = rig;
}

auto nation_priest_rig(bool carthage) -> const NationCivilianRig& {
  return priest_rigs()[carthage ? 1U : 0U];
}

void register_nation_crew_rig(bool carthage, NationCivilianRig rig) {
  rig.carthage = carthage;
  rig.builder = true;
  crew_rigs()[carthage ? 1U : 0U] = rig;
}

auto nation_crew_rig(bool carthage) -> const NationCivilianRig& {
  return crew_rigs()[carthage ? 1U : 0U];
}

void register_nation_civilian_rig(bool carthage, NationCivilianRig rig) {
  rig.carthage = carthage;
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

  QMatrix4x4 world = actor.world;
  world.scale(civilian_render_scale(ctx, rig));
  output.world_matrix = world;
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
  if (actor.overlay_clip != 0xFFFFU && actor.overlay_weight > 0.0F) {
    Pipeline::HumanoidPlaybackLayerSelection overlay{};
    overlay.archetype = actor.archetype;
    overlay.state = AnimationStateId::Idle;
    overlay.phase = actor.overlay_phase;
    overlay.clip_id = actor.overlay_clip;
    overlay.weight = std::clamp(actor.overlay_weight, 0.0F, 1.0F);
    overlay.mode = Render::Creature::PlaybackLayerMode::UpperBodyOverlay;
    selection.upper_body_overlay = overlay;
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
