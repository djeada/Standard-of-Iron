#include "horse_swordsman_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/shader.h"
#include "../../../humanoid/humanoid_math.h"
#include "../../../humanoid/humanoid_specs.h"
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

#include "../../../equipment/equipment_registry.h"
#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Render::GL::Kingdom {

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

      if (attack_phase < 0.18F) {

        float const t = easeInOutCubic(attack_phase / 0.18F);
        pose.hand_r = rest_pos * (1.0F - t) + windup_pos * t;
      } else if (attack_phase < 0.30F) {

        float const t = easeInOutCubic((attack_phase - 0.18F) / 0.12F);
        pose.hand_r = windup_pos * (1.0F - t) + raised_pos * t;
      } else if (attack_phase < 0.48F) {

        float t = (attack_phase - 0.30F) / 0.18F;
        t = t * t * t;
        pose.hand_r = raised_pos * (1.0F - t) + slash_pos * t;
      } else if (attack_phase < 0.62F) {

        float const t = easeInOutCubic((attack_phase - 0.48F) / 0.14F);
        pose.hand_r = slash_pos * (1.0F - t) + follow_through * t;
      } else if (attack_phase < 0.80F) {

        float const t = easeInOutCubic((attack_phase - 0.62F) / 0.18F);
        pose.hand_r = follow_through * (1.0F - t) + recover_pos * t;
      } else {

        float const t = smoothstep(0.80F, 1.0F, attack_phase);
        pose.hand_r = recover_pos * (1.0F - t) + rest_pos * t;
      }

      float const rein_tension = clamp01((attack_phase - 0.10F) * 2.2F);
      pose.handL = rest_hand_l + QVector3D(0.0F, -0.015F * rein_tension,
                                           0.10F * rein_tension);

      pose.elbowR =
          QVector3D(pose.shoulderR.x() * 0.3F + pose.hand_r.x() * 0.7F,
                    (pose.shoulderR.y() + pose.hand_r.y()) * 0.5F - 0.12F,
                    (pose.shoulderR.z() + pose.hand_r.z()) * 0.5F);
      pose.elbowL =
          QVector3D(pose.shoulderL.x() * 0.4F + pose.handL.x() * 0.6F,
                    (pose.shoulderL.y() + pose.handL.y()) * 0.5F - 0.08F,
                    (pose.shoulderL.z() + pose.handL.z()) * 0.5F);
    } else {
      pose.hand_r = rest_hand_r;
      pose.handL = rest_hand_l;
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
    float attack_phase = 0.0F;
    if (is_attacking) {
      attack_phase =
          std::fmod(anim.time * MOUNTED_KNIGHT_INV_ATTACK_CYCLE_TIME, 1.0F);
    }

    if (extras.hasSword) {
      drawSword(ctx, pose, v, extras, is_attacking, attack_phase, out);
    }

    if (extras.hasCavalryShield) {
      drawCavalryShield(ctx, pose, v, extras, out);
    }
  }

  void drawHelmet(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose, ISubmitter &out) const override {

    auto &registry = EquipmentRegistry::instance();
    auto helmet = registry.get(EquipmentCategory::Helmet, "kingdom_heavy");
    if (helmet) {
      HumanoidAnimationContext anim_ctx{};
      helmet->render(ctx, pose.bodyFrames, v.palette, anim_ctx, out);
    }
  }

  void drawArmor(const DrawContext &ctx, const HumanoidVariant &v,
                 const HumanoidPose &pose,
                 const HumanoidAnimationContext &anim,
                 ISubmitter &out) const override {
    auto &registry = EquipmentRegistry::instance();
    auto armor = registry.get(EquipmentCategory::Armor, "kingdom_heavy_armor");
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

  static void drawSword(const DrawContext &ctx, const HumanoidPose &pose,
                        const HumanoidVariant &v,
                        const MountedKnightExtras &extras, bool is_attacking,
                        float attack_phase, ISubmitter &out) {

    const QVector3D grip_pos = pose.hand_r;

    QVector3D sword_dir(0.0F, 0.15F, 1.0F);
    sword_dir.normalize();

    QVector3D const world_up(0.0F, 1.0F, 0.0F);
    QVector3D right_axis = QVector3D::crossProduct(world_up, sword_dir);
    if (right_axis.lengthSquared() < 1e-6F) {
      right_axis = QVector3D(1.0F, 0.0F, 0.0F);
    }
    right_axis.normalize();
    QVector3D up_axis = QVector3D::crossProduct(sword_dir, right_axis);
    up_axis.normalize();

    const QVector3D steel = extras.metalColor;
    const QVector3D steel_hi = steel * 1.18F;
    const QVector3D steel_lo = steel * 0.92F;
    const QVector3D leather = v.palette.leather;
    const QVector3D pommel_col =
        v.palette.metal * QVector3D(1.25F, 1.10F, 0.75F);

    const float pommel_offset = 0.10F;
    const float grip_len = 0.16F;
    const float grip_rad = 0.017F;
    const float guard_half = 0.11F;
    const float guard_rad = 0.012F;
    const float guard_curve = 0.03F;

    const QVector3D pommel_pos = grip_pos - sword_dir * pommel_offset;
    out.mesh(getUnitSphere(), sphereAt(ctx.model, pommel_pos, 0.028F),
             pommel_col, nullptr, 1.0F);

    {
      QVector3D const neck_a = pommel_pos + sword_dir * 0.010F;
      QVector3D const neck_b = grip_pos - sword_dir * 0.005F;
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, neck_a, neck_b, 0.0125F), steel_lo,
               nullptr, 1.0F);

      QVector3D const peen = pommel_pos - sword_dir * 0.012F;
      out.mesh(getUnitCone(), coneFromTo(ctx.model, peen, pommel_pos, 0.010F),
               steel, nullptr, 1.0F);
    }

    const QVector3D grip_a = grip_pos - sword_dir * 0.005F;
    const QVector3D grip_b = grip_pos + sword_dir * (grip_len - 0.005F);
    const int wrap_rings = 5;
    for (int i = 0; i < wrap_rings; ++i) {
      float const t0 = (float)i / wrap_rings;
      float const t1 = (float)(i + 1) / wrap_rings;
      QVector3D const a = grip_a + sword_dir * (grip_len * t0);
      QVector3D const b = grip_a + sword_dir * (grip_len * t1);

      float const r_mid =
          grip_rad *
          (0.96F + 0.08F * std::sin((t0 + t1) * std::numbers::pi_v<float>));
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, a, b, r_mid),
               leather * 0.98F, nullptr, 1.0F);
    }

    const QVector3D guard_center = grip_b + sword_dir * 0.010F;
    {
      const int segs = 4;
      QVector3D prev =
          guard_center - right_axis * guard_half + (-up_axis * guard_curve);
      for (int s = 1; s <= segs; ++s) {
        float const u = -1.0F + 2.0F * (float)s / segs;
        QVector3D const p = guard_center + right_axis * (guard_half * u) +
                            (-up_axis * guard_curve * (1.0F - u * u));
        out.mesh(getUnitCylinder(),
                 cylinderBetween(ctx.model, prev, p, guard_rad), steel_hi,
                 nullptr, 1.0F);
        prev = p;
      }

      QVector3D const lend =
          guard_center - right_axis * guard_half + (-up_axis * guard_curve);
      QVector3D const rend =
          guard_center + right_axis * guard_half + (-up_axis * guard_curve);
      out.mesh(getUnitCone(),
               coneFromTo(ctx.model, lend - right_axis * 0.030F, lend,
                          guard_rad * 1.12F),
               steel_hi, nullptr, 1.0F);
      out.mesh(getUnitCone(),
               coneFromTo(ctx.model, rend + right_axis * 0.030F, rend,
                          guard_rad * 1.12F),
               steel_hi, nullptr, 1.0F);

      out.mesh(getUnitSphere(),
               sphereAt(ctx.model, guard_center, guard_rad * 0.9F), steel,
               nullptr, 1.0F);
    }

    const float blade_len = std::max(0.0F, extras.swordLength - 0.14F);
    const QVector3D blade_root = guard_center + sword_dir * 0.020F;
    const QVector3D blade_tip = blade_root + sword_dir * blade_len;

    const QVector3D ricasso_end = blade_root + sword_dir * (blade_len * 0.08F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, blade_root, ricasso_end,
                             extras.swordWidth * 0.32F),
             steel_hi, nullptr, 1.0F);

    const QVector3D fuller_a = blade_root + sword_dir * (blade_len * 0.10F);
    const QVector3D fuller_b = blade_root + sword_dir * (blade_len * 0.80F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, fuller_a, fuller_b,
                             extras.swordWidth * 0.10F),
             steel_lo, nullptr, 1.0F);

    const float base_r = extras.swordWidth * 0.26F;
    const float mid_r = extras.swordWidth * 0.16F;
    const float pre_tip_r = extras.swordWidth * 0.09F;

    QVector3D const s0 = ricasso_end;
    QVector3D const s1 = blade_root + sword_dir * (blade_len * 0.55F);
    QVector3D const s2 = blade_root + sword_dir * (blade_len * 0.88F);

    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, s0, s1, base_r),
             steel_hi, nullptr, 1.0F);
    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, s1, s2, mid_r),
             steel_hi, nullptr, 1.0F);

    {
      float const edge_r = extras.swordWidth * 0.03F;
      QVector3D const e_a = blade_root + sword_dir * (blade_len * 0.10F);
      QVector3D const e_b = blade_tip - sword_dir * (blade_len * 0.06F);
      QVector3D const left_edge_a = e_a + right_axis * (base_r * 0.95F);
      QVector3D const left_edge_b = e_b + right_axis * (pre_tip_r * 0.95F);
      QVector3D const right_edge_a = e_a - right_axis * (base_r * 0.95F);
      QVector3D const right_edge_b = e_b - right_axis * (pre_tip_r * 0.95F);
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, left_edge_a, left_edge_b, edge_r),
               steel * 1.08F, nullptr, 1.0F);
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, right_edge_a, right_edge_b, edge_r),
               steel * 1.08F, nullptr, 1.0F);
    }

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, s2, blade_tip - sword_dir * 0.020F,
                             pre_tip_r),
             steel_hi, nullptr, 1.0F);
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, blade_tip, blade_tip - sword_dir * 0.060F,
                        pre_tip_r * 0.95F),
             steel_hi * 1.04F, nullptr, 1.0F);

    {
      QVector3D const shoulder_l0 = blade_root + right_axis * (base_r * 1.05F);
      QVector3D const shoulder_l1 = shoulder_l0 - right_axis * (base_r * 0.45F);
      QVector3D const shoulder_r0 = blade_root - right_axis * (base_r * 1.05F);
      QVector3D const shoulder_r1 = shoulder_r0 + right_axis * (base_r * 0.45F);
      out.mesh(getUnitCone(),
               coneFromTo(ctx.model, shoulder_l1, shoulder_l0, base_r * 0.22F),
               steel, nullptr, 1.0F);
      out.mesh(getUnitCone(),
               coneFromTo(ctx.model, shoulder_r1, shoulder_r0, base_r * 0.22F),
               steel, nullptr, 1.0F);
    }

    if (is_attacking && attack_phase >= 0.28F && attack_phase < 0.58F) {
      float const t = (attack_phase - 0.28F) / 0.30F;
      float const alpha = clamp01(0.40F * (1.0F - t * t));
      QVector3D const sweep = (-right_axis * 0.18F - sword_dir * 0.10F) * t;

      QVector3D const trail_tip = blade_tip + sweep;
      QVector3D const trail_root = blade_root + sweep * 0.6F;

      out.mesh(getUnitCone(),
               coneFromTo(ctx.model, trail_root, trail_tip, base_r * 1.10F),
               steel * 0.90F, nullptr, alpha);
      out.mesh(getUnitCone(),
               coneFromTo(ctx.model, trail_root + up_axis * 0.01F, trail_tip,
                          base_r * 0.75F),
               steel * 0.80F, nullptr, alpha * 0.7F);
    }
  }

  static void drawCavalryShield(const DrawContext &ctx,
                                const HumanoidPose &pose,
                                const HumanoidVariant &v,
                                const MountedKnightExtras &extras,
                                ISubmitter &out) {
    const float scale_factor = 2.0F;
    const float r = 0.15F * scale_factor;

    constexpr float k_horse_shield_yaw_degrees = -70.0F;
    QMatrix4x4 rot;
    rot.rotate(k_horse_shield_yaw_degrees, 0.0F, 1.0F, 0.0F);

    const QVector3D n = rot.map(QVector3D(0.0F, 0.0F, 1.0F));
    const QVector3D axis_x = rot.map(QVector3D(1.0F, 0.0F, 0.0F));
    const QVector3D axis_y = rot.map(QVector3D(0.0F, 1.0F, 0.0F));

    QVector3D const shield_center =
        pose.handL + axis_x * (-r * 0.30F) + axis_y * (-0.05F) + n * (0.05F);

    const float plate_half = 0.0012F;
    const float plate_full = plate_half * 2.0F;

    {
      QMatrix4x4 m = ctx.model;
      m.translate(shield_center + n * plate_half);
      m.rotate(k_horse_shield_yaw_degrees, 0.0F, 1.0F, 0.0F);
      m.scale(r, r, plate_full);
      out.mesh(getUnitCylinder(), m, v.palette.cloth * 1.15F, nullptr, 1.0F);
    }

    {
      QMatrix4x4 m = ctx.model;
      m.translate(shield_center - n * plate_half);
      m.rotate(k_horse_shield_yaw_degrees, 0.0F, 1.0F, 0.0F);
      m.scale(r * 0.985F, r * 0.985F, plate_full);
      out.mesh(getUnitCylinder(), m, v.palette.leather * 0.8F, nullptr, 1.0F);
    }

    {
      QMatrix4x4 m = ctx.model;
      m.translate(shield_center + n * (0.015F * scale_factor));
      m.scale(0.035F * scale_factor);
      out.mesh(getUnitSphere(), m, extras.metalColor, nullptr, 1.0F);
    }

    {
      QVector3D const grip_a = shield_center - axis_x * 0.025F - n * 0.025F;
      QVector3D const grip_b = shield_center + axis_x * 0.025F - n * 0.025F;
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, grip_a, grip_b, 0.008F),
               v.palette.leather, nullptr, 1.0F);
    }
  }
};

void registerMountedKnightRenderer(
    Render::GL::EntityRendererRegistry &registry) {
  static MountedKnightRenderer const renderer;
  registry.registerRenderer(
      "troops/kingdom/horse_swordsman",
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

} // namespace Render::GL::Kingdom
