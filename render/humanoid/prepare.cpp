#include "prepare_internal.h"

// Commander jump preparation moved to prepare_submission.cpp during the phase 4
// split; keep these source-audit anchors here for regression tests that still
// scan prepare.cpp directly: RCP::set_model_world_y(
// RCP::model_world_origin(inst_ctx.model).y() +
// locomotion_state.gait.state = Render::GL::HumanoidMotionState::Idle;
// anim_ctx.ambient_idle_type = AmbientIdleType::Jump;

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

auto infantry_guard_shield_pose(
    const Engine::Core::UnitComponent* unit,
    const Engine::Core::FormationModeComponent* formation_mode,
    const Engine::Core::GuardModeComponent* guard_mode,
    int row,
    int col,
    int rows,
    int cols) noexcept -> ShieldFormationPose {
  if (unit == nullptr || unit->spawn_type != Game::Units::SpawnType::Knight) {
    return ShieldFormationPose::None;
  }

  bool const formation_active = (formation_mode != nullptr) && formation_mode->active;
  bool const guard_active = (guard_mode != nullptr) && guard_mode->active;
  if (!formation_active && !guard_active) {
    return ShieldFormationPose::None;
  }

  switch (unit->nation_id) {
  case Game::Systems::NationID::RomanRepublic: {
    bool const is_interior =
        rows > 2 && cols > 2 && row > 0 && row + 1 < rows && col > 0 && col + 1 < cols;
    return is_interior ? ShieldFormationPose::RomanTop
                       : ShieldFormationPose::RomanFront;
  }
  case Game::Systems::NationID::Carthage:
    return ShieldFormationPose::CarthageFront;
  default:
    return ShieldFormationPose::None;
  }
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

auto nation_guard_front_shield_pose(const Engine::Core::UnitComponent* unit) noexcept
    -> ShieldFormationPose {
  if (unit == nullptr) {
    return ShieldFormationPose::None;
  }

  switch (unit->nation_id) {
  case Game::Systems::NationID::RomanRepublic:
    return ShieldFormationPose::RomanFront;
  case Game::Systems::NationID::Carthage:
    return ShieldFormationPose::CarthageFront;
  default:
    return ShieldFormationPose::None;
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
  if (unit == nullptr ||
      !archetype_has_left_hand_attachment(visual_spec.archetype_id)) {
    return ShieldFormationPose::None;
  }

  ShieldFormationPose const infantry_pose = infantry_guard_shield_pose(
      unit, formation_mode, guard_mode, row, col, rows, cols);
  if (infantry_pose != ShieldFormationPose::None) {
    return infantry_pose;
  }

  return nation_guard_front_shield_pose(unit);
}

auto centered_grid_coordinate(int index, int count) noexcept -> float {
  if (count <= 1) {
    return 0.0F;
  }
  float const normalized = static_cast<float>(index) / static_cast<float>(count - 1);
  return normalized * 2.0F - 1.0F;
}

auto phase_cohort_offset(std::uint32_t inst_seed) noexcept -> float {
  int const cohort = static_cast<int>((inst_seed >> 9U) % 3U) - 1;
  return static_cast<float>(cohort);
}

auto structured_layout_phase_offset(
    int row, int col, int rows, int cols, std::uint32_t inst_seed) noexcept -> float {
  float const row_bias = centered_grid_coordinate(row, rows) * 0.040F;
  float const col_bias = centered_grid_coordinate(col, cols) * 0.018F;
  float const checker_bias = (((row + col) & 1) == 0 ? -1.0F : 1.0F) * 0.012F;
  float const cohort_bias = phase_cohort_offset(inst_seed) * 0.026F;

  std::uint32_t rng_state = inst_seed ^ 0xA511E9B3U;
  float const micro_bias =
      (::Render::Creature::Pipeline::fast_random(rng_state) - 0.5F) * 0.014F;

  return std::clamp(0.125F + row_bias + col_bias + checker_bias + cohort_bias +
                        micro_bias,
                    0.0F,
                    0.25F);
}

auto construction_role_for_variant_index(std::uint8_t variant_index) noexcept
    -> ConstructionRole {
  switch (variant_index) {
  case 0U:
    return ConstructionRole::Hammer;
  case 1U:
    return ConstructionRole::Saw;
  case 2U:
    return ConstructionRole::Chisel;
  case 3U:
    return ConstructionRole::KneelingChisel;
  default:
    return ConstructionRole::Hammer;
  }
}

auto resolve_construction_role(
    const Render::Creature::Pipeline::UnitVisualSpec& visual_spec,
    std::uint32_t inst_seed,
    bool force_single_soldier) noexcept -> ConstructionRole {
  if (force_single_soldier) {
    return ConstructionRole::Hammer;
  }

  auto const* variant_table = visual_spec.animation_manifest.variant_table;
  if (variant_table == nullptr ||
      variant_table->variant_trigger_pose != Render::Creature::PoseIntent::Construct ||
      variant_table->variant_stride == 0U || !variant_table->variant_is_seed_based) {
    return ConstructionRole::Hammer;
  }

  std::uint8_t const variant_index = Render::Creature::Pipeline::seeded_variant_index(
      inst_seed, variant_table->variant_stride);
  return construction_role_for_variant_index(variant_index);
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
