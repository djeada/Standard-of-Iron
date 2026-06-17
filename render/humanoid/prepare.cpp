#include "prepare_internal.h"

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
    tunic = Game::Visuals::team_colorForOwner(unit->owner_id);
  } else if (rc != nullptr) {
    tunic = QVector3D(rc->color[0], rc->color[1], rc->color[2]);
  }

  return tunic;
}

auto resolved_individuals_per_unit(const Engine::Core::UnitComponent& unit) -> int {
  if (unit.render_individuals_per_unit_override > 0) {
    return unit.render_individuals_per_unit_override;
  }
  return Game::Units::TroopConfig::instance().get_individuals_per_unit(unit.spawn_type);
}

auto archetype_has_left_hand_attachment(
    Render::Creature::ArchetypeId archetype_id) noexcept -> bool {
  auto const* desc = Render::Creature::ArchetypeRegistry::instance().get(archetype_id);
  if (desc == nullptr) {
    return false;
  }

  constexpr auto k_hand_l_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::HandL);
  for (auto const& attachment : desc->attachments_view()) {
    if (attachment.socket_bone_index == k_hand_l_bone) {
      return true;
    }
  }
  return false;
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
    const Engine::Core::FormationModeComponent* formation_mode,
    const Engine::Core::GuardModeComponent* guard_mode,
    int row,
    int col,
    int rows,
    int cols) noexcept -> ShieldFormationPose {
  if (unit == nullptr) {
    return ShieldFormationPose::None;
  }

  return Animation::resolve_humanoid_guard_shield_pose({
      .has_left_hand_shield =
          archetype_has_left_hand_attachment(visual_spec.archetype_id),
      .infantry_formation_unit = unit->spawn_type == Game::Units::SpawnType::Knight,
      .formation_active = formation_mode != nullptr && formation_mode->active,
      .guard_mode_active = guard_mode != nullptr && guard_mode->active,
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
    bool force_single_soldier) noexcept -> ConstructionRole {
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
  });
}

void apply_spec_pose_layer(
    const Render::Creature::Pipeline::UnitVisualSpec& visual_spec,
    const DrawContext& ctx,
    const HumanoidAnimationContext& anim_ctx,
    const HumanoidVariant& variant,
    std::uint32_t inst_seed,
    HumanoidPose& pose) {
  auto const pose_layer = visual_spec.animation_manifest.pose_layer;
  if (pose_layer == nullptr) {
    return;
  }

  Render::Creature::Pipeline::HumanoidPoseLayerContext layer_ctx;
  layer_ctx.draw_ctx = &ctx;
  layer_ctx.animation = &anim_ctx;
  layer_ctx.variant = &variant;
  layer_ctx.seed = inst_seed;
  pose_layer(layer_ctx, pose);
}

auto HumanoidRendererBase::resolve_formation(
    const HumanoidRendererBase& owner, const DrawContext& ctx) -> FormationParams {
  FormationParams params{};
  params.individuals_per_unit = 1;
  params.max_per_row = 1;
  params.spacing = 0.75F;

  if (ctx.entity != nullptr) {
    auto* unit = ctx.entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr) {
      params.individuals_per_unit = resolved_individuals_per_unit(*unit);
      params.max_per_row =
          Game::Units::TroopConfig::instance().get_max_units_per_row(unit->spawn_type);
      if (auto troop_type = Game::Units::spawn_typeToTroopType(unit->spawn_type)) {
        auto const profile = Game::Systems::TroopProfileService::instance().get_profile(
            unit->nation_id, *troop_type);
        params.spacing = resolve_formation_spacing(unit->spawn_type,
                                                   profile.visuals.formation_spacing,
                                                   owner.get_mount_scale());
      } else {
        params.spacing =
            resolve_formation_spacing(unit->spawn_type, 0.0F, owner.get_mount_scale());
      }
    }
  } else if (owner.uses_mounted_pipeline()) {
    params.spacing = resolve_formation_spacing(
        Game::Units::SpawnType::MountedKnight, 0.0F, owner.get_mount_scale());
  }

  return params;
}

void HumanoidRendererBase::render(const DrawContext& ctx, ISubmitter& out) const {
  AnimationInputs const anim =
      Render::Creature::Pipeline::resolve_humanoid_animation_state(ctx).inputs;

  if (ctx.template_prewarm) {
    render_procedural(
        Render::Creature::Pipeline::make_runtime_prewarm_ctx(ctx), anim, out);
    return;
  }

  render_procedural(ctx, anim, out);
}

void HumanoidRendererBase::render_procedural(const DrawContext& ctx,
                                             const AnimationInputs& anim,
                                             ISubmitter& out) const {
  Render::Humanoid::HumanoidPreparation prep;
  Render::Humanoid::prepare_humanoid_instances(
      *this, ctx, anim, humanoid_current_frame(), prep);

  Render::Creature::Pipeline::submit_preparation(prep, out);
}

} // namespace Render::GL
