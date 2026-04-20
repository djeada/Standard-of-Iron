#include "prepare.h"

#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/core/world.h"
#include "../../game/map/terrain_service.h"
#include "../../game/systems/nation_id.h"
#include "../../game/units/spawn_type.h"
#include "../../game/units/troop_config.h"
#include "../../game/visuals/team_colors.h"
#include "../creature/pipeline/creature_render_graph.h"
#include "../creature/pipeline/lod_decision.h"
#include "../creature/pipeline/prepared_submit.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../graphics_settings.h"
#include "../creature/spec.h"
#include "../entity/registry.h"
#include "../geom/math_utils.h"
#include "../geom/parts.h"
#include "../geom/transforms.h"
#include "../gl/backend.h"
#include "../gl/camera.h"
#include "../gl/humanoid/animation/animation_inputs.h"
#include "../gl/humanoid/animation/gait.h"
#include "../gl/humanoid/humanoid_constants.h"
#include "../gl/primitives.h"
#include "../gl/render_constants.h"
#include "../gl/resources.h"
#include "../palette.h"
#include "../pose_palette_cache.h"
#include "../scene_renderer.h"
#include "../submitter.h"
#include "../visibility_budget.h"
#include "cache_control.h"
#include "clip_driver_cache.h"
#include "formation_calculator.h"
#include "humanoid_math.h"
#include "humanoid_renderer_base.h"
#include "humanoid_spec.h"
#include "pose_controller.h"
#include "render_stats.h"
#include "rig_stats_shim.h"

#include <QMatrix4x4>
#include <QVector2D>
#include <QVector4D>
#include <QtMath>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numbers>
#include <unordered_map>
#include <vector>

namespace Render::Humanoid {

auto make_humanoid_prepared_row(
    const Render::GL::HumanoidRendererBase &owner,
    const Render::GL::HumanoidPose &pose,
    const Render::GL::HumanoidVariant &variant,
    const Render::GL::HumanoidAnimationContext &anim_ctx,
    const QMatrix4x4 &inst_model, std::uint32_t inst_seed,
    Render::Creature::CreatureLOD lod,
    const Render::GL::DrawContext *legacy_ctx,
    Render::Creature::Pipeline::RenderPassIntent pass) noexcept
    -> Render::Creature::Pipeline::PreparedCreatureRenderRow {
  return Render::Creature::Pipeline::make_prepared_humanoid_row(
      owner.visual_spec(), pose, variant, anim_ctx, inst_model, inst_seed, lod,
      legacy_ctx, /*entity_id*/ 0, pass);
}

} // namespace Render::Humanoid

namespace Render::GL {

using namespace Render::GL::Geometry;

namespace {

constexpr float k_shadow_size_infantry = 0.16F;
constexpr float k_shadow_size_mounted = 0.35F;

constexpr float k_run_extra_foot_lift = 0.08F;
constexpr float k_run_stride_enhancement = 0.15F;

struct CachedPoseEntry {
  HumanoidPose pose;
  VariationParams variation;
  uint32_t frame_number{0};
  bool was_moving{false};
};

using PoseCacheKey = uint64_t;
std::unordered_map<PoseCacheKey, CachedPoseEntry> s_pose_cache;
uint32_t s_current_frame = 0;
constexpr uint32_t k_pose_cache_max_age = 300;

struct SoldierLayout {
  float offset_x{0.0F};
  float offset_z{0.0F};
  float vertical_jitter{0.0F};
  float yaw_offset{0.0F};
  float phase_offset{0.0F};
  std::uint32_t inst_seed{0};
};

struct UnitLayoutCacheEntry {
  std::vector<SoldierLayout> soldiers;
  FormationParams formation{};
  FormationCalculatorFactory::Nation nation{
      FormationCalculatorFactory::Nation::Roman};
  FormationCalculatorFactory::UnitCategory category{
      FormationCalculatorFactory::UnitCategory::Infantry};
  int rows{0};
  int cols{0};
  std::uint32_t seed{0};
  std::uint32_t frame_number{0};
};

std::unordered_map<std::uintptr_t, UnitLayoutCacheEntry> s_layout_cache;
constexpr uint32_t k_layout_cache_max_age = 600;

inline auto make_pose_cache_key(uintptr_t entity_ptr,
                                int soldier_idx) -> PoseCacheKey {
  return (static_cast<uint64_t>(entity_ptr) << 16) |
         static_cast<uint64_t>(soldier_idx & 0xFFFF);
}

HumanoidRenderStats s_render_stats;

constexpr float k_shadow_ground_offset = 0.02F;
constexpr float k_shadow_base_alpha = 0.24F;
const QVector3D k_shadow_light_dir(0.4F, 1.0F, 0.25F);

constexpr float k_temporal_skip_distance_reduced = 35.0F;
constexpr float k_temporal_skip_distance_minimal = 45.0F;
constexpr uint32_t k_temporal_skip_period_reduced = 2;
constexpr uint32_t k_temporal_skip_period_minimal = 3;

} // namespace

auto humanoid_current_frame() -> std::uint32_t { return s_current_frame; }

void advance_pose_cache_frame() {
  ++s_current_frame;
  ::Render::Humanoid::ClipDriverCache::instance().advance_frame(
      s_current_frame);

  if ((s_current_frame & 0x1FF) == 0) {
    auto it = s_pose_cache.begin();
    while (it != s_pose_cache.end()) {
      if (s_current_frame - it->second.frame_number >
          k_pose_cache_max_age * 2) {
        it = s_pose_cache.erase(it);
      } else {
        ++it;
      }
    }

    auto layout_it = s_layout_cache.begin();
    while (layout_it != s_layout_cache.end()) {
      if (s_current_frame - layout_it->second.frame_number >
          k_layout_cache_max_age * 2) {
        layout_it = s_layout_cache.erase(layout_it);
      } else {
        ++layout_it;
      }
    }
  }
}

void clear_humanoid_caches() {
  s_pose_cache.clear();
  s_layout_cache.clear();
  s_current_frame = 0;
  ::Render::Humanoid::ClipDriverCache::instance().clear();
}

auto get_humanoid_render_stats() -> const HumanoidRenderStats & {
  return s_render_stats;
}

void reset_humanoid_render_stats() { s_render_stats.reset(); }

namespace detail {
void increment_facial_hair_skipped_distance() {
  ++s_render_stats.facial_hair_skipped_distance;
}
} // namespace detail

} // namespace Render::GL

namespace Render::Humanoid {

using Render::GL::AmbientIdleType;
using Render::GL::AnimationInputs;
using Render::GL::DrawContext;
using Render::GL::FormationCalculatorFactory;
using Render::GL::FormationOffset;
using Render::GL::FormationParams;
using Render::GL::HumanoidAnimationContext;
using Render::GL::HumanoidLOD;
using Render::GL::HumanoidMotionState;
using Render::GL::HumanoidPose;
using Render::GL::HumanoidPoseController;
using Render::GL::HumanoidRendererBase;
using Render::GL::HumanoidVariant;
using Render::GL::IFormationCalculator;
using Render::GL::ISubmitter;
using Render::GL::PosePaletteCache;
using Render::GL::PosePaletteKey;
using Render::GL::Renderer;
using Render::GL::Shader;
using Render::GL::VariationParams;
using Render::GL::AnimState;
using Render::GL::HumanProportions;
using Render::GL::elbow_bend_torso;
using Render::GL::classify_motion_state;
using Render::GL::k_reference_run_speed;
using Render::GL::k_reference_walk_speed;

void prepare_humanoid_instances(const HumanoidRendererBase &owner,
                                const DrawContext &ctx,
                                const AnimationInputs &anim,
                                std::uint32_t frame_index,
                                HumanoidPreparation &out) {
  using namespace Render::GL;

  FormationParams const formation = HumanoidRendererBase::resolve_formation(ctx);

  Engine::Core::UnitComponent *unit_comp = nullptr;
  if (ctx.entity != nullptr) {
    unit_comp = ctx.entity->get_component<Engine::Core::UnitComponent>();
  }

  Engine::Core::MovementComponent *movement_comp = nullptr;
  Engine::Core::TransformComponent *transform_comp = nullptr;
  if (ctx.entity != nullptr) {
    movement_comp =
        ctx.entity->get_component<Engine::Core::MovementComponent>();
    transform_comp =
        ctx.entity->get_component<Engine::Core::TransformComponent>();
  }

  float entity_ground_offset =
      owner.resolve_entity_ground_offset(ctx, unit_comp, transform_comp);

  uint32_t seed = 0U;
  if (ctx.has_seed_override) {
    seed = ctx.seed_override;
  } else {
    if (unit_comp != nullptr) {
      seed ^= uint32_t(unit_comp->owner_id * 2654435761U);
    }
    if (ctx.entity != nullptr) {
      seed ^= uint32_t(reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
    }
  }

  const int rows =
      (formation.individuals_per_unit + formation.max_per_row - 1) /
      formation.max_per_row;
  int cols = formation.max_per_row;
  int effective_rows = rows;
  if (ctx.force_single_soldier) {
    cols = 1;
    effective_rows = 1;
  }

  bool is_mounted_spawn = false;
  if (unit_comp != nullptr) {
    using Game::Units::SpawnType;
    auto const st = unit_comp->spawn_type;
    is_mounted_spawn =
        (st == SpawnType::MountedKnight || st == SpawnType::HorseArcher ||
         st == SpawnType::HorseSpearman);
  }

  int visible_count = effective_rows * cols;
  if (!ctx.force_single_soldier && unit_comp != nullptr) {
    int const mh = std::max(1, unit_comp->max_health);
    float const ratio = std::clamp(unit_comp->health / float(mh), 0.0F, 1.0F);
    visible_count = std::max(1, (int)std::ceil(ratio * float(rows * cols)));
  }

  HumanoidVariant variant;
  owner.get_variant(ctx, seed, variant);

  if (!owner.m_proportion_scale_cached) {
    owner.m_cached_proportion_scale = owner.get_proportion_scaling();
    owner.m_proportion_scale_cached = true;
  }
  const QVector3D prop_scale = owner.m_cached_proportion_scale;
  const float height_scale = prop_scale.y();
  const bool needs_height_scaling = std::abs(height_scale - 1.0F) > 0.001F;

  const QMatrix4x4 k_identity_matrix;

  const IFormationCalculator *formation_calculator = nullptr;
  {
    using Nation = FormationCalculatorFactory::Nation;
    using UnitCategory = FormationCalculatorFactory::UnitCategory;

    Nation nation = Nation::Roman;
    if (unit_comp != nullptr) {
      if (unit_comp->nation_id == Game::Systems::NationID::Carthage) {
        nation = Nation::Carthage;
      }
    }

    UnitCategory category =
        is_mounted_spawn ? UnitCategory::Cavalry : UnitCategory::Infantry;

    if (unit_comp != nullptr &&
        unit_comp->spawn_type == Game::Units::SpawnType::Builder &&
        anim.is_constructing) {
      category = UnitCategory::BuilderConstruction;
    }

    formation_calculator =
        FormationCalculatorFactory::get_calculator(nation, category);
  }

  auto fast_random = [](uint32_t &state) -> float {
    state = state * 1664525U + 1013904223U;
    return float(state & 0x7FFFFFU) / float(0x7FFFFFU);
  };

  s_render_stats.soldiers_total += visible_count;

  out.rows.reserve(out.rows.size() + static_cast<std::size_t>(visible_count));
  out.per_instance_ctx.reserve(out.per_instance_ctx.size() +
                               static_cast<std::size_t>(visible_count));
  out.post_body_draws.reserve(out.post_body_draws.size() +
                              static_cast<std::size_t>(visible_count));

  // Pre-compute LOD config once for all soldiers in this unit
  namespace RCP = Render::Creature::Pipeline;
  const auto lod_config = RCP::humanoid_lod_config_from_settings();

  auto enqueue_prepared_body =
      [&](const HumanoidPose &pose, const HumanoidVariant &variant,
          const HumanoidAnimationContext &anim_ctx, const DrawContext &inst_ctx,
          std::uint32_t inst_seed, Render::Creature::CreatureLOD lod) {
        out.rows.push_back(make_humanoid_prepared_row(
            owner, pose, variant, anim_ctx, inst_ctx.model, inst_seed, lod,
            nullptr));
        out.per_instance_ctx.push_back(inst_ctx);
      };

  for (int idx = 0; idx < visible_count; ++idx) {
    int const r = idx / cols;
    int const c = idx % cols;

    float offset_x = 0.0F;
    float offset_z = 0.0F;

    uint32_t inst_seed = seed ^ uint32_t(idx * 9176U);

    uint32_t rng_state = inst_seed;

    float vertical_jitter = 0.0F;
    float yaw_offset = 0.0F;
    float phase_offset = 0.0F;

    if (!ctx.force_single_soldier) {
      FormationOffset const formation_offset =
          formation_calculator->calculate_offset(idx, r, c, rows, cols,
                                                 formation.spacing, seed);

      offset_x = formation_offset.offset_x;
      offset_z = formation_offset.offset_z;

      vertical_jitter = (fast_random(rng_state) - 0.5F) * 0.03F;
      yaw_offset = (fast_random(rng_state) - 0.5F) * 5.0F;
      phase_offset = fast_random(rng_state) * 0.25F;
    }

    if (anim.is_attacking && anim.is_melee) {
      float const combat_jitter_x =
          (fast_random(rng_state) - 0.5F) * formation.spacing * 0.4F;
      float const combat_jitter_z =
          (fast_random(rng_state) - 0.5F) * formation.spacing * 0.3F;
      float const sway_time = anim.time + phase_offset * 2.0F;
      float const sway_x = std::sin(sway_time * 1.5F) * 0.05F;
      float const sway_z = std::cos(sway_time * 1.2F) * 0.04F;
      offset_x += combat_jitter_x + sway_x;
      offset_z += combat_jitter_z + sway_z;
    }

    float applied_vertical_jitter = vertical_jitter;
    float applied_yaw_offset = yaw_offset;

    if (anim.is_attacking && anim.is_melee) {
      float const combat_yaw = (fast_random(rng_state) - 0.5F) * 15.0F;
      applied_yaw_offset += combat_yaw;
    }

    QMatrix4x4 inst_model;
    float applied_yaw = applied_yaw_offset;

    if (transform_comp != nullptr) {
      applied_yaw = transform_comp->rotation.y + applied_yaw_offset;
      QMatrix4x4 m = k_identity_matrix;
      m.translate(transform_comp->position.x, transform_comp->position.y,
                  transform_comp->position.z);
      m.rotate(applied_yaw, 0.0F, 1.0F, 0.0F);
      m.scale(transform_comp->scale.x, transform_comp->scale.y,
              transform_comp->scale.z);
      m.translate(offset_x, applied_vertical_jitter, offset_z);
      if (!ctx.skip_ground_offset && entity_ground_offset != 0.0F) {
        m.translate(0.0F, -entity_ground_offset, 0.0F);
      }
      inst_model = m;
    } else {
      inst_model = ctx.model;
      inst_model.rotate(applied_yaw, 0.0F, 1.0F, 0.0F);
      inst_model.translate(offset_x, applied_vertical_jitter, offset_z);
      if (!ctx.skip_ground_offset && entity_ground_offset != 0.0F) {
        inst_model.translate(0.0F, -entity_ground_offset, 0.0F);
      }
    }

    QVector3D const soldier_world_pos =
        inst_model.map(QVector3D(0.0F, 0.0F, 0.0F));

    constexpr float k_soldier_cull_radius = 0.6F;
    if (ctx.camera != nullptr &&
        !ctx.camera->is_in_frustum(soldier_world_pos, k_soldier_cull_radius)) {
      ++s_render_stats.soldiers_skipped_frustum;
      continue;
    }

    // Use pre-computed LOD config with per-instance data
    RCP::CreatureLodDecisionInputs lod_in{};
    if (ctx.force_humanoid_lod) {
      lod_in.forced_lod =
          static_cast<Render::Creature::CreatureLOD>(ctx.forced_humanoid_lod);
    }
    lod_in.has_camera = (ctx.camera != nullptr);
    float soldier_distance = 0.0F;
    if (ctx.camera != nullptr) {
      soldier_distance =
          (soldier_world_pos - ctx.camera->get_position()).length();
    }
    lod_in.distance = soldier_distance;
    lod_in.thresholds = lod_config.thresholds;
    lod_in.apply_visibility_budget = lod_config.apply_visibility_budget;
    lod_in.budget_grant_full = true;
    lod_in.temporal = lod_config.temporal;
    lod_in.frame_index = frame_index;
    lod_in.instance_seed = inst_seed;

    if (lod_in.apply_visibility_budget && !ctx.force_humanoid_lod &&
        ctx.camera != nullptr) {
      const auto distance_lod =
          RCP::select_distance_lod(soldier_distance, lod_in.thresholds);
      if (distance_lod == Render::Creature::CreatureLOD::Full) {
        const auto granted =
            Render::VisibilityBudgetTracker::instance().request_humanoid_lod(
                HumanoidLOD::Full);
        lod_in.budget_grant_full = (granted == HumanoidLOD::Full);
      }
    }

    const auto lod_decision = RCP::decide_creature_lod(lod_in);
    if (lod_decision.culled) {
      if (lod_decision.reason == RCP::CullReason::Billboard) {
        ++s_render_stats.soldiers_skipped_lod;
      } else if (lod_decision.reason == RCP::CullReason::Temporal) {
        ++s_render_stats.soldiers_skipped_temporal;
      }
      continue;
    }
    HumanoidLOD soldier_lod = static_cast<HumanoidLOD>(lod_decision.lod);

    ++s_render_stats.soldiers_rendered;

    DrawContext inst_ctx{ctx.resources, ctx.entity, ctx.world, inst_model};
    inst_ctx.selected = ctx.selected;
    inst_ctx.hovered = ctx.hovered;
    inst_ctx.animation_time = ctx.animation_time;
    inst_ctx.renderer_id = ctx.renderer_id;
    inst_ctx.backend = ctx.backend;
    inst_ctx.camera = ctx.camera;

    VariationParams variation = VariationParams::fromSeed(inst_seed);
    owner.adjust_variation(inst_ctx, inst_seed, variation);
    if (anim.is_running) {
      variation.walk_speed_mult *= 1.25F;
      variation.arm_swing_amp *= 1.12F;
      variation.stance_width *= 0.96F;
      variation.posture_slump = std::min(0.16F, variation.posture_slump + 0.020F);
    } else if (anim.is_moving) {
      variation.walk_speed_mult *= 1.05F;
    }

    float const combined_height_scale = height_scale * variation.height_scale;
    if (needs_height_scaling ||
        std::abs(variation.height_scale - 1.0F) > 0.001F) {
      QMatrix4x4 scale_matrix;
      scale_matrix.scale(variation.bulk_scale, combined_height_scale, 1.0F);
      inst_ctx.model = inst_ctx.model * scale_matrix;
    }

    HumanoidPose pose;
    bool used_cached_pose = false;

    PoseCacheKey cache_key =
        make_pose_cache_key(reinterpret_cast<uintptr_t>(ctx.entity), idx);

    auto cache_it = s_pose_cache.find(cache_key);
    const bool allow_cached_pose = (!anim.is_moving) || ctx.animation_throttled;
    if (allow_cached_pose && cache_it != s_pose_cache.end()) {
      CachedPoseEntry &cached = cache_it->second;
      if ((ctx.animation_throttled || !cached.was_moving) &&
          frame_index - cached.frame_number < k_pose_cache_max_age) {
        pose = cached.pose;
        variation = cached.variation;
        cached.frame_number = frame_index;
        used_cached_pose = true;
        ++s_render_stats.poses_cached;
      }
    }

    if (!used_cached_pose) {

      bool used_palette = false;
      if (!anim.is_moving && !anim.is_attacking &&
          PosePaletteCache::instance().is_generated() &&
          (soldier_lod == HumanoidLOD::Reduced ||
           soldier_lod == HumanoidLOD::Minimal)) {
        PosePaletteKey palette_key;
        palette_key.state = AnimState::Idle;
        palette_key.frame = 0;
        palette_key.is_moving = false;
        const auto *palette_entry =
            PosePaletteCache::instance().get(palette_key);
        if (palette_entry != nullptr) {
          pose = palette_entry->pose;
          used_palette = true;
          ++s_render_stats.poses_cached;
        }
      }

      if (!used_palette) {
        HumanoidRendererBase::compute_locomotion_pose(
            inst_seed, anim.time + phase_offset, anim.is_moving, variation,
            pose);
        ++s_render_stats.poses_computed;
      }

      CachedPoseEntry &entry = s_pose_cache[cache_key];
      entry.pose = pose;
      entry.variation = variation;
      entry.frame_number = frame_index;
      entry.was_moving = anim.is_moving;
    }

    HumanoidAnimationContext anim_ctx{};
    anim_ctx.inputs = anim;
    anim_ctx.variation = variation;
    anim_ctx.formation = formation;
    anim_ctx.jitter_seed = phase_offset;
    anim_ctx.instance_position =
        inst_ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));

    float yaw_rad = qDegreesToRadians(applied_yaw);
    QVector3D forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
    if (forward.lengthSquared() > 1e-8F) {
      forward.normalize();
    } else {
      forward = QVector3D(0.0F, 0.0F, 1.0F);
    }
    QVector3D up(0.0F, 1.0F, 0.0F);
    QVector3D right = QVector3D::crossProduct(up, forward);
    if (right.lengthSquared() > 1e-8F) {
      right.normalize();
    } else {
      right = QVector3D(1.0F, 0.0F, 0.0F);
    }

    anim_ctx.entity_forward = forward;
    anim_ctx.entity_right = right;
    anim_ctx.entity_up = up;
    anim_ctx.locomotion_direction = forward;
    anim_ctx.yaw_degrees = applied_yaw;
    anim_ctx.yaw_radians = yaw_rad;
    anim_ctx.has_movement_target = false;
    anim_ctx.move_speed = 0.0F;
    anim_ctx.movement_target = QVector3D(0.0F, 0.0F, 0.0F);

    if (movement_comp != nullptr) {
      QVector3D velocity(movement_comp->vx, 0.0F, movement_comp->vz);
      float speed = velocity.length();
      anim_ctx.move_speed = speed;
      if (speed > 1e-4F) {
        anim_ctx.locomotion_direction = velocity.normalized();
      }
      anim_ctx.has_movement_target = movement_comp->has_target;
      anim_ctx.movement_target =
          QVector3D(movement_comp->target_x, 0.0F, movement_comp->target_y);
    }

    anim_ctx.locomotion_velocity =
        anim_ctx.locomotion_direction * anim_ctx.move_speed;
    anim_ctx.motion_state = classify_motion_state(anim, anim_ctx.move_speed);
    anim_ctx.gait.state = anim_ctx.motion_state;
    anim_ctx.gait.speed = anim_ctx.move_speed;
    anim_ctx.gait.velocity = anim_ctx.locomotion_velocity;
    anim_ctx.gait.has_target = anim_ctx.has_movement_target;
    anim_ctx.gait.is_airborne = false;

    float reference_speed = (anim_ctx.gait.state == HumanoidMotionState::Run)
                                ? k_reference_run_speed
                                : k_reference_walk_speed;
    if (reference_speed > 0.0001F) {
      anim_ctx.gait.normalized_speed =
          std::clamp(anim_ctx.gait.speed / reference_speed, 0.0F, 1.0F);
    } else {
      anim_ctx.gait.normalized_speed = 0.0F;
    }
    if (!anim.is_moving) {
      anim_ctx.gait.normalized_speed = 0.0F;
    }

    if (anim.is_moving) {
      float stride_base = 0.8F;
      if (anim_ctx.motion_state == HumanoidMotionState::Run) {
        stride_base = 0.45F;
      }
      float const base_cycle =
          stride_base / std::max(0.1F, variation.walk_speed_mult);
      anim_ctx.locomotion_cycle_time = base_cycle;
      anim_ctx.locomotion_phase = std::fmod(
          (anim.time + phase_offset) / std::max(0.001F, base_cycle), 1.0F);
      anim_ctx.gait.cycle_time = anim_ctx.locomotion_cycle_time;
      anim_ctx.gait.cycle_phase = anim_ctx.locomotion_phase;
      anim_ctx.gait.stride_distance =
          anim_ctx.gait.speed * anim_ctx.gait.cycle_time;
    } else {
      anim_ctx.locomotion_cycle_time = 0.0F;
      anim_ctx.locomotion_phase = 0.0F;
      anim_ctx.gait.cycle_time = 0.0F;
      anim_ctx.gait.cycle_phase = 0.0F;
      anim_ctx.gait.stride_distance = 0.0F;
    }
    if (anim.is_attacking) {
      float const attack_offset = phase_offset * 1.5F;
      anim_ctx.attack_phase = std::fmod(anim.time + attack_offset, 1.0F);
      if (ctx.has_attack_variant_override) {
        anim_ctx.inputs.attack_variant = ctx.attack_variant_override;
      } else {
        anim_ctx.inputs.attack_variant =
            static_cast<std::uint8_t>(inst_seed % 3);
      }
    }

    owner.customize_pose(inst_ctx, anim_ctx, inst_seed, pose);

    if (ctx.entity != nullptr) {
      auto const entity_id = static_cast<std::uint32_t>(ctx.entity->get_id());
      auto &cache_entry =
          ::Render::Humanoid::ClipDriverCache::instance().get_or_create(
              entity_id);
      float const now = anim.time + phase_offset;
      float dt = 0.0F;
      if (cache_entry.initialised) {
        dt = now - cache_entry.last_time;
        if (dt < 0.0F || dt > 1.0F) {
          dt = 0.0F;
        }
      } else {
        cache_entry.initialised = true;
      }
      cache_entry.last_time = now;
      cache_entry.driver.tick(dt, anim);
      auto const overlays = cache_entry.driver.sample(now);
      ::Render::Humanoid::apply_overlays_to_pose(pose, overlays,
                                                 anim_ctx.entity_right);
    }

    if (!anim.is_moving && !anim.is_attacking) {
      HumanoidPoseController pose_ctrl(pose, anim_ctx);

      pose_ctrl.apply_micro_idle(anim.time + phase_offset, inst_seed);

      if (visible_count > 0) {
        auto hash_u32 = [](std::uint32_t x) -> std::uint32_t {
          x ^= x >> 16;
          x *= 0x7feb352dU;
          x ^= x >> 15;
          x *= 0x846ca68bU;
          x ^= x >> 16;
          return x;
        };

        constexpr float k_ambient_anim_duration = 6.0F;
        constexpr float k_unit_cycle_base = 15.0F;
        constexpr float k_unit_cycle_range = 10.0F;

        float const unit_cycle_period =
            k_unit_cycle_base +
            static_cast<float>(seed % 1000U) / (1000.0F / k_unit_cycle_range);
        float const unit_time_in_cycle =
            std::fmod(anim.time, std::max(0.001F, unit_cycle_period));
        std::uint32_t const unit_cycle_number = static_cast<std::uint32_t>(
            anim.time / std::max(0.001F, unit_cycle_period));

        int const max_slots = std::min(2, visible_count);
        std::uint32_t const cycle_rng =
            hash_u32(seed ^ (unit_cycle_number * 0x9e3779b9U));
        int const slot_count =
            (max_slots <= 0)
                ? 0
                : (1 + static_cast<int>(cycle_rng %
                                        static_cast<std::uint32_t>(max_slots)));

        int const idx_a =
            static_cast<int>(hash_u32(cycle_rng ^ 0xA341316CU) %
                             static_cast<std::uint32_t>(visible_count));
        int idx_b = static_cast<int>(hash_u32(cycle_rng ^ 0xC8013EA4U) %
                                     static_cast<std::uint32_t>(visible_count));
        if (visible_count > 1 && idx_b == idx_a) {
          idx_b = (idx_a + 1 +
                   static_cast<int>(cycle_rng % static_cast<std::uint32_t>(
                                                    visible_count - 1))) %
                  visible_count;
        }

        auto pick_idle_type = [&](std::uint32_t v) -> AmbientIdleType {
          std::uint32_t const roll = v % 100U;
          if (roll < 18U) {
            return AmbientIdleType::SitDown;
          }
          if (roll < 36U) {
            return AmbientIdleType::ShiftWeight;
          }
          if (roll < 52U) {
            return AmbientIdleType::ShuffleFeet;
          }
          if (roll < 66U) {
            return AmbientIdleType::TapFoot;
          }
          if (roll < 78U) {
            return AmbientIdleType::StepInPlace;
          }
          if (roll < 90U) {
            return AmbientIdleType::BendKnee;
          }
          if (roll < 98U) {
            return AmbientIdleType::RaiseWeapon;
          }
          return AmbientIdleType::Jump;
        };

        AmbientIdleType const unit_idle_type =
            pick_idle_type(hash_u32(cycle_rng ^ 0x3C6EF372U));

        if (slot_count > 0 && unit_time_in_cycle <= k_ambient_anim_duration) {
          bool const is_active =
              (idx == idx_a) || (slot_count > 1 && idx == idx_b);
          if (is_active) {
            float const phase = unit_time_in_cycle / k_ambient_anim_duration;
            pose_ctrl.apply_ambient_idle_explicit(unit_idle_type, phase);
          }
        }
      }
    }

    if (anim_ctx.motion_state == HumanoidMotionState::Run) {

      float const run_lean = 0.10F;
      pose.pelvis_pos.setZ(pose.pelvis_pos.z() - 0.05F);
      pose.shoulder_l.setZ(pose.shoulder_l.z() + run_lean);
      pose.shoulder_r.setZ(pose.shoulder_r.z() + run_lean);
      pose.neck_base.setZ(pose.neck_base.z() + run_lean * 0.7F);
      pose.head_pos.setZ(pose.head_pos.z() + run_lean * 0.5F);

      pose.pelvis_pos.setY(pose.pelvis_pos.y() - 0.03F);
      pose.shoulder_l.setY(pose.shoulder_l.y() - 0.04F);
      pose.shoulder_r.setY(pose.shoulder_r.y() - 0.04F);

      float const run_stride_mult = 1.5F;
      float const phase = anim_ctx.locomotion_phase;
      float const left_phase = phase;
      float const right_phase = std::fmod(phase + 0.5F, 1.0F);

      auto enhance_run_foot = [&](QVector3D &foot, float foot_phase) {
        float const lift_raw =
            std::sin(foot_phase * 2.0F * std::numbers::pi_v<float>);
        if (lift_raw > 0.0F) {

          float const extra_lift = lift_raw * k_run_extra_foot_lift;
          foot.setY(foot.y() + extra_lift);

          float const stride_enhance = std::sin((foot_phase - 0.25F) * 2.0F *
                                                std::numbers::pi_v<float>) *
                                       k_run_stride_enhancement;
          foot.setZ(foot.z() + stride_enhance);
        }
      };

      enhance_run_foot(pose.foot_l, left_phase);
      enhance_run_foot(pose.foot_r, right_phase);

      float const run_arm_swing = 0.06F;
      constexpr float max_run_arm_displacement = 0.08F;
      float const left_swing_raw =
          std::sin((left_phase + 0.1F) * 2.0F * std::numbers::pi_v<float>);
      float const left_arm_phase =
          std::clamp(left_swing_raw * run_arm_swing, -max_run_arm_displacement,
                     max_run_arm_displacement);
      float const right_swing_raw =
          std::sin((right_phase + 0.1F) * 2.0F * std::numbers::pi_v<float>);
      float const right_arm_phase =
          std::clamp(right_swing_raw * run_arm_swing, -max_run_arm_displacement,
                     max_run_arm_displacement);

      pose.hand_l.setZ(pose.hand_l.z() - left_arm_phase);
      pose.hand_r.setZ(pose.hand_r.z() - right_arm_phase);

      pose.hand_l.setY(pose.hand_l.y() + 0.02F);
      pose.hand_r.setY(pose.hand_r.y() + 0.02F);

      {
        using HP = HumanProportions;
        float const h_scale = variation.height_scale;
        float const max_reach =
            (HP::UPPER_ARM_LEN + HP::FORE_ARM_LEN) * h_scale * 0.98F;

        auto clamp_hand = [&](const QVector3D &shoulder, QVector3D &hand) {
          QVector3D diff = hand - shoulder;
          float const len = diff.length();
          if (len > max_reach && len > 1e-6F) {
            hand = shoulder + diff * (max_reach / len);
          }
        };
        clamp_hand(pose.shoulder_l, pose.hand_l);
        clamp_hand(pose.shoulder_r, pose.hand_r);

        QVector3D right_axis = pose.shoulder_r - pose.shoulder_l;
        right_axis.setY(0.0F);
        if (right_axis.lengthSquared() < 1e-8F) {
          right_axis = QVector3D(1.0F, 0.0F, 0.0F);
        }
        right_axis.normalize();

        if (right_axis.x() < 0.0F) {
          right_axis = -right_axis;
        }
        QVector3D const outward_l = -right_axis;
        QVector3D const outward_r = right_axis;

        pose.elbow_l = elbow_bend_torso(pose.shoulder_l, pose.hand_l, outward_l,
                                        0.45F, 0.15F, -0.08F, +1.0F);
        pose.elbow_r = elbow_bend_torso(pose.shoulder_r, pose.hand_r, outward_r,
                                        0.48F, 0.12F, 0.02F, +1.0F);
      }

      float const hip_rotation_raw =
          std::sin(phase * 2.0F * std::numbers::pi_v<float>);
      float const hip_rotation = hip_rotation_raw * 0.003F;
      pose.pelvis_pos.setX(pose.pelvis_pos.x() + hip_rotation);

      float const torso_sway_z = 0.0F;
      pose.shoulder_l.setZ(pose.shoulder_l.z() + torso_sway_z);
      pose.shoulder_r.setZ(pose.shoulder_r.z() + torso_sway_z);
      pose.neck_base.setZ(pose.neck_base.z() + torso_sway_z * 0.7F);
      pose.head_pos.setZ(pose.head_pos.z() + torso_sway_z * 0.5F);

      if (pose.head_frame.radius > 0.001F) {
        QVector3D head_up = pose.head_pos - pose.neck_base;
        if (head_up.lengthSquared() < 1e-8F) {
          head_up = pose.head_frame.up;
        } else {
          head_up.normalize();
        }

        QVector3D head_right =
            pose.head_frame.right -
            head_up * QVector3D::dotProduct(pose.head_frame.right, head_up);
        if (head_right.lengthSquared() < 1e-8F) {
          head_right =
              QVector3D::crossProduct(head_up, anim_ctx.entity_forward);
          if (head_right.lengthSquared() < 1e-8F) {
            head_right = QVector3D(1.0F, 0.0F, 0.0F);
          }
        }
        head_right.normalize();
        QVector3D head_forward =
            QVector3D::crossProduct(head_right, head_up).normalized();

        pose.head_frame.origin = pose.head_pos;
        pose.head_frame.up = head_up;
        pose.head_frame.right = head_right;
        pose.head_frame.forward = head_forward;
        pose.body_frames.head = pose.head_frame;
      }
    }

    const auto &gfx_settings = Render::GraphicsSettings::instance();
    const bool should_render_shadow =
        ctx.allow_template_cache && gfx_settings.shadows_enabled() &&
        (soldier_lod == HumanoidLOD::Full ||
         soldier_lod == HumanoidLOD::Reduced) &&
        soldier_distance < gfx_settings.shadow_max_distance();

    if (should_render_shadow && inst_ctx.backend != nullptr &&
        inst_ctx.resources != nullptr) {
      auto *shadow_shader =
          inst_ctx.backend->shader(QStringLiteral("troop_shadow"));
      auto *quad_mesh = inst_ctx.resources->quad();

      if (shadow_shader != nullptr && quad_mesh != nullptr) {

        float const shadow_size =
            is_mounted_spawn ? k_shadow_size_mounted : k_shadow_size_infantry;
        float depth_boost = 1.0F;
        float width_boost = 1.0F;
        if (unit_comp != nullptr) {
          using Game::Units::SpawnType;
          switch (unit_comp->spawn_type) {
          case SpawnType::Spearman:
            depth_boost = 1.8F;
            width_boost = 0.95F;
            break;
          case SpawnType::HorseSpearman:
            depth_boost = 2.1F;
            width_boost = 1.05F;
            break;
          case SpawnType::Archer:
          case SpawnType::HorseArcher:
            depth_boost = 1.2F;
            width_boost = 0.95F;
            break;
          default:
            break;
          }
        }

        float const shadow_width =
            shadow_size * (is_mounted_spawn ? 1.05F : 1.0F) * width_boost;
        float const shadow_depth =
            shadow_size * (is_mounted_spawn ? 1.30F : 1.10F) * depth_boost;

        auto &terrain_service = Game::Map::TerrainService::instance();

        if (terrain_service.is_initialized()) {

          QVector3D const inst_pos =
              inst_ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));
          float const shadow_y =
              terrain_service.get_terrain_height(inst_pos.x(), inst_pos.z());

          QVector3D light_dir = k_shadow_light_dir.normalized();
          QVector2D light_dir_xz(light_dir.x(), light_dir.z());
          if (light_dir_xz.lengthSquared() < 1e-6F) {
            light_dir_xz = QVector2D(0.0F, 1.0F);
          } else {
            light_dir_xz.normalize();
          }
          QVector2D const shadow_dir = -light_dir_xz;
          QVector2D dir_for_use = shadow_dir;
          if (dir_for_use.lengthSquared() < 1e-6F) {
            dir_for_use = QVector2D(0.0F, 1.0F);
          } else {
            dir_for_use.normalize();
          }
          float const shadow_offset = shadow_depth * 1.25F;
          QVector2D const offset_2d = dir_for_use * shadow_offset;
          float const light_yaw_deg = qRadiansToDegrees(
              std::atan2(double(dir_for_use.x()), double(dir_for_use.y())));

          QMatrix4x4 shadow_model;
          shadow_model.translate(inst_pos.x() + offset_2d.x(),
                                 shadow_y + k_shadow_ground_offset,
                                 inst_pos.z() + offset_2d.y());
          shadow_model.rotate(light_yaw_deg, 0.0F, 1.0F, 0.0F);
          shadow_model.rotate(-90.0F, 1.0F, 0.0F, 0.0F);
          shadow_model.scale(shadow_width, shadow_depth, 1.0F);

          out.post_body_draws.emplace_back(
              [shadow_shader, quad_mesh, shadow_model,
               dir_for_use](Render::GL::ISubmitter &submitter) {
                if (auto *renderer = dynamic_cast<Renderer *>(&submitter)) {
                  Shader *previous_shader = renderer->get_current_shader();
                  renderer->set_current_shader(shadow_shader);
                  shadow_shader->set_uniform(QStringLiteral("u_lightDir"),
                                             dir_for_use);

                  submitter.mesh(quad_mesh, shadow_model,
                                 QVector3D(0.0F, 0.0F, 0.0F), nullptr,
                                 k_shadow_base_alpha, 0);

                  renderer->set_current_shader(previous_shader);
                }
              });
        }
      }
    }

    switch (soldier_lod) {
    case HumanoidLOD::Full: {

      ++s_render_stats.lod_full;

      Render::Humanoid::HumanoidBodyMetrics metrics{};
      Render::Humanoid::compute_humanoid_body_metrics(
          pose, owner.get_proportion_scaling(), owner.get_torso_scale(),
          metrics);
      Render::Humanoid::compute_humanoid_head_frame(pose, metrics);
      Render::Humanoid::compute_humanoid_body_frames(pose, metrics);

      enqueue_prepared_body(pose, variant, anim_ctx, inst_ctx, inst_seed,
                            Render::Creature::CreatureLOD::Full);

      using Render::Creature::Pipeline::LegacySlotMask;
      using Render::Creature::Pipeline::owns_slot;
      const auto owned_slots = owner.visual_spec().owned_legacy_slots;
      if (!owns_slot(owned_slots, LegacySlotMask::ArmorOverlay) ||
          !owns_slot(owned_slots, LegacySlotMask::ShoulderDecorations) ||
          !owns_slot(owned_slots, LegacySlotMask::FacialHair) ||
          !owns_slot(owned_slots, LegacySlotMask::Attachments)) {
        out.post_body_draws.emplace_back(
            [&owner, inst_ctx, variant, pose, anim_ctx, metrics,
             owned_slots](Render::GL::ISubmitter &submitter) {
              using Render::Creature::Pipeline::LegacySlotMask;
              using Render::Creature::Pipeline::owns_slot;
              if (!owns_slot(owned_slots, LegacySlotMask::ArmorOverlay)) {
                owner.draw_armor_overlay(
                    inst_ctx, variant, pose, metrics.y_top_cover,
                    metrics.torso_r, metrics.shoulder_half_span,
                    metrics.upper_arm_r, metrics.right_axis, submitter);
              }
              if (!owns_slot(owned_slots,
                             LegacySlotMask::ShoulderDecorations)) {
                owner.draw_shoulder_decorations(
                    inst_ctx, variant, pose, metrics.y_top_cover,
                    pose.neck_base.y(), metrics.right_axis, submitter);
              }
              if (!owns_slot(owned_slots, LegacySlotMask::FacialHair)) {
                owner.draw_facial_hair(inst_ctx, variant, pose, submitter);
              }
              if (!owns_slot(owned_slots, LegacySlotMask::Attachments)) {
                owner.add_attachments(inst_ctx, variant, pose, anim_ctx,
                                      submitter);
              }
            });
      }
      break;
    }

    case HumanoidLOD::Reduced: {

      ++s_render_stats.lod_reduced;
      enqueue_prepared_body(pose, variant, anim_ctx, inst_ctx, inst_seed,
                            Render::Creature::CreatureLOD::Reduced);
      using Render::Creature::Pipeline::LegacySlotMask;
      using Render::Creature::Pipeline::owns_slot;
      const auto owned_slots = owner.visual_spec().owned_legacy_slots;
      if (!owns_slot(owned_slots, LegacySlotMask::Attachments)) {
        out.post_body_draws.emplace_back(
            [&owner, inst_ctx, variant, pose,
             anim_ctx](Render::GL::ISubmitter &submitter) {
              owner.add_attachments(inst_ctx, variant, pose, anim_ctx,
                                    submitter);
            });
      }
      break;
    }

    case HumanoidLOD::Minimal:

      ++s_render_stats.lod_minimal;
      enqueue_prepared_body(pose, variant, anim_ctx, inst_ctx, inst_seed,
                            Render::Creature::CreatureLOD::Minimal);
      break;

    case HumanoidLOD::Billboard:

      break;
    }
  }
}

} // namespace Render::Humanoid
