#include "horse_archer_renderer_base.h"

#include "../equipment/armor/cloak_renderer.h"
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

constexpr QVector3D k_default_proportion_scale{0.80F, 0.88F, 0.88F};

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

  m_horseRenderer.set_attachments(m_config.horse_attachments);
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
  variation.bulk_scale = 0.72F;
  variation.stance_width = 0.60F;
  variation.arm_swing_amp = 0.45F;
  variation.walk_speed_mult = 1.0F;
  variation.posture_slump = 0.0F;
  variation.shoulder_tilt = 0.0F;
}

void HorseArcherRendererBase::get_variant(const DrawContext &ctx, uint32_t seed,
                                          HumanoidVariant &v) const {
  QVector3D const team_tint = resolve_team_tint(ctx);
  v.palette = make_humanoid_palette(team_tint, seed);
}

void HorseArcherRendererBase::apply_riding_animation(
    MountedPoseController &mounted_controller, MountedAttachmentFrame &mount,
    const HumanoidAnimationContext &anim_ctx, HumanoidPose &pose,
    const HorseDimensions &dims, const ReinState &reins) const {
  (void)pose;
  (void)dims;
  (void)reins;
  const AnimationInputs &anim = anim_ctx.inputs;
  if (anim.is_attacking && !anim.is_melee) {
    float const attack_phase =
        std::fmod(anim.time * ARCHER_INV_ATTACK_CYCLE_TIME, 1.0F);
    mounted_controller.ridingBowShot(mount, attack_phase);
  } else {
    mounted_controller.ridingIdle(mount);
  }
}

void HorseArcherRendererBase::draw_equipment(
    const DrawContext &ctx, const HumanoidVariant &v, const HumanoidPose &pose,
    const HumanoidAnimationContext &anim_ctx, ISubmitter &out) const {
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
        bow_renderer->set_config(bow_config);
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
        quiver_renderer->set_config(quiver_config);
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
    BodyFrames frames = pose.body_frames;
    if (ctx.entity != nullptr) {
      auto *move = ctx.entity->get_component<Engine::Core::MovementComponent>();
      if (move != nullptr) {
        float speed_sq = move->vx * move->vx + move->vz * move->vz;
        if (speed_sq > 0.0001F && m_config.helmet_offset_moving > 0.0F) {
          frames.head.origin +=
              frames.head.forward * m_config.helmet_offset_moving;
        }
      }
    }
    helmet->render(ctx, frames, v.palette, anim_ctx, out);
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

  if (m_config.has_cloak && !m_config.cloak_equipment_id.empty()) {
    auto cloak =
        registry.get(EquipmentCategory::Armor, m_config.cloak_equipment_id);
    if (cloak) {
      CloakConfig cloak_config;
      cloak_config.primary_color = m_config.cloak_color;
      cloak_config.trim_color = m_config.cloak_trim_color;
      cloak_config.back_material_id = m_config.cloak_back_material_id;
      cloak_config.shoulder_material_id = m_config.cloak_shoulder_material_id;

      if (auto *cloak_renderer = dynamic_cast<CloakRenderer *>(cloak.get())) {
        cloak_renderer->set_config(cloak_config);
      }

      cloak->render(ctx, pose.body_frames, v.palette, anim, out);
    }
  }
}

auto HorseArcherRendererBase::resolve_shader_key(const DrawContext &ctx) const
    -> QString {
  std::string nation;
  if (ctx.entity != nullptr) {
    if (auto *unit = ctx.entity->get_component<Engine::Core::UnitComponent>()) {
      nation = Game::Systems::nation_id_to_string(unit->nation_id);
    }
  }
  if (!nation.empty()) {
    return QString::fromStdString(std::string("horse_archer_") + nation);
  }
  return QStringLiteral("horse_archer");
}

} // namespace Render::GL
