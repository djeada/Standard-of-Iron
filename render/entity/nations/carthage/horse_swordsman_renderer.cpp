#include "horse_swordsman_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../equipment/equipment_registry.h"
#include "../../../equipment/weapons/shield_renderer.h"
#include "../../../equipment/weapons/sword_renderer.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/shader.h"
#include "../../../humanoid/humanoid_math.h"
#include "../../../humanoid/humanoid_specs.h"
#include "../../../humanoid/mounted_pose_controller.h"
#include "../../../humanoid/pose_controller.h"
#include "../../../humanoid/rig.h"
#include "../../../palette.h"
#include "../../../scene_renderer.h"
#include "../../../submitter.h"
#include "../../horse_renderer.h"
#include "../../registry.h"
#include "../../renderer_constants.h"
#include <numbers>
#include <qmatrix4x4.h>
#include <qstringliteral.h>
#include <qvectornd.h>
#include <unordered_map>

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Render::GL::Carthage {

using Render::Geom::clamp01;
using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::easeInOutCubic;
using Render::Geom::smoothstep;
using Render::Geom::sphereAt;

struct MountedKnightExtras {
  QVector3D metalColor;
  HorseProfile horseProfile;
  float swordLength = 0.85F;
  float swordWidth = 0.045F;
  bool hasSword = true;
  bool hasCavalryShield = false;
};

class MountedKnightRenderer : public HumanoidRendererBase {
public:
  auto get_proportion_scaling() const -> QVector3D override {

    return {1.F, 1.7F, 1.F};
  }

  auto get_mount_scale() const -> float override { return 0.75F; }

  void adjust_variation(const DrawContext &, uint32_t,
                        VariationParams &variation) const override {
    variation.height_scale = 0.92F;
    variation.bulkScale = 0.80F;
    variation.stanceWidth = 0.65F;
    variation.armSwingAmp = 0.55F;
    variation.walkSpeedMult = 1.0F;
    variation.postureSlump = 0.0F;
    variation.shoulderTilt = 0.0F;
  }

private:
  mutable std::unordered_map<uint32_t, MountedKnightExtras> m_extrasCache;
  HorseRenderer m_horseRenderer;
  mutable const HumanoidPose *m_lastPose = nullptr;
  mutable MountedAttachmentFrame m_lastMount{};
  mutable ReinState m_lastReinState{};
  mutable bool m_hasLastReins = false;

  auto getScaledHorseDimensions(uint32_t seed) const -> HorseDimensions {
    HorseDimensions dims = makeHorseDimensions(seed);
    scaleHorseDimensions(dims, get_mount_scale());
    return dims;
  }

public:
  void get_variant(const DrawContext &ctx, uint32_t seed,
                   HumanoidVariant &v) const override {
    QVector3D const team_tint = resolveTeamTint(ctx);
    v.palette = makeHumanoidPalette(team_tint, seed);
  }

  auto resolve_shader_key(const DrawContext &ctx) const -> QString {
    std::string nation;
    if (ctx.entity != nullptr) {
      if (auto *unit =
              ctx.entity->getComponent<Engine::Core::UnitComponent>()) {
        nation = Game::Systems::nationIDToString(unit->nation_id);
      }
    }
    if (!nation.empty()) {
      return QString::fromStdString(std::string("horse_swordsman_") + nation);
    }
    return QStringLiteral("horse_swordsman");
  }

  void customize_pose(const DrawContext &ctx,
                      const HumanoidAnimationContext &anim_ctx, uint32_t seed,
                      HumanoidPose &pose) const override {
    using HP = HumanProportions;

    const AnimationInputs &anim = anim_ctx.inputs;

    uint32_t horse_seed = seed;
    if (ctx.entity != nullptr) {
      horse_seed = static_cast<uint32_t>(
          reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
    }

    HorseDimensions dims = getScaledHorseDimensions(horse_seed);
    HorseProfile mount_profile{};
    mount_profile.dims = dims;
    MountedAttachmentFrame mount = compute_mount_frame(mount_profile);
    HorseMotionSample const motion =
        evaluate_horse_motion(mount_profile, anim, anim_ctx);
    apply_mount_vertical_offset(mount, motion.bob);

    m_lastPose = &pose;
    m_lastMount = mount;

    ReinState const reins = compute_rein_state(horse_seed, anim_ctx);
    m_lastReinState = reins;
    m_hasLastReins = true;

    MountedPoseController mounted_controller(pose, anim_ctx);
    mounted_controller.mountOnHorse(mount);

    float const speed_norm = anim_ctx.locomotion_normalized_speed();
    float const speed_lean = std::clamp(
        anim_ctx.locomotion_speed() * 0.10F + speed_norm * 0.05F, 0.0F, 0.22F);
    float const forward_lean =
        (dims.seatForwardOffset * 0.08F + speed_lean) / 0.15F;
    mounted_controller.ridingLeaning(mount, forward_lean, 0.0F);

    if (anim.is_attacking && anim.isMelee) {

      float const attack_phase =
          std::fmod(anim.time * MOUNTED_KNIGHT_INV_ATTACK_CYCLE_TIME, 1.0F);
      mounted_controller.ridingMeleeStrike(mount, attack_phase);
    } else {
      mounted_controller.holdReins(mount, reins.slack, reins.slack,
                                   reins.tension, reins.tension);
    }
  }

  void addAttachments(const DrawContext &ctx, const HumanoidVariant &v,
                      const HumanoidPose &pose,
                      const HumanoidAnimationContext &anim_ctx,
                      ISubmitter &out) const override {
    const AnimationInputs &anim = anim_ctx.inputs;
    uint32_t horse_seed = 0U;
    if (ctx.entity != nullptr) {
      horse_seed = static_cast<uint32_t>(
          reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
    }

    MountedKnightExtras extras;
    auto it = m_extrasCache.find(horse_seed);
    if (it != m_extrasCache.end()) {
      extras = it->second;
    } else {
      HorseDimensions dims = getScaledHorseDimensions(horse_seed);
      extras = computeMountedKnightExtras(horse_seed, v, dims);
      m_extrasCache[horse_seed] = extras;

      if (m_extrasCache.size() > MAX_EXTRAS_CACHE_SIZE) {
        m_extrasCache.clear();
      }
    }

    const bool is_current_pose = (m_lastPose == &pose);
    const MountedAttachmentFrame *mount_ptr =
        (is_current_pose) ? &m_lastMount : nullptr;
    const ReinState *rein_ptr =
        (is_current_pose && m_hasLastReins) ? &m_lastReinState : nullptr;
    m_horseRenderer.render(ctx, anim, anim_ctx, extras.horseProfile, mount_ptr,
                           rein_ptr, out);
    m_lastPose = nullptr;
    m_hasLastReins = false;

    bool const is_attacking = anim.is_attacking && anim.isMelee;

    auto &registry = EquipmentRegistry::instance();

    if (extras.hasSword) {
      auto sword = registry.get(EquipmentCategory::Weapon, "sword");
      if (sword) {
        SwordRenderConfig sword_config;
        sword_config.metal_color = extras.metalColor;
        sword_config.sword_length = extras.swordLength;
        sword_config.sword_width = extras.swordWidth;

        auto *sword_renderer = dynamic_cast<SwordRenderer *>(sword.get());
        if (sword_renderer) {
          sword_renderer->setConfig(sword_config);
        }
        sword->render(ctx, pose.bodyFrames, v.palette, anim_ctx, out);
      }
    }

    if (extras.hasCavalryShield) {
      auto shield = registry.get(EquipmentCategory::Weapon, "shield");
      if (shield) {
        ShieldRenderConfig shield_config;
        shield_config.metal_color = extras.metalColor;

        auto *shield_renderer = dynamic_cast<ShieldRenderer *>(shield.get());
        if (shield_renderer) {
          shield_renderer->setConfig(shield_config);
        }
        shield->render(ctx, pose.bodyFrames, v.palette, anim_ctx, out);
      }
    }
  }

  void draw_helmet(const DrawContext &ctx, const HumanoidVariant &v,
                   const HumanoidPose &pose, ISubmitter &out) const override {

    auto &registry = EquipmentRegistry::instance();
    auto helmet = registry.get(EquipmentCategory::Helmet, "montefortino");
    if (helmet) {
      HumanoidAnimationContext anim_ctx{};
      helmet->render(ctx, pose.bodyFrames, v.palette, anim_ctx, out);
    }
  }

  void draw_armor(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose,
                  const HumanoidAnimationContext &anim,
                  ISubmitter &out) const override {
    auto &registry = EquipmentRegistry::instance();
    auto armor = registry.get(EquipmentCategory::Armor, "armor_heavy_carthage");
    if (armor) {
      armor->render(ctx, pose.bodyFrames, v.palette, anim, out);
    }
  }

private:
  static auto computeMountedKnightExtras(
      uint32_t seed, const HumanoidVariant &v,
      const HorseDimensions &dims) -> MountedKnightExtras {
    MountedKnightExtras e;

    e.metalColor = QVector3D(0.72F, 0.73F, 0.78F);

    e.horseProfile = makeHorseProfile(seed, v.palette.leather, v.palette.cloth);

    e.horseProfile.dims = dims;

    e.swordLength = 0.82F + (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.12F;
    e.swordWidth = 0.042F + (hash_01(seed ^ 0x7777U) - 0.5F) * 0.008F;

    e.hasSword = (hash_01(seed ^ 0xFACEU) > 0.15F);
    e.hasCavalryShield = (hash_01(seed ^ 0xCAFEU) > 0.60F);

    return e;
  }
};
void registerMountedKnightRenderer(
    Render::GL::EntityRendererRegistry &registry) {
  registry.register_renderer(
      "troops/carthage/horse_swordsman",
      [](const DrawContext &ctx, ISubmitter &out) {
        static MountedKnightRenderer const static_renderer;
        Shader *horse_swordsman_shader = nullptr;
        if (ctx.backend != nullptr) {
          QString shader_key = static_renderer.resolve_shader_key(ctx);
          horse_swordsman_shader = ctx.backend->shader(shader_key);
          if (horse_swordsman_shader == nullptr) {
            horse_swordsman_shader =
                ctx.backend->shader(QStringLiteral("horse_swordsman"));
          }
        }
        auto *scene_renderer = dynamic_cast<Renderer *>(&out);
        if ((scene_renderer != nullptr) &&
            (horse_swordsman_shader != nullptr)) {
          scene_renderer->setCurrentShader(horse_swordsman_shader);
        }
        static_renderer.render(ctx, out);
        if (scene_renderer != nullptr) {
          scene_renderer->setCurrentShader(nullptr);
        }
      });
}

} // namespace Render::GL::Carthage
