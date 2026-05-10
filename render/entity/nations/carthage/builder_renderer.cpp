#include "builder_renderer.h"
#include "../../../../game/core/component.h"
#include "../../../../game/core/entity.h"
#include "../../../../game/systems/nation_id.h"
#include "../../../creature/archetype_registry.h"
#include "../../../creature/pipeline/unit_visual_spec.h"
#include "../../../equipment/armor/arm_guards_renderer.h"
#include "../../../equipment/armor/tool_belt_renderer.h"
#include "../../../equipment/armor/work_apron_renderer.h"
#include "../../../equipment/attachment_builder.h"
#include "../../../equipment/equipment_registry.h"
#include "../../../equipment/generated_equipment.h"
#include "../../../equipment/humanoid_attachment_archetype.h"
#include "../../../equipment/humanoid_equipment_archetype.h"
#include "../../../geom/math_utils.h"
#include "../../../geom/transforms.h"
#include "../../../gl/backend.h"
#include "../../../gl/primitives.h"
#include "../../../gl/render_constants.h"
#include "../../../gl/shader.h"
#include "../../../humanoid/humanoid_math.h"
#include "../../../humanoid/humanoid_renderer_base.h"
#include "../../../humanoid/humanoid_spec.h"
#include "../../../humanoid/humanoid_specs.h"
#include "../../../humanoid/skeleton.h"
#include "../../../humanoid/style_palette.h"
#include "../../../palette.h"
#include "../../../scene_renderer.h"
#include "../../../static_attachment_spec.h"
#include "../../../submitter.h"
#include "../../registry.h"
#include "../../renderer_constants.h"
#include "../equipment_loadout_catalog.h"
#include "builder_style.h"

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <qmatrix4x4.h>
#include <qstringliteral.h>
#include <qvectornd.h>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

#include "../../../equipment/equipment_submit.h"
using Render::Geom::cylinder_between;
using Render::Geom::sphere_at;

namespace Render::GL::Carthage {

namespace {

constexpr std::string_view k_default_style_key = "default";

auto style_registry() -> std::unordered_map<std::string, BuilderStyleConfig> & {
  static std::unordered_map<std::string, BuilderStyleConfig> styles;
  return styles;
}

void ensure_builder_styles_registered() {
  static const bool registered = []() {
    register_carthage_builder_style();
    return true;
  }();
  (void)registered;
}

constexpr float k_team_mix_weight = 0.65F;
constexpr float k_style_mix_weight = 0.35F;

void draw_stone_hammer_into(const DrawContext &ctx,
                            const HumanoidPalette &palette,
                            const BodyFrames &frames,
                            const AnimationInputs &anim,
                            EquipmentBatch &batch) {
  QVector3D const wood = palette.wood;
  QVector3D const stone_color(0.52F, 0.50F, 0.46F);
  QVector3D const stone_dark(0.42F, 0.40F, 0.36F);

  QVector3D const hand = frames.hand_l.origin;
  QVector3D const up(0.0F, 1.0F, 0.0F);
  QVector3D const forward(0.0F, 0.0F, 1.0F);
  QVector3D const right(1.0F, 0.0F, 0.0F);

  QVector3D handle_axis;
  QVector3D head_axis;
  if (anim.is_constructing) {
    handle_axis = forward;
    head_axis = up;
  } else {
    handle_axis = up;
    head_axis = right;
  }

  float const h_len = 0.30F;
  QVector3D const handle_offset = anim.is_constructing
                                      ? (forward * 0.11F + up * 0.02F)
                                      : (up * 0.11F + forward * 0.02F);
  QVector3D const h_top = hand + handle_offset;
  QVector3D const h_bot = h_top - handle_axis * h_len;

  batch.meshes.push_back({get_unit_cylinder(), nullptr,
                          cylinder_between(ctx.model, h_bot, h_top, 0.015F),
                          wood, nullptr, 1.0F, 0});

  float const head_len = 0.09F;
  float const head_r = 0.028F;
  QVector3D const head_center = h_top + handle_axis * 0.03F;

  batch.meshes.push_back(
      {get_unit_cylinder(), nullptr,
       cylinder_between(ctx.model, head_center - head_axis * (head_len * 0.5F),
                        head_center + head_axis * (head_len * 0.5F), head_r),
       stone_color, nullptr, 1.0F, 0});

  batch.meshes.push_back(
      {get_unit_sphere(), nullptr,
       sphere_at(ctx.model, head_center + head_axis * (head_len * 0.5F),
                 head_r * 1.1F),
       stone_dark, nullptr, 1.0F, 0});

  batch.meshes.push_back(
      {get_unit_sphere(), nullptr,
       sphere_at(ctx.model, head_center - head_axis * (head_len * 0.5F),
                 head_r * 0.85F),
       stone_color * 0.92F, nullptr, 1.0F, 0});
}

void draw_headwrap_into(const DrawContext &ctx, const BodyFrames &frames,
                        EquipmentBatch &batch) {
  QVector3D const wrap_color(0.88F, 0.82F, 0.72F);
  QVector3D const head_top = frames.head.origin + frames.head.up * 0.05F;
  QVector3D const head_back =
      frames.head.origin - frames.head.forward * 0.03F + frames.head.up * 0.02F;

  batch.meshes.push_back({get_unit_sphere(), nullptr,
                          sphere_at(ctx.model, head_top, 0.052F), wrap_color,
                          nullptr, 1.0F, 0});
  batch.meshes.push_back({get_unit_sphere(), nullptr,
                          sphere_at(ctx.model, head_back, 0.048F),
                          wrap_color * 0.95F, nullptr, 1.0F, 0});
}

void draw_extended_forearm_into(const DrawContext &ctx,
                                const HumanoidPalette &palette,
                                const BodyFrames &frames,
                                EquipmentBatch &batch) {
  QVector3D const skin_color = palette.skin;

  QVector3D const elbow_r =
      frames.shoulder_r.origin +
      (frames.hand_r.origin - frames.shoulder_r.origin) * 0.55F;
  QVector3D const hand_r = frames.hand_r.origin;
  for (int i = 0; i < 4; ++i) {
    float const t = 0.25F + static_cast<float>(i) * 0.20F;
    QVector3D const pos = elbow_r * (1.0F - t) + hand_r * t;
    float const r = 0.022F - static_cast<float>(i) * 0.002F;
    batch.meshes.push_back({get_unit_sphere(), nullptr,
                            sphere_at(ctx.model, pos, r), skin_color, nullptr,
                            1.0F, 0});
  }
}

void draw_craftsman_robes_into(const DrawContext &ctx,
                               const HumanoidPalette &palette,
                               const BodyFrames &frames, uint32_t seed,
                               EquipmentBatch &batch) {
  using HP = HumanProportions;
  const AttachmentFrame &torso = frames.torso;
  const AttachmentFrame &waist = frames.waist;

  if (torso.radius <= 0.0F) {
    return;
  }

  float const var = hash_01(seed ^ 0xCDEU);
  QVector3D robe_color;
  if (var < 0.35F) {
    robe_color = QVector3D(0.85F, 0.78F, 0.68F);
  } else if (var < 0.65F) {
    robe_color = QVector3D(0.72F, 0.65F, 0.55F);
  } else {
    robe_color = QVector3D(0.62F, 0.58F, 0.52F);
  }
  QVector3D const robe_dark = robe_color * 0.88F;

  const QVector3D &origin = torso.origin;
  const QVector3D &right = torso.right;
  const QVector3D &up = torso.up;
  const QVector3D &forward = torso.forward;
  float const tr = torso.radius * 1.06F;
  float const td =
      (torso.depth > 0.0F) ? torso.depth * 0.90F : torso.radius * 0.78F;

  float const y_sh = origin.y() + 0.035F;
  float const y_w = waist.origin.y();
  float const y_hem = y_w - 0.22F;

  constexpr int segs = 12;
  constexpr float pi = std::numbers::pi_v<float>;

  auto ring = [&](float y, float w, float d, const QVector3D &c, float th) {
    for (int i = 0; i < segs; ++i) {
      float const a1 = (static_cast<float>(i) / segs) * 2.0F * pi;
      float const a2 = (static_cast<float>(i + 1) / segs) * 2.0F * pi;
      QVector3D const p1 = origin + right * (w * std::sin(a1)) +
                           forward * (d * std::cos(a1)) + up * (y - origin.y());
      QVector3D const p2 = origin + right * (w * std::sin(a2)) +
                           forward * (d * std::cos(a2)) + up * (y - origin.y());
      batch.meshes.push_back({get_unit_cylinder(), nullptr,
                              cylinder_between(ctx.model, p1, p2, th), c,
                              nullptr, 1.0F, 0});
    }
  };

  ring(y_sh + 0.045F, tr * 0.65F, td * 0.58F, robe_dark, 0.020F);
  ring(y_sh + 0.03F, tr * 1.15F, td * 1.08F, robe_color, 0.035F);
  ring(y_sh, tr * 1.10F, td * 1.04F, robe_color, 0.032F);

  for (int i = 0; i < 5; ++i) {
    float const t = static_cast<float>(i) / 4.0F;
    float const y = y_sh - 0.02F - t * (y_sh - y_w - 0.02F);
    QVector3D const c = robe_color * (1.0F - t * 0.05F);
    ring(y, tr * (1.06F - t * 0.12F), td * (1.00F - t * 0.10F), c,
         0.026F - t * 0.003F);
  }

  for (int i = 0; i < 6; ++i) {
    float const t = static_cast<float>(i) / 5.0F;
    float const y = y_w - 0.02F - t * (y_w - y_hem);
    float const flare = 1.0F + t * 0.28F;
    QVector3D const c = robe_color * (1.0F - t * 0.06F);
    ring(y, tr * 0.85F * flare, td * 0.80F * flare, c, 0.020F + t * 0.008F);
  }

  auto sleeve = [&](const QVector3D &sh, const QVector3D &out_dir) {
    QVector3D const back = -forward;
    QVector3D anchor = sh + up * 0.055F + back * 0.012F;
    for (int i = 0; i < 4; ++i) {
      float const t = static_cast<float>(i) / 4.0F;
      QVector3D const pos = anchor + out_dir * (0.012F + t * 0.022F) +
                            forward * (-0.012F + t * 0.05F) - up * (t * 0.035F);
      float const r = HP::UPPER_ARM_R * (1.48F - t * 0.08F);
      batch.meshes.push_back(
          {get_unit_sphere(), nullptr, sphere_at(ctx.model, pos, r),
           robe_color * (1.0F - t * 0.03F), nullptr, 1.0F, 0});
    }
  };
  sleeve(frames.shoulder_l.origin, -right);
  sleeve(frames.shoulder_r.origin, right);

  draw_extended_forearm_into(ctx, palette, frames, batch);
}

} // namespace

namespace {

constexpr std::uint32_t k_carthage_headwrap_role_count = 1U;
constexpr std::uint32_t k_carthage_robes_role_count = 2U;
constexpr std::uint32_t k_carthage_hammer_role_count = 2U;
constexpr std::uint32_t k_carthage_saw_role_count = 4U;
constexpr std::uint32_t k_carthage_chisel_role_count = 2U;
constexpr std::uint32_t k_carthage_civilian_sash_role_count = 2U;

enum CarthageSawPaletteSlot : std::uint8_t {
  k_carthage_saw_wood_slot = 0U,
  k_carthage_saw_metal_slot = 1U,
  k_carthage_saw_metal_dark_slot = 2U,
  k_carthage_saw_leather_slot = 3U,
};

enum CarthageChiselPaletteSlot : std::uint8_t {
  k_carthage_chisel_wood_slot = 0U,
  k_carthage_chisel_metal_slot = 1U,
};

enum class BuilderConstructionTool : std::uint8_t {
  Hammer = 0U,
  Saw = 1U,
  Chisel = 2U,
};

auto carthage_headwrap_archetype() -> const RenderArchetype & {
  static const RenderArchetype arch = []() {
    QVector3D const top_local(0.0F, 0.05F, 0.0F);
    QVector3D const back_local(0.0F, 0.02F, -0.03F);
    std::array<GeneratedEquipmentPrimitive, 2> const prims{{
        generated_sphere(top_local, 0.052F, 0U),
        generated_sphere(back_local, 0.048F, 0U),
    }};
    return build_generated_equipment_archetype("carthage_headwrap", prims);
  }();
  return arch;
}

auto carthage_robes_archetype() -> const RenderArchetype & {
  static const RenderArchetype arch = []() {
    const auto &bind = Render::Humanoid::humanoid_bind_body_frames();
    const AttachmentFrame &torso = bind.torso;
    const AttachmentFrame &waist = bind.waist;
    constexpr std::uint8_t k_main = 0U;
    constexpr std::uint8_t k_dark = 1U;
    float const tr = torso.radius * 1.06F;
    float const td =
        (torso.depth > 0.0F) ? torso.depth * 0.90F : torso.radius * 0.78F;
    float const y_sh = 0.035F;
    float const y_w = waist.origin.y() - torso.origin.y();
    float const y_hem = y_w - 0.24F;
    constexpr int k_segments = 12;
    constexpr int k_skirt_layers = 9;
    constexpr float k_pi = std::numbers::pi_v<float>;

    std::vector<GeneratedEquipmentPrimitive> prims;
    prims.reserve(72);
    auto add_ring = [&](float y, float width, float depth, std::uint8_t slot,
                        float thickness) {
      for (int i = 0; i < k_segments; ++i) {
        float const a0 = (static_cast<float>(i) / k_segments) * 2.0F * k_pi;
        float const a1 = (static_cast<float>(i + 1) / k_segments) * 2.0F * k_pi;
        prims.push_back(generated_cylinder(
            QVector3D(width * std::sin(a0), y, depth * std::cos(a0)),
            QVector3D(width * std::sin(a1), y, depth * std::cos(a1)), thickness,
            slot));
      }
    };

    auto add_section = [&](float y_top, float y_bot, float width_top,
                           float width_bot, float depth_top, float depth_bot,
                           std::uint8_t slot) {
      float const radius =
          (width_top + width_bot + depth_top + depth_bot) * 0.25F;
      prims.push_back(generated_cylinder(QVector3D(0.0F, y_bot, 0.0F),
                                         QVector3D(0.0F, y_top, 0.0F), radius,
                                         slot));
    };

    add_ring(y_sh + 0.046F, tr * 0.68F, td * 0.60F, k_dark, 0.018F);
    add_ring(y_sh + 0.030F, tr * 1.14F, td * 1.05F, k_main, 0.033F);
    add_ring(y_sh - 0.004F, tr * 1.08F, td * 0.99F, k_main, 0.029F);

    add_section(y_sh + 0.016F, y_sh - 0.09F, tr * 1.10F, tr * 1.02F, td * 0.99F,
                td * 0.92F, k_main);
    add_section(y_sh - 0.09F, y_w + 0.018F, tr * 1.00F, tr * 0.90F, td * 0.90F,
                td * 0.82F, k_dark);
    add_ring(y_w + 0.012F, tr * 0.92F, td * 0.84F, k_dark, 0.024F);

    for (int layer = 0; layer < k_skirt_layers; ++layer) {
      float const t =
          static_cast<float>(layer) / static_cast<float>(k_skirt_layers - 1);
      float const y = y_w - 0.012F - t * (y_w - y_hem);
      float const flare = 1.0F + t * 0.34F;
      float const thickness = 0.019F + t * 0.011F;
      std::uint8_t const slot = (layer % 3 == 2) ? k_dark : k_main;
      add_ring(y, tr * 0.90F * flare, td * 0.82F * flare, slot, thickness);
    }
    add_ring(y_hem + 0.012F, tr * 1.22F, td * 1.10F, k_dark, 0.021F);

    QVector3D const left_top(-tr * 0.72F, y_sh - 0.010F, td * 0.10F);
    QVector3D const right_top(tr * 0.72F, y_sh - 0.010F, td * 0.10F);
    QVector3D const left_bottom(-tr * 0.86F, y_hem + 0.060F, td * 0.18F);
    QVector3D const right_bottom(tr * 0.86F, y_hem + 0.060F, td * 0.18F);
    prims.push_back(
        generated_cylinder(left_top, left_bottom, tr * 0.16F, k_main));
    prims.push_back(
        generated_cylinder(right_top, right_bottom, tr * 0.16F, k_main));

    for (int fold = -1; fold <= 1; ++fold) {
      float const t = static_cast<float>(fold);
      QVector3D const start(t * tr * 0.36F, y_w - 0.004F, td * 0.56F);
      QVector3D const end(t * tr * 0.42F, y_hem + 0.030F, td * 0.76F);
      prims.push_back(generated_cylinder(start, end, tr * 0.10F, k_main));
    }
    return build_generated_equipment_archetype(
        "carthage_robes", std::span<const GeneratedEquipmentPrimitive>(
                              prims.data(), prims.size()));
  }();
  return arch;
}

auto carthage_civilian_sash_archetype() -> const RenderArchetype & {
  static const RenderArchetype arch = []() {
    const auto &bind = Render::Humanoid::humanoid_bind_body_frames();
    const AttachmentFrame &torso = bind.torso;
    const AttachmentFrame &waist = bind.waist;
    float const tr = torso.radius * 1.02F;
    float const td =
        (torso.depth > 0.0F) ? torso.depth * 0.88F : torso.radius * 0.76F;
    float const y_w = waist.origin.y() - torso.origin.y();

    std::vector<GeneratedEquipmentPrimitive> prims;
    prims.reserve(2);
    auto add_box = [&](const QVector3D &center, const QVector3D &scale,
                       std::uint8_t slot) {
      prims.push_back(generated_box(center, scale, slot));
    };

    add_box(QVector3D(0.0F, y_w - 0.01F, 0.0F),
            QVector3D(tr * 1.44F, 0.095F, td * 1.46F), 0U);

    add_box(QVector3D(tr * 0.22F, y_w - 0.11F, td * 0.84F),
            QVector3D(0.08F, 0.18F, 0.05F), 1U);

    return build_generated_equipment_archetype(
        "carthage_civilian_sash", std::span<const GeneratedEquipmentPrimitive>(
                                      prims.data(), prims.size()));
  }();
  return arch;
}

auto carthage_hammer_archetype() -> const RenderArchetype & {
  static const RenderArchetype arch = []() {
    constexpr std::uint8_t k_wood = 0U;
    constexpr std::uint8_t k_bronze = 1U;
    QVector3D const handle_top(0.0F, 0.11F, 0.02F);
    QVector3D const handle_bot(0.0F, -0.19F, 0.02F);
    QVector3D const head_axis(1.0F, 0.0F, 0.0F);
    QVector3D const head_center = handle_top + QVector3D(0.0F, 0.03F, 0.0F);
    float const head_len = 0.09F;
    float const head_r = 0.028F;
    std::array<GeneratedEquipmentPrimitive, 4> const prims{{
        generated_cylinder(handle_bot, handle_top, 0.015F, k_wood),
        generated_cylinder(head_center - head_axis * (head_len * 0.5F),
                           head_center + head_axis * (head_len * 0.5F), head_r,
                           k_bronze),
        generated_sphere(head_center + head_axis * (head_len * 0.5F),
                         head_r * 1.1F, k_bronze),
        generated_sphere(head_center - head_axis * (head_len * 0.5F),
                         head_r * 0.85F, k_bronze),
    }};
    return build_generated_equipment_archetype("carthage_stone_hammer", prims);
  }();
  return arch;
}

auto carthage_saw_archetype() -> const RenderArchetype & {
  static const RenderArchetype arch = []() {
    std::array<GeneratedEquipmentPrimitive, 8> const prims{{
        generated_cylinder(QVector3D(-0.01F, -0.17F, 0.03F),
                           QVector3D(0.07F, -0.08F, 0.03F), 0.019F,
                           k_carthage_saw_wood_slot),
        generated_sphere(QVector3D(0.07F, -0.07F, 0.03F), 0.024F,
                         k_carthage_saw_leather_slot),
        generated_cylinder(QVector3D(0.02F, -0.06F, 0.03F),
                           QVector3D(0.04F, 0.18F, 0.03F), 0.013F,
                           k_carthage_saw_metal_slot),
        generated_cylinder(QVector3D(0.04F, 0.18F, 0.03F),
                           QVector3D(-0.08F, 0.11F, 0.03F), 0.018F,
                           k_carthage_saw_metal_dark_slot),
        generated_sphere(QVector3D(-0.01F, 0.145F, 0.03F), 0.017F,
                         k_carthage_saw_metal_slot),
        generated_sphere(QVector3D(-0.04F, 0.13F, 0.03F), 0.014F,
                         k_carthage_saw_metal_slot),
        generated_sphere(QVector3D(-0.065F, 0.117F, 0.03F), 0.012F,
                         k_carthage_saw_metal_slot),
        generated_sphere(QVector3D(-0.085F, 0.105F, 0.03F), 0.010F,
                         k_carthage_saw_metal_slot),
    }};
    return build_generated_equipment_archetype("carthage_builder_saw", prims);
  }();
  return arch;
}

auto carthage_chisel_archetype() -> const RenderArchetype & {
  static const RenderArchetype arch = []() {
    std::array<GeneratedEquipmentPrimitive, 4> const prims{{
        generated_cylinder(QVector3D(0.0F, -0.15F, 0.01F),
                           QVector3D(0.0F, 0.05F, 0.01F), 0.017F,
                           k_carthage_chisel_wood_slot),
        generated_sphere(QVector3D(0.0F, -0.17F, 0.01F), 0.022F,
                         k_carthage_chisel_wood_slot),
        generated_cylinder(QVector3D(0.0F, 0.05F, 0.01F),
                           QVector3D(0.0F, 0.20F, 0.01F), 0.011F,
                           k_carthage_chisel_metal_slot),
        generated_sphere(QVector3D(0.0F, 0.215F, 0.01F), 0.011F,
                         k_carthage_chisel_metal_slot),
    }};
    return build_generated_equipment_archetype("carthage_builder_chisel",
                                               prims);
  }();
  return arch;
}

auto carthage_headwrap_make_static_attachment(std::uint16_t head_bone,
                                              std::uint8_t base_role)
    -> Render::Creature::StaticAttachmentSpec {
  const auto &bind = Render::Humanoid::humanoid_bind_body_frames();
  QMatrix4x4 const bind_mat = make_humanoid_attachment_transform_scaled(
      QMatrix4x4{}, bind.head, QVector3D(0.0F, 0.0F, 0.0F),
      QVector3D(1.0F, 1.0F, 1.0F));
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &carthage_headwrap_archetype(),
      .socket_bone_index = head_bone,
      .unit_local_pose_at_bind = bind_mat,
  });
  spec.palette_role_remap[0] = base_role;
  return spec;
}

auto carthage_headwrap_contribution_attachments(std::uint8_t base_role)
    -> std::vector<Render::Creature::StaticAttachmentSpec> {
  return {carthage_headwrap_make_static_attachment(
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head),
      base_role)};
}

auto carthage_robes_make_static_attachment(std::uint16_t chest_bone,
                                           std::uint8_t base_role)
    -> Render::Creature::StaticAttachmentSpec {
  const auto &bind = Render::Humanoid::humanoid_bind_body_frames();
  QMatrix4x4 const bind_mat = make_humanoid_attachment_transform_scaled(
      QMatrix4x4{}, bind.torso, QVector3D(0.0F, 0.0F, 0.0F),
      QVector3D(1.0F, 1.0F, 1.0F));
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &carthage_robes_archetype(),
      .socket_bone_index = chest_bone,
      .unit_local_pose_at_bind = bind_mat,
  });
  spec.palette_role_remap[0] = base_role;
  spec.palette_role_remap[1] = static_cast<std::uint8_t>(base_role + 1U);
  return spec;
}

auto carthage_robes_contribution_attachments(std::uint8_t base_role)
    -> std::vector<Render::Creature::StaticAttachmentSpec> {
  return {carthage_robes_make_static_attachment(
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest),
      base_role)};
}

auto carthage_tool_make_static_attachment(
    const RenderArchetype &archetype, std::uint8_t base_role,
    std::span<const std::uint8_t> slot_indices)
    -> Render::Creature::StaticAttachmentSpec {
  constexpr auto k_socket = Render::Humanoid::HumanoidSocket::GripR;
  constexpr auto k_bone = Render::Humanoid::HumanoidBone::HandR;
  QMatrix4x4 const bind_bone =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
          k_bone)];
  QMatrix4x4 const bind_socket =
      Render::Humanoid::bind_socket_transform(k_socket);
  auto spec = Render::Equipment::build_socket_static_attachment({
      .archetype = &archetype,
      .socket_bone_index = static_cast<std::uint16_t>(k_bone),
      .bind_bone_transform = bind_bone,
      .bind_socket_transform = bind_socket,
      .mesh_from_socket = QMatrix4x4{},
  });
  for (std::size_t i = 0; i < slot_indices.size(); ++i) {
    spec.palette_role_remap[slot_indices[i]] =
        static_cast<std::uint8_t>(base_role + i);
  }
  return spec;
}

auto carthage_civilian_sash_make_static_attachment(std::uint16_t chest_bone,
                                                   std::uint8_t base_role)
    -> Render::Creature::StaticAttachmentSpec {
  const auto &bind = Render::Humanoid::humanoid_bind_body_frames();
  QMatrix4x4 const bind_mat = make_humanoid_attachment_transform_scaled(
      QMatrix4x4{}, bind.torso, QVector3D(0.0F, 0.0F, 0.0F),
      QVector3D(1.0F, 1.0F, 1.0F));
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &carthage_civilian_sash_archetype(),
      .socket_bone_index = chest_bone,
      .unit_local_pose_at_bind = bind_mat,
  });
  spec.palette_role_remap[0] = base_role;
  spec.palette_role_remap[1] = static_cast<std::uint8_t>(base_role + 1U);
  return spec;
}

auto carthage_civilian_sash_contribution_attachments(std::uint8_t base_role)
    -> std::vector<Render::Creature::StaticAttachmentSpec> {
  return {carthage_civilian_sash_make_static_attachment(
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest),
      base_role)};
}

auto carthage_headwrap_fill_role_colors(QVector3D *out,
                                        std::size_t max) -> std::uint32_t {
  if (max < k_carthage_headwrap_role_count) {
    return 0U;
  }
  out[0] = QVector3D(0.88F, 0.82F, 0.72F);
  return k_carthage_headwrap_role_count;
}

auto carthage_robes_fill_role_colors(const HumanoidPalette &palette,
                                     QVector3D *out,
                                     std::size_t max) -> std::uint32_t {
  if (max < k_carthage_robes_role_count) {
    return 0U;
  }
  out[0] = palette.cloth;
  out[1] = palette.cloth * 0.84F;
  return k_carthage_robes_role_count;
}

auto carthage_headwrap_extra_role_colors(
    const void *, QVector3D *out, std::uint32_t base_count,
    std::size_t max_count) -> std::uint32_t {
  if (max_count - base_count < k_carthage_headwrap_role_count) {
    return base_count;
  }
  out[base_count] = QVector3D(0.88F, 0.82F, 0.72F);
  return base_count + k_carthage_headwrap_role_count;
}

auto carthage_robes_extra_role_colors(const void *variant_void, QVector3D *out,
                                      std::uint32_t base_count,
                                      std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  const auto &variant = *static_cast<const HumanoidVariant *>(variant_void);
  return base_count + carthage_robes_fill_role_colors(variant.palette,
                                                      out + base_count,
                                                      max_count - base_count);
}

auto carthage_tool_belt_fill_role_colors(const HumanoidPalette &palette,
                                         QVector3D *out,
                                         std::size_t max) -> std::uint32_t {
  if (max < Render::GL::k_tool_belt_role_count) {
    return 0U;
  }
  out[0] = palette.leather;
  out[1] = palette.leather_dark;
  out[2] = palette.metal;
  out[3] = palette.metal * 0.90F;
  out[4] = palette.wood;
  return Render::GL::k_tool_belt_role_count;
}

auto carthage_work_apron_fill_role_colors(const HumanoidPalette &palette,
                                          QVector3D *out,
                                          std::size_t max) -> std::uint32_t {
  if (max < Render::GL::k_work_apron_role_count) {
    return 0U;
  }
  QVector3D const apron = palette.leather * 0.74F + palette.cloth * 0.26F;
  for (std::uint32_t i = 0; i < 7U; ++i) {
    float const t = static_cast<float>(i) / 6.0F * 0.18F;
    out[i] = apron * (1.0F - t);
  }
  out[7] = apron * 0.82F;
  out[8] = palette.leather_dark;
  return Render::GL::k_work_apron_role_count;
}

auto carthage_arm_guards_fill_role_colors(const HumanoidPalette &palette,
                                          QVector3D *out,
                                          std::size_t max) -> std::uint32_t {
  if (max < Render::GL::k_arm_guards_role_count) {
    return 0U;
  }
  out[0] = palette.leather_dark;
  out[1] = palette.leather_dark * 0.80F + palette.metal * 0.20F;
  return Render::GL::k_arm_guards_role_count;
}

auto carthage_hammer_fill_role_colors(const HumanoidPalette &palette,
                                      QVector3D *out,
                                      std::size_t max) -> std::uint32_t {
  if (max < k_carthage_hammer_role_count) {
    return 0U;
  }
  out[0] = palette.wood;
  out[1] = palette.metal * 0.88F + QVector3D(0.08F, 0.05F, 0.01F);
  return k_carthage_hammer_role_count;
}

auto carthage_saw_fill_role_colors(const HumanoidPalette &palette,
                                   QVector3D *out,
                                   std::size_t max) -> std::uint32_t {
  if (max < k_carthage_saw_role_count) {
    return 0U;
  }
  out[0] = palette.wood;
  out[1] = palette.metal * 0.92F + QVector3D(0.06F, 0.04F, 0.01F);
  out[2] = palette.metal * 0.62F;
  out[3] = palette.leather_dark;
  return k_carthage_saw_role_count;
}

auto carthage_chisel_fill_role_colors(const HumanoidPalette &palette,
                                      QVector3D *out,
                                      std::size_t max) -> std::uint32_t {
  if (max < k_carthage_chisel_role_count) {
    return 0U;
  }
  out[0] = palette.wood;
  out[1] = palette.metal * 0.86F + QVector3D(0.05F, 0.03F, 0.01F);
  return k_carthage_chisel_role_count;
}

auto register_builder_tool_variant_archetype(
    std::string_view debug_name,
    Render::Creature::ArchetypeId base_archetype_id,
    const Render::Creature::StaticAttachmentSpec &tool_spec,
    Render::Creature::ArchetypeDescriptor::ExtraRoleColorsFn tool_role_colors,
    std::uint8_t tool_role_count) -> Render::Creature::ArchetypeId {
  auto &registry = Render::Creature::ArchetypeRegistry::instance();
  const auto *base_desc = registry.get(base_archetype_id);
  if (base_desc == nullptr) {
    return Render::Creature::k_invalid_archetype;
  }

  auto desc = *base_desc;
  desc.debug_name = debug_name;
  desc.bake_attachments[desc.bake_attachment_count++] = tool_spec;
  desc.role_count =
      static_cast<std::uint8_t>(desc.role_count + tool_role_count);
  desc.append_extra_role_colors_fn(tool_role_colors);
  return registry.register_archetype(desc);
}

auto carthage_builder_base_archetype() -> Render::Creature::ArchetypeId {
  static const auto archetype = []() {
    auto &registry = Render::Creature::ArchetypeRegistry::instance();
    const auto *base_desc =
        registry.get(Render::Creature::ArchetypeRegistry::k_humanoid_base);
    if (base_desc == nullptr) {
      return Render::Creature::k_invalid_archetype;
    }

    auto desc = *base_desc;
    desc.debug_name = "troops/carthage/builder/base";
    desc.bake_attachments[desc.bake_attachment_count++] =
        carthage_headwrap_make_static_attachment(
            static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head),
            desc.role_count);
    desc.role_count = static_cast<std::uint8_t>(desc.role_count +
                                                k_carthage_headwrap_role_count);
    desc.append_extra_role_colors_fn(&carthage_headwrap_extra_role_colors);
    desc.bake_attachments[desc.bake_attachment_count++] =
        carthage_robes_make_static_attachment(
            static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest),
            desc.role_count);
    desc.role_count = static_cast<std::uint8_t>(desc.role_count +
                                                k_carthage_robes_role_count);
    desc.append_extra_role_colors_fn(&carthage_robes_extra_role_colors);
    return registry.register_archetype(desc);
  }();
  return archetype;
}

auto carthage_builder_idle_archetype() -> Render::Creature::ArchetypeId {
  static const auto archetype = []() {
    const auto loadout = Render::GL::Nation::resolve_equipment_loadout(
        "troops/carthage/builder");
    const std::array<EquipmentHandle, 3> handles{loadout.tool_belt_handle,
                                                 loadout.work_apron_handle,
                                                 loadout.arm_guards_handle};
    return resolve_humanoid_equipment_archetype(
        "troops/carthage/builder", carthage_builder_base_archetype(), handles);
  }();
  return archetype;
}

auto carthage_civilian_sash_fill_role_colors(const HumanoidPalette &palette,
                                             QVector3D *out,
                                             std::size_t max) -> std::uint32_t {
  if (max < k_carthage_civilian_sash_role_count) {
    return 0U;
  }
  out[0] = palette.leather;
  out[1] = palette.leather_dark;
  return k_carthage_civilian_sash_role_count;
}

auto carthage_civilian_sash_extra_role_colors(
    const void *variant_void, QVector3D *out, std::uint32_t base_count,
    std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  const auto &variant = *static_cast<const HumanoidVariant *>(variant_void);
  return base_count +
         carthage_civilian_sash_fill_role_colors(
             variant.palette, out + base_count, max_count - base_count);
}

void ensure_carthage_civilian_equipment_contributions_registered() {
  static const bool registered = []() {
    const auto loadout = Render::GL::Nation::resolve_equipment_loadout(
        "troops/carthage/civilian");
    if (loadout.helmet_handle != k_invalid_equipment_handle) {
      register_humanoid_equipment_contribution(
          loadout.helmet_handle,
          {.build_attachments = &carthage_headwrap_contribution_attachments,
           .append_role_colors = &carthage_headwrap_extra_role_colors,
           .role_count =
               static_cast<std::uint8_t>(k_carthage_headwrap_role_count)});
    }
    if (loadout.armor_handle != k_invalid_equipment_handle) {
      register_humanoid_equipment_contribution(
          loadout.armor_handle,
          {.build_attachments = &carthage_robes_contribution_attachments,
           .append_role_colors = &carthage_robes_extra_role_colors,
           .role_count =
               static_cast<std::uint8_t>(k_carthage_robes_role_count)});
    }
    if (loadout.cloak_handle != k_invalid_equipment_handle) {
      register_humanoid_equipment_contribution(
          loadout.cloak_handle,
          {.build_attachments =
               &carthage_civilian_sash_contribution_attachments,
           .append_role_colors = &carthage_civilian_sash_extra_role_colors,
           .role_count =
               static_cast<std::uint8_t>(k_carthage_civilian_sash_role_count)});
    }
    return true;
  }();
  (void)registered;
}

auto carthage_builder_tool_base_role_byte() -> std::uint8_t {
  static const auto base_role = static_cast<std::uint8_t>(
      Render::Humanoid::k_humanoid_role_count + 1U +
      Render::GL::k_tool_belt_role_count + Render::GL::k_work_apron_role_count +
      Render::GL::k_arm_guards_role_count + k_carthage_headwrap_role_count +
      k_carthage_robes_role_count);
  return base_role;
}

auto carthage_builder_common_attachments()
    -> const std::array<Render::Creature::StaticAttachmentSpec, 13> & {
  static const auto k_pelvis_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Pelvis);
  static const auto k_chest_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest);
  static const auto k_head_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head);
  static const auto k_elbow_l_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ForearmL);
  static const auto k_elbow_r_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ForearmR);
  static const auto k_tool_belt_base_role_byte =
      static_cast<std::uint8_t>(Render::Humanoid::k_humanoid_role_count + 1U);
  static const auto k_work_apron_base_role_byte = static_cast<std::uint8_t>(
      k_tool_belt_base_role_byte + Render::GL::k_tool_belt_role_count);
  static const auto k_arm_guards_base_role_byte = static_cast<std::uint8_t>(
      k_work_apron_base_role_byte + Render::GL::k_work_apron_role_count);
  static const auto k_headwrap_base_role_byte = static_cast<std::uint8_t>(
      k_arm_guards_base_role_byte + Render::GL::k_arm_guards_role_count);
  static const auto k_robes_base_role_byte = static_cast<std::uint8_t>(
      k_headwrap_base_role_byte + k_carthage_headwrap_role_count);
  static const std::array<Render::Creature::StaticAttachmentSpec, 6>
      k_tool_belt_specs = Render::GL::tool_belt_make_static_attachments(
          k_pelvis_bone, k_tool_belt_base_role_byte);
  static const std::array<Render::Creature::StaticAttachmentSpec, 3>
      k_work_apron_specs = Render::GL::work_apron_make_static_attachments(
          k_pelvis_bone, k_chest_bone, k_work_apron_base_role_byte);
  static const std::array<Render::Creature::StaticAttachmentSpec, 2>
      k_arm_guards_specs = Render::GL::arm_guards_make_static_attachments(
          k_elbow_l_bone, k_elbow_r_bone, k_arm_guards_base_role_byte);
  static const Render::Creature::StaticAttachmentSpec k_headwrap_spec =
      carthage_headwrap_make_static_attachment(k_head_bone,
                                               k_headwrap_base_role_byte);
  static const Render::Creature::StaticAttachmentSpec k_robes_spec =
      carthage_robes_make_static_attachment(k_chest_bone,
                                            k_robes_base_role_byte);
  static const std::array<Render::Creature::StaticAttachmentSpec, 13>
      k_attachments{
          k_tool_belt_specs[0],  k_tool_belt_specs[1],  k_tool_belt_specs[2],
          k_tool_belt_specs[3],  k_tool_belt_specs[4],  k_tool_belt_specs[5],
          k_work_apron_specs[0], k_work_apron_specs[1], k_work_apron_specs[2],
          k_arm_guards_specs[0], k_arm_guards_specs[1], k_headwrap_spec,
          k_robes_spec};
  return k_attachments;
}

auto carthage_builder_attachments_with_tool(
    const Render::Creature::StaticAttachmentSpec &tool_spec)
    -> std::array<Render::Creature::StaticAttachmentSpec, 14> {
  std::array<Render::Creature::StaticAttachmentSpec, 14> attachments{};
  auto const &common = carthage_builder_common_attachments();
  for (std::size_t i = 0; i < common.size(); ++i) {
    attachments[i] = common[i];
  }
  attachments.back() = tool_spec;
  return attachments;
}

auto carthage_builder_extra_role_colors(
    const void *variant_void, QVector3D *out, std::uint32_t base_count,
    std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  const auto &v = *static_cast<const HumanoidVariant *>(variant_void);
  auto count = base_count;
  count += carthage_tool_belt_fill_role_colors(v.palette, out + count,
                                               max_count - count);
  if (max_count <= count) {
    return count;
  }
  count += carthage_work_apron_fill_role_colors(v.palette, out + count,
                                                max_count - count);
  if (max_count <= count) {
    return count;
  }
  count += carthage_arm_guards_fill_role_colors(v.palette, out + count,
                                                max_count - count);
  if (max_count <= count) {
    return count;
  }
  count += carthage_headwrap_fill_role_colors(out + count, max_count - count);
  if (max_count <= count) {
    return count;
  }
  count += carthage_robes_fill_role_colors(v.palette, out + count,
                                           max_count - count);
  return count;
}

auto carthage_builder_hammer_unit_archetype() -> Render::Creature::ArchetypeId {
  static constexpr std::array<std::uint8_t, 2> k_slots{0U, 1U};
  static const auto k_tool_spec = carthage_tool_make_static_attachment(
      carthage_hammer_archetype(),
      Render::Creature::ArchetypeRegistry::instance()
          .get(carthage_builder_idle_archetype())
          ->role_count,
      k_slots);
  static const auto k_archetype = register_builder_tool_variant_archetype(
      "troops/carthage/builder/construction_hammer",
      carthage_builder_idle_archetype(), k_tool_spec,
      +[](const void *variant_void, QVector3D *out, std::uint32_t base_count,
          std::size_t max_count) -> std::uint32_t {
        if (variant_void == nullptr || max_count <= base_count) {
          return base_count;
        }
        const auto &v = *static_cast<const HumanoidVariant *>(variant_void);
        return base_count +
               carthage_hammer_fill_role_colors(v.palette, out + base_count,
                                                max_count - base_count);
      },
      static_cast<std::uint8_t>(k_carthage_hammer_role_count));
  return k_archetype;
}

auto carthage_builder_saw_unit_archetype() -> Render::Creature::ArchetypeId {
  static constexpr std::array<std::uint8_t, 4> k_slots{
      k_carthage_saw_wood_slot, k_carthage_saw_metal_slot,
      k_carthage_saw_metal_dark_slot, k_carthage_saw_leather_slot};
  static const auto k_tool_spec = carthage_tool_make_static_attachment(
      carthage_saw_archetype(),
      Render::Creature::ArchetypeRegistry::instance()
          .get(carthage_builder_idle_archetype())
          ->role_count,
      k_slots);
  static const auto k_archetype = register_builder_tool_variant_archetype(
      "troops/carthage/builder/construction_saw",
      carthage_builder_idle_archetype(), k_tool_spec,
      +[](const void *variant_void, QVector3D *out, std::uint32_t base_count,
          std::size_t max_count) -> std::uint32_t {
        if (variant_void == nullptr || max_count <= base_count) {
          return base_count;
        }
        const auto &v = *static_cast<const HumanoidVariant *>(variant_void);
        return base_count +
               carthage_saw_fill_role_colors(v.palette, out + base_count,
                                             max_count - base_count);
      },
      static_cast<std::uint8_t>(k_carthage_saw_role_count));
  return k_archetype;
}

auto carthage_builder_chisel_unit_archetype() -> Render::Creature::ArchetypeId {
  static constexpr std::array<std::uint8_t, 2> k_slots{
      k_carthage_chisel_wood_slot, k_carthage_chisel_metal_slot};
  static const auto k_tool_spec = carthage_tool_make_static_attachment(
      carthage_chisel_archetype(),
      Render::Creature::ArchetypeRegistry::instance()
          .get(carthage_builder_idle_archetype())
          ->role_count,
      k_slots);
  static const auto k_archetype = register_builder_tool_variant_archetype(
      "troops/carthage/builder/construction_chisel",
      carthage_builder_idle_archetype(), k_tool_spec,
      +[](const void *variant_void, QVector3D *out, std::uint32_t base_count,
          std::size_t max_count) -> std::uint32_t {
        if (variant_void == nullptr || max_count <= base_count) {
          return base_count;
        }
        const auto &v = *static_cast<const HumanoidVariant *>(variant_void);
        return base_count +
               carthage_chisel_fill_role_colors(v.palette, out + base_count,
                                                max_count - base_count);
      },
      static_cast<std::uint8_t>(k_carthage_chisel_role_count));
  return k_archetype;
}

void carthage_builder_render_hook(
    const Render::GL::HumanoidAnimationContext &anim,
    const Render::GL::HumanoidVariant &, std::uint32_t seed,
    Render::Creature::Pipeline::HumanoidRenderSelection &io_selection) {
  if (!anim.inputs.is_constructing) {
    return;
  }
  switch (static_cast<BuilderConstructionTool>(seed % 3U)) {
  case BuilderConstructionTool::Hammer:
    io_selection.archetype = carthage_builder_hammer_unit_archetype();
    io_selection.state = Render::Creature::AnimationStateId::AttackSword;
    break;
  case BuilderConstructionTool::Saw:
    io_selection.archetype = carthage_builder_saw_unit_archetype();
    io_selection.state = Render::Creature::AnimationStateId::AttackSword;
    break;
  case BuilderConstructionTool::Chisel:
    io_selection.archetype = carthage_builder_chisel_unit_archetype();
    io_selection.state = Render::Creature::AnimationStateId::AttackSpear;
    break;
  }
}

} // namespace

void register_builder_style(const std::string &nation_id,
                            const BuilderStyleConfig &style) {
  style_registry()[nation_id] = style;
}

using Render::GL::Humanoid::mix_palette_color;

auto resolve_builder_style(const DrawContext &ctx)
    -> const BuilderStyleConfig & {
  ensure_builder_styles_registered();
  auto &styles = style_registry();
  std::string nation_id;
  if (ctx.entity != nullptr) {
    if (auto *unit = ctx.entity->get_component<Engine::Core::UnitComponent>()) {
      nation_id = Game::Systems::nation_id_to_string(unit->nation_id);
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
  static const BuilderStyleConfig default_style{};
  return default_style;
}

void apply_builder_palette_overrides(const BuilderStyleConfig &style,
                                     const QVector3D &team_tint,
                                     HumanoidVariant &variant) {
  auto apply = [&](const std::optional<QVector3D> &c, QVector3D &t, float tw,
                   float sw) {
    t = mix_palette_color(t, c, team_tint, tw, sw);
  };
  apply(style.skin_color, variant.palette.skin, 0.0F, 1.0F);
  apply(style.cloth_color, variant.palette.cloth, 0.0F, 1.0F);
  apply(style.leather_color, variant.palette.leather, k_team_mix_weight,
        k_style_mix_weight);
  apply(style.leather_dark_color, variant.palette.leather_dark,
        k_team_mix_weight, k_style_mix_weight);
  apply(style.metal_color, variant.palette.metal, k_team_mix_weight,
        k_style_mix_weight);
  apply(style.wood_color, variant.palette.wood, k_team_mix_weight,
        k_style_mix_weight);
}

void apply_carthage_beard(std::uint32_t seed, float chance, bool force_beard,
                          HumanoidVariant &variant) {
  (void)seed;
  (void)chance;
  if (!force_beard) {
    return;
  }

  variant.facial_hair.style = FacialHairStyle::FullBeard;
  variant.facial_hair.length = 0.92F;
  variant.facial_hair.color = QVector3D(0.18F, 0.14F, 0.11F);
  variant.facial_hair.thickness = 0.88F;
}

void apply_carthage_civilian_palette(const QVector3D &team_tint,
                                     std::uint32_t seed,
                                     HumanoidVariant &variant) {
  QVector3D const skin_color(0.08F, 0.07F, 0.065F);
  QVector3D const wood_color(0.45F, 0.35F, 0.22F);
  QVector3D const metal_color(0.70F, 0.52F, 0.32F);
  float const robe_roll = hash_01(seed ^ 0xCA77U);
  float const sash_roll = hash_01(seed ^ 0xB10EU);

  QVector3D robe_color = robe_roll < 0.33F   ? QVector3D(0.84F, 0.76F, 0.63F)
                         : robe_roll < 0.66F ? QVector3D(0.74F, 0.67F, 0.56F)
                                             : QVector3D(0.68F, 0.61F, 0.52F);
  QVector3D sash_color = sash_roll < 0.5F ? QVector3D(0.16F, 0.38F, 0.42F)
                                          : QVector3D(0.42F, 0.29F, 0.16F);

  variant.palette.cloth = mix_palette_color(variant.palette.cloth, robe_color,
                                            team_tint, 0.08F, 0.92F);
  variant.palette.leather = mix_palette_color(
      variant.palette.leather, sash_color, team_tint, 0.06F, 0.94F);
  variant.palette.leather_dark = variant.palette.leather * 0.74F;
  variant.palette.wood = mix_palette_color(variant.palette.wood, wood_color,
                                           team_tint, 0.08F, 0.92F);
  variant.palette.metal = mix_palette_color(variant.palette.metal, metal_color,
                                            team_tint, 0.06F, 0.94F);
  variant.palette.skin = mix_palette_color(variant.palette.skin, skin_color,
                                           team_tint, 0.0F, 1.0F);
}

class BuilderRenderer : public HumanoidRendererBase {
public:
  BuilderRenderer() = default;

  friend void
  register_builder_renderer(Render::GL::EntityRendererRegistry &registry);

  auto get_proportion_scaling() const -> QVector3D override {
    return {0.98F, 1.01F, 0.96F};
  }

  auto visual_spec() const
      -> const Render::Creature::Pipeline::UnitVisualSpec & override {
    using namespace Render::Creature::Pipeline;
    static const UnitVisualSpec spec = []() {
      UnitVisualSpec s{};
      s.kind = CreatureKind::Humanoid;
      s.debug_name = "troops/carthage/builder";
      s.scaling = ProportionScaling{0.98F, 1.01F, 0.96F};
      s.owned_legacy_slots = LegacySlotMask::AllHumanoid;
      s.archetype_id = carthage_builder_idle_archetype();
      s.humanoid_render_hook = carthage_builder_render_hook;
      return s;
    }();
    return spec;
  }

  void get_variant(const DrawContext &ctx, uint32_t seed,
                   HumanoidVariant &v) const override {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
    auto const &style = resolve_builder_style(ctx);
    apply_builder_palette_overrides(style, team_tint, v);
    apply_carthage_beard(seed, 0.75F, style.force_beard, v);
  }
};

class CivilianRenderer : public HumanoidRendererBase {
public:
  auto get_proportion_scaling() const -> QVector3D override {
    return {0.96F, 1.0F, 0.94F};
  }

  auto visual_spec() const
      -> const Render::Creature::Pipeline::UnitVisualSpec & override {
    using namespace Render::Creature::Pipeline;
    static const UnitVisualSpec spec = []() {
      UnitVisualSpec s{};
      ensure_carthage_civilian_equipment_contributions_registered();
      const auto loadout = Render::GL::Nation::resolve_equipment_loadout(
          "troops/carthage/civilian");
      const std::array<EquipmentHandle, 3> handles{
          loadout.helmet_handle, loadout.armor_handle, loadout.cloak_handle};
      s.kind = CreatureKind::Humanoid;
      s.debug_name = "troops/carthage/civilian";
      s.scaling = ProportionScaling{0.96F, 1.0F, 0.94F};
      s.owned_legacy_slots = LegacySlotMask::AllHumanoid;
      s.archetype_id = resolve_humanoid_equipment_archetype(
          "troops/carthage/civilian",
          Render::Creature::ArchetypeRegistry::k_humanoid_base, handles);
      return s;
    }();
    return spec;
  }

  void get_variant(const DrawContext &ctx, std::uint32_t seed,
                   HumanoidVariant &v) const override {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
    apply_carthage_civilian_palette(team_tint, seed, v);
    apply_carthage_beard(seed, 0.35F, false, v);
  }
};

void register_builder_renderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_builder_styles_registered();
  static BuilderRenderer const renderer;
  registry.register_renderer("troops/carthage/builder",
                             [](const DrawContext &ctx, ISubmitter &out) {
                               static BuilderRenderer const r;
                               r.render(ctx, out);
                             });
}

void register_civilian_renderer(Render::GL::EntityRendererRegistry &registry) {
  ensure_builder_styles_registered();
  static CivilianRenderer const renderer;
  registry.register_renderer("troops/carthage/civilian",
                             [](const DrawContext &ctx, ISubmitter &out) {
                               static CivilianRenderer const r;
                               r.render(ctx, out);
                             });
}

} // namespace Render::GL::Carthage
