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

namespace Render::GL::Roman {

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
  auto getProportionScaling() const -> QVector3D override {
    return {1.40F, 1.05F, 1.10F};
  }

private:
  mutable std::unordered_map<uint32_t, MountedKnightExtras> m_extrasCache;
  HorseRenderer m_horseRenderer;

public:
  void getVariant(const DrawContext &ctx, uint32_t seed,
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

  void customizePose(const DrawContext &ctx,
                     const HumanoidAnimationContext &anim_ctx, uint32_t seed,
                     HumanoidPose &pose) const override {
    using HP = HumanProportions;

    const AnimationInputs &anim = anim_ctx.inputs;

    const float arm_height_jitter = (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.03F;
    const float arm_asymmetry = (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.04F;

    uint32_t horse_seed = seed;
    if (ctx.entity != nullptr) {
      horse_seed = static_cast<uint32_t>(
          reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
    }

    const HorseDimensions dims = makeHorseDimensions(horse_seed);
    HorseProfile mount_profile{};
    mount_profile.dims = dims;
    const HorseMountFrame mount = compute_mount_frame(mount_profile);

    const float saddle_height = mount.seat_position.y();
    const float offset_y = saddle_height - pose.pelvisPos.y();

    pose.pelvisPos.setY(pose.pelvisPos.y() + offset_y);
    pose.headPos.setY(pose.headPos.y() + offset_y);
    pose.neck_base.setY(pose.neck_base.y() + offset_y);
    pose.shoulderL.setY(pose.shoulderL.y() + offset_y);
    pose.shoulderR.setY(pose.shoulderR.y() + offset_y);

    float const speed_norm = anim_ctx.locomotion_normalized_speed();
    float const speed_lean = std::clamp(
        anim_ctx.locomotion_speed() * 0.10F + speed_norm * 0.05F, 0.0F, 0.22F);
    const float lean_forward = dims.seatForwardOffset * 0.08F + speed_lean;
    pose.shoulderL.setZ(pose.shoulderL.z() + lean_forward);
    pose.shoulderR.setZ(pose.shoulderR.z() + lean_forward);

    pose.footYOffset = 0.0F;
    pose.footL = mount.stirrup_bottom_left;
    pose.foot_r = mount.stirrup_bottom_right;

    const float knee_y =
        mount.stirrup_bottom_left.y() +
        (saddle_height - mount.stirrup_bottom_left.y()) * 0.62F;
    const float knee_z = mount.stirrup_bottom_left.z() * 0.60F + 0.06F;

    QVector3D knee_left = mount.stirrup_attach_left;
    knee_left.setY(knee_y);
    knee_left.setZ(knee_z);
    pose.knee_l = knee_left;

    QVector3D knee_right = mount.stirrup_attach_right;
    knee_right.setY(knee_y);
    knee_right.setZ(knee_z);
    pose.knee_r = knee_right;

    float const shoulder_height = pose.shoulderL.y();
    float const rein_extension = std::clamp(
        speed_norm * 0.14F + anim_ctx.locomotion_speed() * 0.015F, 0.0F, 0.12F);
    float const rein_drop = std::clamp(
        speed_norm * 0.06F + anim_ctx.locomotion_speed() * 0.008F, 0.0F, 0.04F);

    QVector3D forward = anim_ctx.heading_forward();
    QVector3D right = anim_ctx.heading_right();
    QVector3D up = anim_ctx.heading_up();
    float const rein_spread =
        std::abs(mount.rein_attach_right.x() - mount.rein_attach_left.x()) *
        0.5F;

    QVector3D rest_hand_r = mount.rein_attach_right;
    rest_hand_r += forward * (0.08F + rein_extension);
    rest_hand_r -= right * (0.10F - arm_asymmetry * 0.05F);
    rest_hand_r += up * (0.05F + arm_height_jitter * 0.6F - rein_drop);

    QVector3D rest_hand_l = mount.rein_attach_left;
    rest_hand_l += forward * (0.05F + rein_extension * 0.6F);
    rest_hand_l += right * (0.08F + arm_asymmetry * 0.04F);
    rest_hand_l += up * (0.04F - arm_height_jitter * 0.5F - rein_drop * 0.6F);

    float const rein_forward = rest_hand_r.z();

    HumanoidPoseController controller(pose, anim_ctx);

    controller.placeHandAt(false, rest_hand_r);
    controller.placeHandAt(true, rest_hand_l);

    pose.elbowL =
        QVector3D(pose.shoulderL.x() * 0.4F + rest_hand_l.x() * 0.6F,
                  (pose.shoulderL.y() + rest_hand_l.y()) * 0.5F - 0.08F,
                  (pose.shoulderL.z() + rest_hand_l.z()) * 0.5F);
    pose.elbowR =
        QVector3D(pose.shoulderR.x() * 0.4F + rest_hand_r.x() * 0.6F,
                  (pose.shoulderR.y() + rest_hand_r.y()) * 0.5F - 0.08F,
                  (pose.shoulderR.z() + rest_hand_r.z()) * 0.5F);

    if (anim.is_attacking && anim.isMelee) {
      float const attack_phase =
          std::fmod(anim.time * MOUNTED_KNIGHT_INV_ATTACK_CYCLE_TIME, 1.0F);

      QVector3D const rest_pos = rest_hand_r;
      QVector3D const windup_pos =
          QVector3D(rest_hand_r.x() + 0.32F, shoulder_height + 0.15F,
                    rein_forward - 0.35F);
      QVector3D const raised_pos = QVector3D(
          rein_spread + 0.38F, shoulder_height + 0.28F, rein_forward - 0.25F);
      QVector3D const slash_pos = QVector3D(
          -rein_spread * 0.65F, shoulder_height - 0.08F, rein_forward + 0.85F);
      QVector3D const follow_through = QVector3D(
          -rein_spread * 0.85F, shoulder_height - 0.15F, rein_forward + 0.60F);
      QVector3D const recover_pos = QVector3D(
          rein_spread * 0.45F, shoulder_height - 0.05F, rein_forward + 0.25F);

      QVector3D hand_r_target;

      if (attack_phase < 0.18F) {
        float const t = easeInOutCubic(attack_phase / 0.18F);
        hand_r_target = rest_pos * (1.0F - t) + windup_pos * t;
      } else if (attack_phase < 0.30F) {
        float const t = easeInOutCubic((attack_phase - 0.18F) / 0.12F);
        hand_r_target = windup_pos * (1.0F - t) + raised_pos * t;
      } else if (attack_phase < 0.48F) {
        float t = (attack_phase - 0.30F) / 0.18F;
        t = t * t * t;
        hand_r_target = raised_pos * (1.0F - t) + slash_pos * t;
      } else if (attack_phase < 0.62F) {
        float const t = easeInOutCubic((attack_phase - 0.48F) / 0.14F);
        hand_r_target = slash_pos * (1.0F - t) + follow_through * t;
      } else if (attack_phase < 0.80F) {
        float const t = easeInOutCubic((attack_phase - 0.62F) / 0.18F);
        hand_r_target = follow_through * (1.0F - t) + recover_pos * t;
      } else {
        float const t = smoothstep(0.80F, 1.0F, attack_phase);
        hand_r_target = recover_pos * (1.0F - t) + rest_pos * t;
      }

      float const rein_tension = clamp01((attack_phase - 0.10F) * 2.2F);
      QVector3D const hand_l_target =
          rest_hand_l +
          QVector3D(0.0F, -0.015F * rein_tension, 0.10F * rein_tension);

      controller.placeHandAt(false, hand_r_target);
      controller.placeHandAt(true, hand_l_target);

      pose.elbowR =
          QVector3D(pose.shoulderR.x() * 0.3F + pose.hand_r.x() * 0.7F,
                    (pose.shoulderR.y() + pose.hand_r.y()) * 0.5F - 0.12F,
                    (pose.shoulderR.z() + pose.hand_r.z()) * 0.5F);
      pose.elbowL =
          QVector3D(pose.shoulderL.x() * 0.4F + pose.handL.x() * 0.6F,
                    (pose.shoulderL.y() + pose.handL.y()) * 0.5F - 0.08F,
                    (pose.shoulderL.z() + pose.handL.z()) * 0.5F);
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
      extras = computeMountedKnightExtras(horse_seed, v);
      m_extrasCache[horse_seed] = extras;

      if (m_extrasCache.size() > MAX_EXTRAS_CACHE_SIZE) {
        m_extrasCache.clear();
      }
    }

    m_horseRenderer.render(ctx, anim, anim_ctx, extras.horseProfile, out);

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

  void drawHelmet(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose, ISubmitter &out) const override {

    auto &registry = EquipmentRegistry::instance();
    auto helmet = registry.get(EquipmentCategory::Helmet, "roman_heavy");
    if (helmet) {
      HumanoidAnimationContext anim_ctx{};
      helmet->render(ctx, pose.bodyFrames, v.palette, anim_ctx, out);
    }
  }

  void drawArmor(const DrawContext &ctx, const HumanoidVariant &v,
                 const HumanoidPose &pose, const HumanoidAnimationContext &anim,
                 ISubmitter &out) const override {
    auto &registry = EquipmentRegistry::instance();
    auto armor = registry.get(EquipmentCategory::Armor, "roman_heavy_armor");
    if (armor) {
      armor->render(ctx, pose.bodyFrames, v.palette, anim, out);
    }
  }

private:
  static auto
  computeMountedKnightExtras(uint32_t seed,
                             const HumanoidVariant &v) -> MountedKnightExtras {
    MountedKnightExtras e;

    e.metalColor = QVector3D(0.72F, 0.73F, 0.78F);

    e.horseProfile = makeHorseProfile(seed, v.palette.leather, v.palette.cloth);

    e.swordLength = 0.82F + (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.12F;
    e.swordWidth = 0.042F + (hash_01(seed ^ 0x7777U) - 0.5F) * 0.008F;

    e.hasSword = (hash_01(seed ^ 0xFACEU) > 0.15F);
    e.hasCavalryShield = (hash_01(seed ^ 0xCAFEU) > 0.60F);

    return e;
  }
};
void registerMountedKnightRenderer(
    Render::GL::EntityRendererRegistry &registry) {
  static MountedKnightRenderer const renderer;
  registry.registerRenderer(
      "troops/roman/horse_swordsman",
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

} // namespace Render::GL::Roman
