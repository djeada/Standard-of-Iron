#include "archer_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/render_constants.h"
#include "../../../gl/shader.h"
#include "../../../humanoid/rig.h"
#include "../../../humanoid/style_palette.h"
#include "../../../humanoid/humanoid_math.h"
#include "../../../humanoid/humanoid_specs.h"
#include "../../../palette.h"
#include "../../../scene_renderer.h"
#include "../../../submitter.h"
#include "../../registry.h"
#include "../../renderer_constants.h"
#include "archer_style.h"

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <qmatrix4x4.h>
#include <qstringliteral.h>
#include <qvectornd.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Render::GL::Carthage {

namespace {

constexpr std::string_view k_default_style_key = "default";
constexpr std::string_view k_attachment_headwrap = "carthage_headwrap";

auto style_registry() -> std::unordered_map<std::string, ArcherStyleConfig> & {
  static std::unordered_map<std::string, ArcherStyleConfig> styles;
  return styles;
}

void ensure_archer_styles_registered() {
  static const bool registered = []() {
    register_carthage_archer_style();
    return true;
  }();
  (void)registered;
}

constexpr float k_team_mix_weight = 0.65F;
constexpr float k_style_mix_weight = 0.35F;

} // namespace

void register_archer_style(const std::string &nation_id,
                           const ArcherStyleConfig &style) {
  style_registry()[nation_id] = style;
}

using Render::Geom::clamp01;
using Render::Geom::clampf;
using Render::Geom::coneFromTo;
using Render::Geom::cylinderBetween;
using Render::Geom::sphereAt;
using Render::GL::Humanoid::mix_palette_color;
using Render::GL::Humanoid::saturate_color;

struct ArcherExtras {
  QVector3D stringCol;
  QVector3D fletch;
  QVector3D metalHead;
  float bowRodR = 0.035F;
  float stringR = 0.008F;
  float bowDepth = 0.25F;
  float bowX = 0.0F;
  float bowTopY{};
  float bowBotY{};
};

class ArcherRenderer : public HumanoidRendererBase {
public:
  auto getProportionScaling() const -> QVector3D override {

    return {0.94F, 1.01F, 0.96F};
  }

  void getVariant(const DrawContext &ctx, uint32_t seed,
                  HumanoidVariant &v) const override {
    QVector3D const team_tint = resolveTeamTint(ctx);
    v.palette = makeHumanoidPalette(team_tint, seed);
    auto const &style = resolve_style(ctx);
    apply_palette_overrides(style, team_tint, v);
  }

  void customizePose(const DrawContext &,
                     const HumanoidAnimationContext &anim_ctx, uint32_t seed,
                     HumanoidPose &pose) const override {
    using HP = HumanProportions;

    const AnimationInputs &anim = anim_ctx.inputs;

    float const arm_height_jitter = (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.03F;
    float const arm_asymmetry = (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.04F;

    float const bow_x = 0.0F;

    if (anim.isInHoldMode || anim.isExitingHold) {

      float const t = anim.isInHoldMode ? 1.0F : (1.0F - anim.holdExitProgress);

      float const kneel_depth = 0.45F * t;

      float const pelvis_y = HP::WAIST_Y - kneel_depth;
      pose.pelvisPos.setY(pelvis_y);

      float const stance_narrow = 0.12F;

      float const left_knee_y = HP::GROUND_Y + 0.08F * t;
      float const left_knee_z = -0.05F * t;

      pose.knee_l = QVector3D(-stance_narrow, left_knee_y, left_knee_z);

      pose.footL = QVector3D(-stance_narrow - 0.03F, HP::GROUND_Y,
                             left_knee_z - HP::LOWER_LEG_LEN * 0.95F * t);

      float const right_foot_z = 0.30F * t;
      pose.foot_r = QVector3D(stance_narrow, HP::GROUND_Y + pose.footYOffset,
                              right_foot_z);

      float const right_knee_y = pelvis_y - 0.10F;
      float const right_knee_z = right_foot_z - 0.05F;

      pose.knee_r = QVector3D(stance_narrow, right_knee_y, right_knee_z);

      float const upper_body_drop = kneel_depth;

      pose.shoulderL.setY(HP::SHOULDER_Y - upper_body_drop);
      pose.shoulderR.setY(HP::SHOULDER_Y - upper_body_drop);
      pose.neck_base.setY(HP::NECK_BASE_Y - upper_body_drop);
      pose.headPos.setY((HP::HEAD_TOP_Y + HP::CHIN_Y) * 0.5F - upper_body_drop);

      float const forward_lean = 0.10F * t;
      pose.shoulderL.setZ(pose.shoulderL.z() + forward_lean);
      pose.shoulderR.setZ(pose.shoulderR.z() + forward_lean);
      pose.neck_base.setZ(pose.neck_base.z() + forward_lean * 0.8F);
      pose.headPos.setZ(pose.headPos.z() + forward_lean * 0.7F);

      QVector3D const hold_hand_l(bow_x - 0.15F, pose.shoulderL.y() + 0.30F,
                                  0.55F);
      QVector3D const hold_hand_r(bow_x + 0.12F, pose.shoulderR.y() + 0.15F,
                                  0.10F);
      QVector3D const normal_hand_l(bow_x - 0.05F + arm_asymmetry,
                                    HP::SHOULDER_Y + 0.05F + arm_height_jitter,
                                    0.55F);
      QVector3D const normal_hand_r(
          0.15F - arm_asymmetry * 0.5F,
          HP::SHOULDER_Y + 0.15F + arm_height_jitter * 0.8F, 0.20F);

      pose.handL = normal_hand_l * (1.0F - t) + hold_hand_l * t;
      pose.hand_r = normal_hand_r * (1.0F - t) + hold_hand_r * t;
    } else {
      pose.handL = QVector3D(bow_x - 0.05F + arm_asymmetry,
                             HP::SHOULDER_Y + 0.05F + arm_height_jitter, 0.55F);
      pose.hand_r =
          QVector3D(0.15F - arm_asymmetry * 0.5F,
                    HP::SHOULDER_Y + 0.15F + arm_height_jitter * 0.8F, 0.20F);
    }

    if (anim.is_attacking && !anim.isInHoldMode) {
      float const attack_phase =
          std::fmod(anim.time * ARCHER_INV_ATTACK_CYCLE_TIME, 1.0F);

      if (anim.isMelee) {
        QVector3D const rest_pos(0.25F, HP::SHOULDER_Y, 0.10F);
        QVector3D const raised_pos(0.30F, HP::HEAD_TOP_Y + 0.2F, -0.05F);
        QVector3D const strike_pos(0.35F, HP::WAIST_Y, 0.45F);

        if (attack_phase < 0.25F) {
          float t = attack_phase / 0.25F;
          t = t * t;
          pose.hand_r = rest_pos * (1.0F - t) + raised_pos * t;
          pose.handL = QVector3D(-0.15F, HP::SHOULDER_Y - 0.1F * t, 0.20F);
        } else if (attack_phase < 0.35F) {
          pose.hand_r = raised_pos;
          pose.handL = QVector3D(-0.15F, HP::SHOULDER_Y - 0.1F, 0.20F);
        } else if (attack_phase < 0.55F) {
          float t = (attack_phase - 0.35F) / 0.2F;
          t = t * t * t;
          pose.hand_r = raised_pos * (1.0F - t) + strike_pos * t;
          pose.handL =
              QVector3D(-0.15F, HP::SHOULDER_Y - 0.1F * (1.0F - t * 0.5F),
                        0.20F + 0.15F * t);
        } else {
          float t = (attack_phase - 0.55F) / 0.45F;
          t = 1.0F - (1.0F - t) * (1.0F - t);
          pose.hand_r = strike_pos * (1.0F - t) + rest_pos * t;
          pose.handL = QVector3D(-0.15F, HP::SHOULDER_Y - 0.05F * (1.0F - t),
                                 0.35F * (1.0F - t) + 0.20F * t);
        }
      } else {
        QVector3D const aim_pos(0.18F, HP::SHOULDER_Y + 0.18F, 0.35F);
        QVector3D const draw_pos(0.22F, HP::SHOULDER_Y + 0.10F, -0.30F);
        QVector3D const release_pos(0.18F, HP::SHOULDER_Y + 0.20F, 0.10F);

        if (attack_phase < 0.20F) {
          float t = attack_phase / 0.20F;
          t = t * t;
          pose.hand_r = aim_pos * (1.0F - t) + draw_pos * t;
          pose.handL = QVector3D(bow_x - 0.05F, HP::SHOULDER_Y + 0.05F, 0.55F);

          float const shoulder_twist = t * 0.08F;
          pose.shoulderR.setY(pose.shoulderR.y() + shoulder_twist);
          pose.shoulderL.setY(pose.shoulderL.y() - shoulder_twist * 0.5F);
        } else if (attack_phase < 0.50F) {
          pose.hand_r = draw_pos;
          pose.handL = QVector3D(bow_x - 0.05F, HP::SHOULDER_Y + 0.05F, 0.55F);

          float const shoulder_twist = 0.08F;
          pose.shoulderR.setY(pose.shoulderR.y() + shoulder_twist);
          pose.shoulderL.setY(pose.shoulderL.y() - shoulder_twist * 0.5F);
        } else if (attack_phase < 0.58F) {
          float t = (attack_phase - 0.50F) / 0.08F;
          t = t * t * t;
          pose.hand_r = draw_pos * (1.0F - t) + release_pos * t;
          pose.handL = QVector3D(bow_x - 0.05F, HP::SHOULDER_Y + 0.05F, 0.55F);

          float const shoulder_twist = 0.08F * (1.0F - t * 0.6F);
          pose.shoulderR.setY(pose.shoulderR.y() + shoulder_twist);
          pose.shoulderL.setY(pose.shoulderL.y() - shoulder_twist * 0.5F);

          pose.headPos.setZ(pose.headPos.z() - t * 0.04F);
        } else {
          float t = (attack_phase - 0.58F) / 0.42F;
          t = 1.0F - (1.0F - t) * (1.0F - t);
          pose.hand_r = release_pos * (1.0F - t) + aim_pos * t;
          pose.handL = QVector3D(bow_x - 0.05F, HP::SHOULDER_Y + 0.05F, 0.55F);

          float const shoulder_twist = 0.08F * 0.4F * (1.0F - t);
          pose.shoulderR.setY(pose.shoulderR.y() + shoulder_twist);
          pose.shoulderL.setY(pose.shoulderL.y() - shoulder_twist * 0.5F);

          pose.headPos.setZ(pose.headPos.z() - 0.04F * (1.0F - t));
        }
      }
    }

    QVector3D right_axis = pose.shoulderR - pose.shoulderL;
    right_axis.setY(0.0F);
    if (right_axis.lengthSquared() < 1e-8F) {
      right_axis = QVector3D(1, 0, 0);
    }
    right_axis.normalize();
    QVector3D const outward_l = -right_axis;
    QVector3D const outward_r = right_axis;

    pose.elbowL = elbowBendTorso(pose.shoulderL, pose.handL, outward_l, 0.45F,
                                 0.15F, -0.08F, +1.0F);
    pose.elbowR = elbowBendTorso(pose.shoulderR, pose.hand_r, outward_r, 0.48F,
                                 0.12F, 0.02F, +1.0F);
  }

  void addAttachments(const DrawContext &ctx, const HumanoidVariant &v,
                      const HumanoidPose &pose,
                      const HumanoidAnimationContext &anim_ctx,
                      ISubmitter &out) const override {
    using HP = HumanProportions;

    auto const &style = resolve_style(ctx);
    const AnimationInputs &anim = anim_ctx.inputs;
    QVector3D team_tint = resolveTeamTint(ctx);
    uint32_t seed = 0U;
    if (ctx.entity != nullptr) {
      auto *unit = ctx.entity->getComponent<Engine::Core::UnitComponent>();
      if (unit != nullptr) {
        seed ^= uint32_t(unit->owner_id * 2654435761U);
      }
      seed ^= uint32_t(reinterpret_cast<uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
    }

    ArcherExtras extras;
    auto it = m_extrasCache.find(seed);
    if (it != m_extrasCache.end()) {
      extras = it->second;
    } else {
      extras.metalHead = Render::Geom::clampVec01(v.palette.metal * 1.15F);
      extras.stringCol = QVector3D(0.30F, 0.30F, 0.32F);
      auto tint = [&](float k) {
        return QVector3D(clamp01(team_tint.x() * k), clamp01(team_tint.y() * k),
                         clamp01(team_tint.z() * k));
      };
      extras.fletch = tint(0.9F);
      extras.bowTopY = HP::SHOULDER_Y + 0.55F;
      extras.bowBotY = HP::WAIST_Y - 0.25F;

      apply_extras_overrides(style, extras);
      m_extrasCache[seed] = extras;

      if (m_extrasCache.size() > MAX_EXTRAS_CACHE_SIZE) {
        m_extrasCache.clear();
      }
    }
    apply_extras_overrides(style, extras);

    drawQuiver(ctx, v, pose, extras, seed, out);

    float attack_phase = 0.0F;
    if (anim.is_attacking && !anim.isMelee) {
      attack_phase = std::fmod(anim.time * ARCHER_INV_ATTACK_CYCLE_TIME, 1.0F);
    }
    drawBowAndArrow(ctx, pose, v, extras, anim.is_attacking && !anim.isMelee,
                    attack_phase, out);
  }

  void drawHelmet(const DrawContext &ctx, const HumanoidVariant &v,
                  const HumanoidPose &pose, ISubmitter &out) const override {
    using HP = HumanProportions;

    auto const &style = resolve_style(ctx);
    if (!style.show_helmet) {
      if (style.attachment_profile == std::string(k_attachment_headwrap)) {
        draw_headwrap(ctx, v, pose, out);
      }
      return;
    }

    auto draw_montefortino = [&](const QVector3D &base_color) {
      QVector3D bronze =
          saturate_color(base_color * QVector3D(1.14F, 1.00F, 0.76F));
      QVector3D tinned_highlight =
          saturate_color(base_color * QVector3D(1.38F, 1.36F, 1.44F));
      QVector3D patina =
          saturate_color(base_color * QVector3D(0.92F, 1.05F, 1.04F));
      QVector3D leather_band =
          saturate_color(v.palette.leather * QVector3D(1.12F, 0.98F, 0.84F));

      float const head_r = pose.headR;
      QVector3D const bowl_center(0.0F, pose.headPos.y() + head_r * 0.72F,
                                  0.0F);

      QMatrix4x4 bowl = ctx.model;
      bowl.translate(bowl_center);
      bowl.scale(head_r * 1.08F, head_r * 1.24F, head_r * 1.02F);
      bowl.rotate(-5.0F, 1.0F, 0.0F, 0.0F);
      out.mesh(getUnitSphere(), bowl, bronze, nullptr, 1.0F);

      QMatrix4x4 ridge = ctx.model;
      ridge.translate(QVector3D(0.0F, pose.headPos.y() + head_r * 1.00F, 0.0F));
      ridge.scale(head_r * 0.20F, head_r * 0.48F, head_r * 0.20F);
      out.mesh(getUnitCone(), ridge, patina, nullptr, 1.0F);

      QMatrix4x4 knob = ctx.model;
      knob.translate(QVector3D(0.0F, pose.headPos.y() + head_r * 1.38F, 0.0F));
      knob.scale(head_r * 0.22F, head_r * 0.32F, head_r * 0.22F);
      out.mesh(getUnitSphere(), knob, tinned_highlight, nullptr, 1.0F);

      QVector3D const brow_top(0.0F, pose.headPos.y() + head_r * 0.58F, 0.0F);
      QVector3D const brow_bottom(0.0F, pose.headPos.y() + head_r * 0.46F,
                                  0.0F);
      QMatrix4x4 brow =
          cylinderBetween(ctx.model, brow_bottom, brow_top, head_r * 1.20F);
      brow.scale(1.04F, 1.0F, 0.86F);
      out.mesh(getUnitCylinder(), brow, leather_band, nullptr, 1.0F);

      QVector3D const rim_upper(0.0F, pose.headPos.y() + head_r * 0.44F, 0.0F);
      QVector3D const rim_lower(0.0F, pose.headPos.y() + head_r * 0.34F, 0.0F);
      QMatrix4x4 rim =
          cylinderBetween(ctx.model, rim_lower, rim_upper, head_r * 1.30F);
      rim.scale(1.06F, 1.0F, 0.90F);
      out.mesh(getUnitCylinder(), rim, bronze * QVector3D(0.94F, 0.92F, 0.88F),
               nullptr, 1.0F);

      QVector3D const neck_base(0.0F, pose.headPos.y() + head_r * 0.32F,
                                -head_r * 0.68F);
      QVector3D const neck_drop(0.0F, pose.headPos.y() - head_r * 0.48F,
                                -head_r * 0.96F);
      QMatrix4x4 neck =
          coneFromTo(ctx.model, neck_drop, neck_base, head_r * 1.34F);
      neck.scale(1.0F, 1.0F, 0.94F);
      out.mesh(getUnitCone(), neck, bronze * 0.96F, nullptr, 1.0F);

      auto cheek_plate = [&](float sign) {
        QVector3D const hinge(sign * head_r * 0.80F,
                              pose.headPos.y() + head_r * 0.38F,
                              head_r * 0.38F);
        QVector3D const lobe =
            hinge + QVector3D(sign * head_r * 0.20F, -head_r * 0.82F, 0.02F);
        QMatrix4x4 cheek = coneFromTo(ctx.model, lobe, hinge, head_r * 0.46F);
        cheek.scale(0.60F, 1.0F, 0.42F);
        out.mesh(getUnitCone(), cheek, patina, nullptr, 1.0F);
      };
      cheek_plate(+1.0F);
      cheek_plate(-1.0F);

      QVector3D const crest_front(0.0F, pose.headPos.y() + head_r * 0.96F,
                                  head_r * 0.82F);
      QVector3D const crest_back(0.0F, pose.headPos.y() + head_r * 0.96F,
                                 -head_r * 0.90F);
      QMatrix4x4 crest =
          cylinderBetween(ctx.model, crest_back, crest_front, head_r * 0.14F);
      crest.scale(0.54F, 1.0F, 1.0F);
      out.mesh(getUnitCylinder(), crest,
               tinned_highlight * QVector3D(0.94F, 0.96F, 1.02F), nullptr,
               1.0F);
    };

    draw_montefortino(v.palette.metal);
  }

  void draw_armorOverlay(const DrawContext &ctx, const HumanoidVariant &v,
                         const HumanoidPose &pose, float y_top_cover,
                         float torso_r, float, float, const QVector3D &,
                         ISubmitter &out) const override {
    using HP = HumanProportions;

    auto const &style = resolve_style(ctx);
    if (!style.show_armor) {
      return;
    }

    float const waist_y = pose.pelvisPos.y();
    float const cuirass_top = y_top_cover + 0.02F;
    float const cuirass_bottom = waist_y - 0.10F;

    QVector3D linen =
        saturate_color(v.palette.cloth * QVector3D(1.12F, 1.04F, 0.88F));
    QVector3D bronze =
        saturate_color(v.palette.metal * QVector3D(1.18F, 1.02F, 0.74F));
    QVector3D bronze_shadow =
        saturate_color(bronze * QVector3D(0.90F, 0.94F, 0.98F));
    QVector3D tinned =
        saturate_color(v.palette.metal * QVector3D(1.36F, 1.36F, 1.42F));
    QVector3D leather =
        saturate_color(v.palette.leatherDark * QVector3D(1.06F, 0.98F, 0.84F));

    QVector3D const tunic_top(0.0F, cuirass_top + 0.04F, 0.0F);
    QVector3D const tunic_bot(0.0F, waist_y + 0.05F, 0.0F);
    QMatrix4x4 tunic =
        cylinderBetween(ctx.model, tunic_bot, tunic_top, torso_r * 0.94F);
    tunic.scale(1.02F, 1.0F, 0.90F);
    out.mesh(getUnitCylinder(), tunic, linen, nullptr, 1.0F);

    constexpr int k_scale_rows = 6;
    constexpr float k_tile_height = 0.085F;
    constexpr float k_tile_width = 0.11F;
    constexpr float k_tile_thickness = 0.020F;
    constexpr float k_row_overlap = 0.032F;
    constexpr float k_radial_push = 0.010F;

    auto draw_scale_tile = [&](const QVector3D &center, float yaw,
                               const QVector3D &color, float height,
                               float width_scale) {
      QVector3D const top = center + QVector3D(0.0F, height * 0.5F, 0.0F);
      QVector3D const bot = center - QVector3D(0.0F, height * 0.5F, 0.0F);
      float const radius = (k_tile_width * width_scale) * 0.5F;
      QMatrix4x4 plate = cylinderBetween(ctx.model, bot, top, radius);
      float const yaw_deg = yaw * (180.0F / std::numbers::pi_v<float>);
      plate.rotate(yaw_deg, 0.0F, 1.0F, 0.0F);
      plate.scale(0.92F, 1.0F,
                  (k_tile_thickness / (k_tile_width * width_scale)));
      out.mesh(getUnitCylinder(18), plate, color, nullptr, 1.0F);

      QVector3D const lip_top = bot + QVector3D(0.0F, height * 0.22F, 0.0F);
      QMatrix4x4 lip = cylinderBetween(ctx.model, bot, lip_top, radius * 0.92F);
      lip.rotate(yaw_deg, 0.0F, 1.0F, 0.0F);
      lip.scale(0.88F, 1.0F,
                (k_tile_thickness / (k_tile_width * width_scale)) * 0.72F);
      out.mesh(getUnitCylinder(16), lip, color * QVector3D(0.90F, 0.92F, 0.96F),
               nullptr, 1.0F);
    };

    auto emit_scale_band = [&](float center_angle, float span, int columns,
                               float radius_scale) {
      float const start = center_angle - span * 0.5F;
      float const step = columns > 1 ? span / float(columns - 1) : 0.0F;

      for (int row = 0; row < k_scale_rows; ++row) {
        float const row_y = cuirass_top - row * (k_tile_height - k_row_overlap);
        float const layer_radius =
            torso_r * radius_scale + row * (k_radial_push * radius_scale);
        for (int col = 0; col < columns; ++col) {
          float const angle = start + step * col;
          QVector3D const radial(std::sin(angle), 0.0F, std::cos(angle));
          QVector3D center(radial.x() * layer_radius, row_y,
                           radial.z() * layer_radius);
          center += radial * (row * 0.006F);
          float const yaw = std::atan2(radial.x(), radial.z());
          bool const tinned_tile = ((row + col) % 4) == 0;
          QVector3D color =
              tinned_tile ? tinned : (row % 2 == 0 ? bronze : bronze_shadow);
          float const width_scale =
              1.0F - 0.04F * std::abs((columns - 1) * 0.5F - col);
          draw_scale_tile(center, yaw, color, k_tile_height,
                          std::clamp(width_scale, 0.78F, 1.05F));
        }
      }
    };

    emit_scale_band(0.0F, 1.30F, 7, 1.08F);
    emit_scale_band(std::numbers::pi_v<float>, 1.20F, 7, 1.04F);
    emit_scale_band(std::numbers::pi_v<float> * 0.5F, 0.82F, 5, 1.02F);
    emit_scale_band(-std::numbers::pi_v<float> * 0.5F, 0.82F, 5, 1.02F);

    QVector3D const collar_top(0.0F, cuirass_top + 0.020F, 0.0F);
    QVector3D const collar_bot(0.0F, cuirass_top - 0.010F, 0.0F);
    QMatrix4x4 collar = cylinderBetween(ctx.model, collar_bot, collar_top,
                                        HP::NECK_RADIUS * 1.90F);
    collar.scale(1.04F, 1.0F, 0.90F);
    out.mesh(getUnitCylinder(), collar, leather, nullptr, 1.0F);

    QVector3D const waist_top(0.0F, waist_y + 0.05F, 0.0F);
    QVector3D const waist_bot(0.0F, waist_y - 0.02F, 0.0F);
    QMatrix4x4 waist =
        cylinderBetween(ctx.model, waist_bot, waist_top, torso_r * 1.16F);
    waist.scale(1.06F, 1.0F, 0.90F);
    out.mesh(getUnitCylinder(), waist, leather, nullptr, 1.0F);

    auto draw_pteruge = [&](float angle, float length) {
      QVector3D const radial(std::sin(angle), 0.0F, std::cos(angle));
      QVector3D const base =
          QVector3D(radial.x() * torso_r * 1.18F, waist_y - 0.03F,
                    radial.z() * torso_r * 1.18F);
      QVector3D const tip =
          base + QVector3D(radial.x() * 0.02F, -length, radial.z() * 0.02F);
      QMatrix4x4 strap =
          cylinderBetween(ctx.model, tip, base, k_tile_width * 0.18F);
      float const yaw = std::atan2(radial.x(), radial.z());
      strap.rotate(yaw * (180.0F / std::numbers::pi_v<float>), 0.0F, 1.0F,
                   0.0F);
      strap.scale(0.65F, 1.0F, 0.40F);
      out.mesh(getUnitCylinder(12), strap,
               leather * QVector3D(0.90F, 0.92F, 0.96F), nullptr, 1.0F);
    };

    constexpr int k_pteruge_count = 12;
    for (int i = 0; i < k_pteruge_count; ++i) {
      float const angle = (static_cast<float>(i) / k_pteruge_count) * 2.0F *
                          std::numbers::pi_v<float>;
      draw_pteruge(angle, 0.18F);
    }
  }

  void drawShoulderDecorations(const DrawContext &ctx, const HumanoidVariant &v,
                               const HumanoidPose &pose, float, float y_neck,
                               const QVector3D &,
                               ISubmitter &out) const override {
    using HP = HumanProportions;

    auto const &style = resolve_style(ctx);
    if (!style.show_shoulder_decor && !style.show_cape) {
      return;
    }

    QVector3D brass_color = v.palette.metal * QVector3D(1.2F, 1.0F, 0.65F);

    auto draw_phalera = [&](const QVector3D &pos) {
      QMatrix4x4 m = ctx.model;
      m.translate(pos);
      m.scale(0.025F);
      out.mesh(getUnitSphere(), m, brass_color, nullptr, 1.0F);
    };

    if (style.show_shoulder_decor) {
      draw_phalera(pose.shoulderL + QVector3D(0, 0.05F, 0.02F));
      draw_phalera(pose.shoulderR + QVector3D(0, 0.05F, 0.02F));
    }

    if (!style.show_cape) {
      return;
    }

    QVector3D const clasp_pos(0, y_neck + 0.02F, 0.08F);
    QMatrix4x4 clasp_m = ctx.model;
    clasp_m.translate(clasp_pos);
    clasp_m.scale(0.020F);
    out.mesh(getUnitSphere(), clasp_m, brass_color * 1.1F, nullptr, 1.0F);

    QVector3D const cape_top = clasp_pos + QVector3D(0, -0.02F, -0.05F);
    QVector3D const cape_bot = clasp_pos + QVector3D(0, -0.25F, -0.15F);
    QVector3D cape_fabric = v.palette.cloth * QVector3D(1.2F, 0.3F, 0.3F);
    if (style.cape_color) {
      cape_fabric = saturate_color(*style.cape_color);
    }

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, cape_top, cape_bot, 0.025F),
             cape_fabric * 0.85F, nullptr, 1.0F);
  }

private:
  mutable std::unordered_map<uint32_t, ArcherExtras> m_extrasCache;

  auto
  resolve_style(const DrawContext &ctx) const -> const ArcherStyleConfig & {
    ensure_archer_styles_registered();
    auto &styles = style_registry();
    std::string nation_id;
    if (ctx.entity != nullptr) {
      if (auto *unit =
              ctx.entity->getComponent<Engine::Core::UnitComponent>()) {
        nation_id = Game::Systems::nationIDToString(unit->nation_id);
      }
    }
    if (!nation_id.empty()) {
      auto it = styles.find(nation_id);
      if (it != styles.end()) {
        return it->second;
      }
    }
    auto fallback = styles.find(std::string(k_default_style_key));
    if (fallback != styles.end()) {
      return fallback->second;
    }
    static const ArcherStyleConfig default_style{};
    return default_style;
  }

public:
  auto resolve_shader_key(const DrawContext &ctx) const -> QString {
    const ArcherStyleConfig &style = resolve_style(ctx);
    if (!style.shader_id.empty()) {
      return QString::fromStdString(style.shader_id);
    }
    return QStringLiteral("archer");
  }

private:
  void apply_palette_overrides(const ArcherStyleConfig &style,
                               const QVector3D &team_tint,
                               HumanoidVariant &variant) const {
    auto apply_color = [&](const std::optional<QVector3D> &override_color,
                           QVector3D &target) {
      target = mix_palette_color(target, override_color, team_tint,
                                 k_team_mix_weight, k_style_mix_weight);
    };

    apply_color(style.cloth_color, variant.palette.cloth);
    apply_color(style.leather_color, variant.palette.leather);
    apply_color(style.leather_dark_color, variant.palette.leatherDark);
    apply_color(style.metal_color, variant.palette.metal);
    apply_color(style.wood_color, variant.palette.wood);
  }

  void apply_extras_overrides(const ArcherStyleConfig &style,
                              ArcherExtras &extras) const {
    if (style.fletching_color) {
      extras.fletch = saturate_color(*style.fletching_color);
    }
    if (style.bow_string_color) {
      extras.stringCol = saturate_color(*style.bow_string_color);
    }
  }

  void draw_headwrap(const DrawContext &ctx, const HumanoidVariant &v,
                     const HumanoidPose &pose, ISubmitter &out) const {
    QVector3D const cloth_color =
        saturate_color(v.palette.cloth * QVector3D(0.9F, 1.05F, 1.05F));
    float const head_r = pose.headR;

    QVector3D const band_top(0, pose.headPos.y() + head_r * 0.70F, 0);
    QVector3D const band_bot(0, pose.headPos.y() + head_r * 0.30F, 0);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, band_bot, band_top, head_r * 1.08F),
             cloth_color, nullptr, 1.0F);

    QVector3D const knot_center(0.10F, pose.headPos.y() + head_r * 0.60F,
                                head_r * 0.72F);
    QMatrix4x4 knot_m = ctx.model;
    knot_m.translate(knot_center);
    knot_m.scale(head_r * 0.32F);
    out.mesh(getUnitSphere(), knot_m, cloth_color * 1.05F, nullptr, 1.0F);

    QVector3D const tail_top = knot_center + QVector3D(-0.08F, -0.05F, -0.06F);
    QVector3D const tail_bot = tail_top + QVector3D(0.02F, -0.28F, -0.08F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, tail_top, tail_bot, head_r * 0.28F),
             cloth_color * QVector3D(0.92F, 0.98F, 1.05F), nullptr, 1.0F);
  }

  static void drawQuiver(const DrawContext &ctx, const HumanoidVariant &v,
                         const HumanoidPose &pose, const ArcherExtras &extras,
                         uint32_t seed, ISubmitter &out) {
    using HP = HumanProportions;

    QVector3D const spine_mid = (pose.shoulderL + pose.shoulderR) * 0.5F;
    QVector3D const quiver_offset(-0.08F, 0.10F, -0.25F);
    QVector3D const q_top = spine_mid + quiver_offset;
    QVector3D const q_base = q_top + QVector3D(-0.02F, -0.30F, 0.03F);

    float const quiver_r = HP::HEAD_RADIUS * 0.45F;
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, q_base, q_top, quiver_r),
             v.palette.leather, nullptr, 1.0F);

    float const j = (hash_01(seed) - 0.5F) * 0.04F;
    float const k =
        (hash_01(seed ^ HashXorShift::k_golden_ratio) - 0.5F) * 0.04F;

    QVector3D const a1 = q_top + QVector3D(0.00F + j, 0.08F, 0.00F + k);
    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, q_top, a1, 0.010F),
             v.palette.wood, nullptr, 1.0F);
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, a1, a1 + QVector3D(0, 0.05F, 0), 0.025F),
             extras.fletch, nullptr, 1.0F);

    QVector3D const a2 = q_top + QVector3D(0.02F - j, 0.07F, 0.02F - k);
    out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, q_top, a2, 0.010F),
             v.palette.wood, nullptr, 1.0F);
    out.mesh(getUnitCone(),
             coneFromTo(ctx.model, a2, a2 + QVector3D(0, 0.05F, 0), 0.025F),
             extras.fletch, nullptr, 1.0F);
  }

  static void drawBowAndArrow(const DrawContext &ctx, const HumanoidPose &pose,
                              const HumanoidVariant &v,
                              const ArcherExtras &extras, bool is_attacking,
                              float attack_phase, ISubmitter &out) {
    const QVector3D up(0.0F, 1.0F, 0.0F);
    const QVector3D forward(0.0F, 0.0F, 1.0F);

    QVector3D const grip = pose.handL;

    float const bow_plane_z = 0.45F;
    QVector3D const top_end(extras.bowX, extras.bowTopY, bow_plane_z);
    QVector3D const bot_end(extras.bowX, extras.bowBotY, bow_plane_z);

    QVector3D const nock(
        extras.bowX,
        clampf(pose.hand_r.y(), extras.bowBotY + 0.05F, extras.bowTopY - 0.05F),
        clampf(pose.hand_r.z(), bow_plane_z - 0.30F, bow_plane_z + 0.30F));

    constexpr int k_bowstring_segments = 22;
    auto q_bezier = [](const QVector3D &a, const QVector3D &c,
                       const QVector3D &b, float t) {
      float const u = 1.0F - t;
      return u * u * a + 2.0F * u * t * c + t * t * b;
    };

    float const bow_mid_y = (top_end.y() + bot_end.y()) * 0.5F;

    float const ctrl_y = bow_mid_y + 0.45F;

    QVector3D const ctrl(extras.bowX, ctrl_y,
                         bow_plane_z + extras.bowDepth * 0.6F);

    QVector3D prev = bot_end;
    for (int i = 1; i <= k_bowstring_segments; ++i) {
      float const t = float(i) / float(k_bowstring_segments);
      QVector3D const cur = q_bezier(bot_end, ctrl, top_end, t);
      out.mesh(getUnitCylinder(),
               cylinderBetween(ctx.model, prev, cur, extras.bowRodR),
               v.palette.wood, nullptr, 1.0F);
      prev = cur;
    }
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, grip - up * 0.05F, grip + up * 0.05F,
                             extras.bowRodR * 1.45F),
             v.palette.wood, nullptr, 1.0F);

    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, top_end, nock, extras.stringR),
             extras.stringCol, nullptr, 1.0F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, nock, bot_end, extras.stringR),
             extras.stringCol, nullptr, 1.0F);
    out.mesh(getUnitCylinder(),
             cylinderBetween(ctx.model, pose.hand_r, nock, 0.0045F),
             extras.stringCol * 0.9F, nullptr, 1.0F);

    bool const show_arrow =
        !is_attacking ||
        (is_attacking && attack_phase >= 0.0F && attack_phase < 0.52F);

    if (show_arrow) {
      QVector3D const tail = nock - forward * 0.06F;
      QVector3D const tip = tail + forward * 0.90F;
      out.mesh(getUnitCylinder(), cylinderBetween(ctx.model, tail, tip, 0.018F),
               v.palette.wood, nullptr, 1.0F);
      QVector3D const head_base = tip - forward * 0.10F;
      out.mesh(getUnitCone(), coneFromTo(ctx.model, head_base, tip, 0.05F),
               extras.metalHead, nullptr, 1.0F);
      QVector3D const f1b = tail - forward * 0.02F;
      QVector3D const f1a = f1b - forward * 0.06F;
      QVector3D const f2b = tail + forward * 0.02F;
      QVector3D const f2a = f2b + forward * 0.06F;
      out.mesh(getUnitCone(), coneFromTo(ctx.model, f1b, f1a, 0.04F),
               extras.fletch, nullptr, 1.0F);
      out.mesh(getUnitCone(), coneFromTo(ctx.model, f2a, f2b, 0.04F),
               extras.fletch, nullptr, 1.0F);
    }
  }
};

void registerArcherRenderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_archer_styles_registered();
  static ArcherRenderer const renderer;
  registry.registerRenderer(
      "troops/carthage/archer", [](const DrawContext &ctx, ISubmitter &out) {
        static ArcherRenderer const static_renderer;
        Shader *archer_shader = nullptr;
        if (ctx.backend != nullptr) {
          QString shader_key = static_renderer.resolve_shader_key(ctx);
          archer_shader = ctx.backend->shader(shader_key);
          if (archer_shader == nullptr) {
            archer_shader = ctx.backend->shader(QStringLiteral("archer"));
          }
        }
        auto *scene_renderer = dynamic_cast<Renderer *>(&out);
        if ((scene_renderer != nullptr) && (archer_shader != nullptr)) {
          scene_renderer->setCurrentShader(archer_shader);
        }
        static_renderer.render(ctx, out);
        if (scene_renderer != nullptr) {
          scene_renderer->setCurrentShader(nullptr);
        }
      });
}

} // namespace Render::GL::Carthage
