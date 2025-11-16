#include "horse_archer_renderer_base.h"

#include "../equipment/equipment_registry.h"
#include "../equipment/weapons/bow_renderer.h"
#include "../equipment/weapons/quiver_renderer.h"
#include "../humanoid/humanoid_math.h"
#include "../humanoid/humanoid_specs.h"
#include "../humanoid/mounted_pose_controller.h"
#include "../palette.h"

#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/systems/nation_id.h"

#include "mounted_knight_pose.h"
#include "renderer_constants.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <utility>

namespace Render::GL {

namespace {

constexpr QVector3D k_default_proportion_scale{0.92F, 0.88F, 0.96F};

}

HorseArcherRendererBase::HorseArcherRendererBase(
    HorseArcherRendererConfig config)
    : m_config(std::move(config)) {
  m_config.has_bow = m_config.has_bow && !m_config.bow_equipment_id.empty();
  if (!m_config.has_bow) {
    m_config.bow_equipment_id.clear();
  }

  m_config.has_quiver =
      m_config.has_quiver && !m_config.quiver_equipment_id.empty();
  if (!m_config.has_quiver) {
    m_config.quiver_equipment_id.clear();
  }

  m_horseRenderer.setAttachments(m_config.horse_attachments);
}

auto HorseArcherRendererBase::get_proportion_scaling() const -> QVector3D {
  return k_default_proportion_scale;
}

auto HorseArcherRendererBase::get_mount_scale() const -> float {
  return m_config.mount_scale;
}

void HorseArcherRendererBase::adjust_variation(
    const DrawContext &, uint32_t, VariationParams &variation) const {
  variation.height_scale = 0.88F;
  variation.bulk_scale = 0.78F;
  variation.stance_width = 0.60F;
  variation.arm_swing_amp = 0.45F;
  variation.walk_speed_mult = 1.0F;
  variation.posture_slump = 0.0F;
  variation.shoulder_tilt = 0.0F;
}

void HorseArcherRendererBase::get_variant(const DrawContext &ctx, uint32_t seed,
                                          HumanoidVariant &v) const {
  QVector3D const team_tint = resolveTeamTint(ctx);
  v.palette = makeHumanoidPalette(team_tint, seed);
}

auto HorseArcherRendererBase::get_scaled_horse_dimensions(uint32_t seed) const
    -> HorseDimensions {
  HorseDimensions dims = makeHorseDimensions(seed);
  scaleHorseDimensions(dims, get_mount_scale());
  return dims;
}

void HorseArcherRendererBase::customize_pose(
    const DrawContext &ctx, const HumanoidAnimationContext &anim_ctx,
    uint32_t seed, HumanoidPose &pose) const {
  const AnimationInputs &anim = anim_ctx.inputs;

  uint32_t horse_seed = seed;
  if (ctx.entity != nullptr) {
    horse_seed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx.entity) &
                                       0xFFFFFFFFU);
  }

  HorseDimensions dims = get_scaled_horse_dimensions(horse_seed);
  HorseProfile mount_profile{};
  mount_profile.dims = dims;
  MountedAttachmentFrame mount = compute_mount_frame(mount_profile);
  tuneMountedKnightFrame(dims, mount);
  HorseMotionSample const motion =
      evaluate_horse_motion(mount_profile, anim, anim_ctx);
  apply_mount_vertical_offset(mount, motion.bob);

  m_last_pose = &pose;
  m_last_mount = mount;

  ReinState const reins = compute_rein_state(horse_seed, anim_ctx);
  m_last_rein_state = reins;
  m_has_last_reins = true;

  MountedPoseController mounted_controller(pose, anim_ctx);

  mounted_controller.mountOnHorse(mount);

  if (anim.is_attacking && !anim.is_melee) {
    float const attack_phase =
        std::fmod(anim.time * ARCHER_INV_ATTACK_CYCLE_TIME, 1.0F);
    mounted_controller.ridingBowShot(mount, attack_phase);
  } else {
    mounted_controller.ridingIdle(mount);
  }

  applyMountedKnightLowerBody(dims, mount, anim_ctx, pose);
}

auto HorseArcherRendererBase::compute_horse_archer_extras(
    uint32_t seed, const HumanoidVariant &v,
    const HorseDimensions &dims) const -> HorseArcherExtras {
  HorseArcherExtras extras;
  extras.horse_profile =
      makeHorseProfile(seed, v.palette.leather, v.palette.cloth);
  extras.horse_profile.dims = dims;
  extras.bow_length = 0.75F + (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.10F;

  return extras;
}

void HorseArcherRendererBase::addAttachments(
    const DrawContext &ctx, const HumanoidVariant &v, const HumanoidPose &pose,
    const HumanoidAnimationContext &anim_ctx, ISubmitter &out) const {
  uint32_t horse_seed = 0U;
  if (ctx.entity != nullptr) {
    horse_seed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx.entity) &
                                       0xFFFFFFFFU);
  }

  HorseArcherExtras extras;
  auto it = m_extras_cache.find(horse_seed);
  if (it != m_extras_cache.end()) {
    extras = it->second;
  } else {
    HorseDimensions dims = get_scaled_horse_dimensions(horse_seed);
    extras = compute_horse_archer_extras(horse_seed, v, dims);
    m_extras_cache[horse_seed] = extras;

    if (m_extras_cache.size() > MAX_EXTRAS_CACHE_SIZE) {
      m_extras_cache.clear();
    }
  }

  const bool is_current_pose = (m_last_pose == &pose);
  const MountedAttachmentFrame *mount_ptr =
      (is_current_pose) ? &m_last_mount : nullptr;
  const ReinState *rein_ptr =
      (is_current_pose && m_has_last_reins) ? &m_last_rein_state : nullptr;
  const AnimationInputs &anim = anim_ctx.inputs;

  // CRITICAL FIX: Update gait parameters in extras.horse_profile before rendering!
  // The profile from cache has old random stride/lift values (0.26-0.32, 0.10-0.14)
  // We need to update them with proper gait-based values before rendering
  evaluate_horse_motion(extras.horse_profile, anim, anim_ctx);

  m_horseRenderer.render(ctx, anim, anim_ctx, extras.horse_profile, mount_ptr,
                         rein_ptr, out);
  m_last_pose = nullptr;
  m_has_last_reins = false;

  auto &registry = EquipmentRegistry::instance();

  if (m_config.has_bow && !m_config.bow_equipment_id.empty()) {
    auto bow =
        registry.get(EquipmentCategory::Weapon, m_config.bow_equipment_id);
    if (bow) {
      BowRenderConfig bow_config;
      bow_config.string_color = QVector3D(0.30F, 0.30F, 0.32F);
      bow_config.metal_color = m_config.metal_color;
      bow_config.fletching_color = m_config.fletching_color;
      bow_config.bow_top_y = HumanProportions::SHOULDER_Y + 0.55F;
      bow_config.bow_bot_y = HumanProportions::WAIST_Y - 0.25F;
      bow_config.bow_x = 0.0F;

      if (auto *bow_renderer = dynamic_cast<BowRenderer *>(bow.get())) {
        bow_renderer->setConfig(bow_config);
      }
      bow->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }
  }

  if (m_config.has_quiver && !m_config.quiver_equipment_id.empty()) {
    auto quiver =
        registry.get(EquipmentCategory::Weapon, m_config.quiver_equipment_id);
    if (quiver) {
      QuiverRenderConfig quiver_config;
      quiver_config.fletching_color = m_config.fletching_color;
      quiver_config.quiver_radius = HumanProportions::HEAD_RADIUS * 0.45F;

      if (auto *quiver_renderer =
              dynamic_cast<QuiverRenderer *>(quiver.get())) {
        quiver_renderer->setConfig(quiver_config);
      }
      quiver->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
    }
  }
}

void HorseArcherRendererBase::draw_helmet(const DrawContext &ctx,
                                          const HumanoidVariant &v,
                                          const HumanoidPose &pose,
                                          ISubmitter &out) const {
  if (m_config.helmet_equipment_id.empty()) {
    return;
  }

  auto &registry = EquipmentRegistry::instance();
  auto helmet =
      registry.get(EquipmentCategory::Helmet, m_config.helmet_equipment_id);
  if (helmet) {
    HumanoidAnimationContext anim_ctx{};
    helmet->render(ctx, pose.body_frames, v.palette, anim_ctx, out);
  }
}

void HorseArcherRendererBase::draw_armor(const DrawContext &ctx,
                                         const HumanoidVariant &v,
                                         const HumanoidPose &pose,
                                         const HumanoidAnimationContext &anim,
                                         ISubmitter &out) const {
  if (m_config.armor_equipment_id.empty()) {
    return;
  }

  auto &registry = EquipmentRegistry::instance();
  auto armor =
      registry.get(EquipmentCategory::Armor, m_config.armor_equipment_id);
  if (armor) {
    armor->render(ctx, pose.body_frames, v.palette, anim, out);
  }
}

auto HorseArcherRendererBase::resolve_shader_key(const DrawContext &ctx) const
    -> QString {
  std::string nation;
  if (ctx.entity != nullptr) {
    if (auto *unit = ctx.entity->getComponent<Engine::Core::UnitComponent>()) {
      nation = Game::Systems::nationIDToString(unit->nation_id);
    }
  }
  if (!nation.empty()) {
    return QString::fromStdString(std::string("horse_archer_") + nation);
  }
  return QStringLiteral("horse_archer");
}

} // namespace Render::GL
