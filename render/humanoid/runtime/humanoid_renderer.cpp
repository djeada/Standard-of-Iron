#include "render/humanoid/runtime/humanoid_renderer.h"

#include <string>
#include <utility>

#include "animation/guard_manifest.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/systems/formation_combat_geometry.h"
#include "game/systems/nation_id.h"
#include "game/units/spawn_type.h"
#include "render/creature/animation_state_components.h"
#include "render/creature/pipeline/creature_prepared_state.h"
#include "render/creature/pipeline/preparation_common.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/entity/humanoid_pose_policies.h"
#include "render/entity/registry.h"
#include "render/entity_appearance.h"
#include "render/gl/humanoid/animation/animation_inputs.h"
#include "render/humanoid/runtime/frame_control.h"
#include "render/humanoid/runtime/humanoid_runtime_types.h"
#include "render/humanoid/runtime/instance_state.h"
#include "render/humanoid/runtime/runtime_context.h"
#include "render/humanoid/runtime/unit_layout_spacing.h"
#include "render/palette.h"

namespace Render::GL {

using namespace Render::GL::Geometry;

auto HumanoidRendererBase::resolve_team_tint(const DrawContext& ctx) -> QVector3D {
  QVector3D tunic(0.8F, 0.9F, 1.0F);
  Engine::Core::UnitComponent* unit = nullptr;
  Engine::Core::RenderableComponent* rc = nullptr;

  if (ctx.entity != nullptr) {
    unit = ctx.entity->get_component<Engine::Core::UnitComponent>();
    rc = ctx.entity->get_component<Engine::Core::RenderableComponent>();
  }

  if ((unit != nullptr) && unit->owner_id > 0) {
    tunic = Render::team_color(unit->owner_id);
  } else if (rc != nullptr) {
    tunic = Render::entity_color(*ctx.entity);
  }

  return tunic;
}

auto guard_shield_family(const Engine::Core::UnitComponent* unit) noexcept
    -> Animation::GuardShieldFamily {
  if (unit == nullptr) {
    return Animation::GuardShieldFamily::None;
  }

  switch (unit->nation_id) {
  case Game::Systems::NationID::RomanRepublic:
    return Animation::GuardShieldFamily::Roman;
  case Game::Systems::NationID::Carthage:
    return Animation::GuardShieldFamily::Carthage;
  default:
    return Animation::GuardShieldFamily::None;
  }
}

auto shared_guard_shield_pose(
    const Engine::Core::UnitComponent* unit,
    const Render::Creature::Pipeline::UnitVisualSpec& visual_spec,
    bool formation_active,
    bool guard_mode_active,
    bool defensive_layout_locked,
    int row,
    int col,
    int rows,
    int cols) noexcept -> ShieldFormationPose {
  if (unit == nullptr) {
    return ShieldFormationPose::None;
  }

  return Animation::resolve_humanoid_guard_shield_pose({
      .has_left_hand_shield = visual_spec.capabilities.contains(
          Render::Humanoid::HumanoidCapability::LeftHandShield),
      .infantry_formation_unit = unit->spawn_type == Game::Units::SpawnType::Knight,
      .formation_active = formation_active,
      .guard_mode_active = guard_mode_active,
      .defensive_layout_locked = defensive_layout_locked,
      .shield_family = guard_shield_family(unit),
      .row = row,
      .col = col,
      .rows = rows,
      .cols = cols,
  });
}

auto resolve_construction_role(
    const Render::Creature::Pipeline::UnitVisualSpec& visual_spec,
    std::uint32_t inst_seed,
    bool force_single_soldier,
    std::uint8_t construction_job) noexcept -> ConstructionRole {
  auto const* variant_table = visual_spec.animation_manifest.variant_table;
  return Animation::resolve_humanoid_construction_role({
      .seed = inst_seed,
      .force_single_soldier = force_single_soldier,
      .variant_table_can_select_roles =
          variant_table != nullptr && variant_table->variant_trigger_pose ==
                                          Render::Creature::PoseIntent::Construct,
      .variant_stride = variant_table != nullptr
                            ? static_cast<std::uint8_t>(variant_table->variant_stride)
                            : std::uint8_t{0U},
      .variant_is_seed_based =
          variant_table != nullptr && variant_table->variant_is_seed_based,
      .job = static_cast<Animation::HumanoidWorkJob>(construction_job),
  });
}

void apply_spec_pose_policy(
    const Render::Creature::Pipeline::UnitVisualSpec& visual_spec,
    const HumanoidAnimationContext& anim_ctx,
    const HumanoidVariant& variant,
    std::uint32_t inst_seed,
    HumanoidPose& pose) {
  Render::Entity::HumanoidPosePolicyInputs inputs;
  inputs.animation = &anim_ctx;
  inputs.variant = &variant;
  inputs.seed = inst_seed;
  Render::Entity::apply_humanoid_pose_policy(
      visual_spec.animation_manifest.pose_policy, inputs, pose);
}

auto HumanoidRendererBase::resolve_formation(
    const HumanoidRendererBase& owner, const DrawContext& ctx) -> FormationParams {
  FormationParams params{};
  params.individuals_per_unit = 1;
  params.max_per_row = 1;
  params.spacing = 0.75F;

  if (ctx.entity != nullptr) {
    auto const* presentation =
        ctx.entity->get_component<Engine::Core::FormationPresentationComponent>();
    if (presentation != nullptr && !presentation->soldiers.empty()) {
      params.individuals_per_unit = static_cast<int>(presentation->soldiers.size());
      params.max_per_row = static_cast<int>(presentation->cols);
      params.spacing = presentation->spacing;
    } else if (auto* unit = ctx.entity->get_component<Engine::Core::UnitComponent>();
               unit != nullptr) {

      auto const definition = Game::Systems::FormationCombat::resolve_definition(*unit);
      params.individuals_per_unit = definition.total_count;
      params.max_per_row = definition.max_per_row;
      params.spacing = definition.spacing;
    }
  } else if (owner.uses_mounted_pipeline()) {
    params.spacing = resolve_formation_spacing(*ctx.world_view.troop_config(),
                                               Game::Units::SpawnType::MountedKnight,
                                               0.0F,
                                               owner.get_mount_scale());
  }

  return params;
}

void HumanoidRendererBase::ensure_prepare_components(
    Engine::Core::Entity& entity) const {
  Engine::Core::get_or_add_component<Render::Creature::HumanoidAnimationStateComponent>(
      entity);
  Engine::Core::get_or_add_component<Render::Humanoid::HumanoidInstanceStateComponent>(
      entity);
}

void HumanoidRendererBase::prepare(
    const DrawContext& ctx,
    Render::Creature::Pipeline::CreaturePreparationResult& out) const {
  AnimationInputs const anim =
      Render::Creature::Pipeline::resolve_humanoid_animation_state(ctx).inputs;

  Render::Humanoid::HumanoidRuntimeContext& runtime =
      ctx.humanoid_runtime != nullptr
          ? *ctx.humanoid_runtime
          : Render::Humanoid::current_humanoid_runtime_context();

  if (ctx.template_prewarm) [[unlikely]] {
    const DrawContext prewarm_ctx =
        Render::Creature::Pipeline::make_runtime_prewarm_ctx(ctx);
    Render::Humanoid::prepare_humanoid_instances(
        *this, prewarm_ctx, anim, runtime, out);
    return;
  }

  Render::Humanoid::prepare_humanoid_instances(*this, ctx, anim, runtime, out);
}

void HumanoidRendererBase::render(const DrawContext& ctx, ISubmitter& out) const {
  thread_local Render::Humanoid::HumanoidPreparation prep;
  prep.clear();
  prepare(ctx, prep);
  Render::Creature::Pipeline::submit_preparation(prep, out);
}

void register_humanoid_renderer(EntityRendererRegistry& registry,
                                std::string key,
                                std::shared_ptr<const HumanoidRendererBase> renderer) {
  registry.register_renderer(
      std::move(key),
      [renderer](const DrawContext& ctx, ISubmitter& out) {
        renderer->render(ctx, out);
      },
      renderer);
}

} // namespace Render::GL
