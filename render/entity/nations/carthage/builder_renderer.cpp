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
#include "builder_style.h"

#include <QMatrix4x4>
#include <QString>
#include <QVector3D>
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
    constexpr int segs = 12;
    constexpr float pi = std::numbers::pi_v<float>;
    float const tr = torso.radius * 1.06F;
    float const td =
        (torso.depth > 0.0F) ? torso.depth * 0.90F : torso.radius * 0.78F;
    float const y_sh = 0.035F;
    float const y_w = waist.origin.y() - torso.origin.y();
    float const y_hem = y_w - 0.22F;

    std::vector<GeneratedEquipmentPrimitive> prims;
    prims.reserve(64);
    auto ring = [&](float y, float w, float d, std::uint8_t slot, float th) {
      for (int i = 0; i < segs; ++i) {
        float const a1 = (static_cast<float>(i) / segs) * 2.0F * pi;
        float const a2 = (static_cast<float>(i + 1) / segs) * 2.0F * pi;
        QVector3D const p1(w * std::sin(a1), y, d * std::cos(a1));
        QVector3D const p2(w * std::sin(a2), y, d * std::cos(a2));
        prims.push_back(generated_cylinder(p1, p2, th, slot));
      }
    };
    ring(y_sh + 0.045F, tr * 0.65F, td * 0.58F, k_dark, 0.020F);
    ring(y_sh + 0.03F, tr * 1.15F, td * 1.08F, k_main, 0.035F);
    ring(y_sh, tr * 1.10F, td * 1.04F, k_main, 0.032F);
    for (int i = 0; i < 5; ++i) {
      float const t = static_cast<float>(i) / 4.0F;
      float const y = y_sh - 0.02F - t * (y_sh - y_w - 0.02F);
      ring(y, tr * (1.06F - t * 0.12F), td * (1.00F - t * 0.10F), k_main,
           0.026F - t * 0.003F);
    }
    for (int i = 0; i < 6; ++i) {
      float const t = static_cast<float>(i) / 5.0F;
      float const y = y_w - 0.02F - t * (y_w - y_hem);
      float const flare = 1.0F + t * 0.28F;
      ring(y, tr * 0.85F * flare, td * 0.80F * flare, k_main,
           0.020F + t * 0.008F);
    }
    return build_generated_equipment_archetype(
        "carthage_robes", std::span<const GeneratedEquipmentPrimitive>(
                              prims.data(), prims.size()));
  }();
  return arch;
}

auto carthage_hammer_archetype() -> const RenderArchetype & {
  static const RenderArchetype arch = []() {
    constexpr std::uint8_t k_wood = 0U;
    constexpr std::uint8_t k_stone = 1U;
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
                           k_stone),
        generated_sphere(head_center + head_axis * (head_len * 0.5F),
                         head_r * 1.1F, k_stone),
        generated_sphere(head_center - head_axis * (head_len * 0.5F),
                         head_r * 0.85F, k_stone),
    }};
    return build_generated_equipment_archetype("carthage_stone_hammer", prims);
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

auto carthage_hammer_make_static_attachment(std::uint8_t base_role)
    -> Render::Creature::StaticAttachmentSpec {
  constexpr auto k_socket = Render::Humanoid::HumanoidSocket::GripL;
  constexpr auto k_bone = Render::Humanoid::HumanoidBone::HandL;
  QMatrix4x4 const bind_bone =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
          k_bone)];
  QMatrix4x4 const bind_socket =
      Render::Humanoid::bind_socket_transform(k_socket);
  auto spec = Render::Equipment::build_socket_static_attachment({
      .archetype = &carthage_hammer_archetype(),
      .socket_bone_index = static_cast<std::uint16_t>(k_bone),
      .bind_bone_transform = bind_bone,
      .bind_socket_transform = bind_socket,
      .mesh_from_socket = QMatrix4x4{},
  });
  spec.palette_role_remap[0] = base_role;
  spec.palette_role_remap[1] = static_cast<std::uint8_t>(base_role + 1U);
  return spec;
}

auto carthage_headwrap_fill_role_colors(QVector3D *out,
                                        std::size_t max) -> std::uint32_t {
  if (max < k_carthage_headwrap_role_count) {
    return 0U;
  }
  out[0] = QVector3D(0.88F, 0.82F, 0.72F);
  return k_carthage_headwrap_role_count;
}

auto carthage_robes_fill_role_colors(QVector3D *out,
                                     std::size_t max) -> std::uint32_t {
  if (max < k_carthage_robes_role_count) {
    return 0U;
  }
  QVector3D const robe(0.72F, 0.65F, 0.55F);
  out[0] = robe;
  out[1] = robe * 0.88F;
  return k_carthage_robes_role_count;
}

auto carthage_hammer_fill_role_colors(const HumanoidPalette &palette,
                                      QVector3D *out,
                                      std::size_t max) -> std::uint32_t {
  if (max < k_carthage_hammer_role_count) {
    return 0U;
  }
  out[0] = palette.wood;
  out[1] = QVector3D(0.52F, 0.50F, 0.46F);
  return k_carthage_hammer_role_count;
}

} // namespace

void register_builder_style(const std::string &nation_id,
                            const BuilderStyleConfig &style) {
  style_registry()[nation_id] = style;
}

using Render::Geom::clamp01;
using Render::Geom::clamp_f;
using Render::GL::Humanoid::mix_palette_color;
using Render::GL::Humanoid::saturate_color;

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
    static auto &reg = Render::GL::EquipmentRegistry::instance();
    static auto work_apron_ptr =
        reg.get(Render::GL::EquipmentCategory::Armor, "work_apron_carthage");
    static auto tool_belt_ptr =
        reg.get(Render::GL::EquipmentCategory::Armor, "tool_belt_carthage");
    static auto arm_guards_ptr =
        reg.get(Render::GL::EquipmentCategory::Armor, "arm_guards");

    static const WorkApronConfig work_apron_cfg = []() {
      auto *r = dynamic_cast<WorkApronRenderer *>(work_apron_ptr.get());
      return r ? r->base_config() : WorkApronConfig{};
    }();
    static const ToolBeltConfig tool_belt_cfg = []() {
      auto *r = dynamic_cast<ToolBeltRenderer *>(tool_belt_ptr.get());
      return r ? r->base_config() : ToolBeltConfig{};
    }();
    static const ArmGuardsConfig arm_guards_cfg = []() {
      auto *r = dynamic_cast<ArmGuardsRenderer *>(arm_guards_ptr.get());
      return r ? r->base_config() : ArmGuardsConfig{};
    }();

    static const UnitVisualSpec spec = []() {
      static const auto k_pelvis_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Pelvis);
      static const auto k_chest_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest);
      static const auto k_head_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head);
      static const auto k_hand_l_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::HandL);
      static const auto k_elbow_l_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ForearmL);
      static const auto k_elbow_r_bone =
          static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ForearmR);
      static const auto k_tool_belt_base_role_byte =
          static_cast<std::uint8_t>(Render::Humanoid::kHumanoidRoleCount + 1U);
      static const auto k_work_apron_base_role_byte = static_cast<std::uint8_t>(
          k_tool_belt_base_role_byte + Render::GL::kToolBeltRoleCount);
      static const auto k_arm_guards_base_role_byte = static_cast<std::uint8_t>(
          k_work_apron_base_role_byte + Render::GL::kWorkApronRoleCount);
      static const auto k_headwrap_base_role_byte = static_cast<std::uint8_t>(
          k_arm_guards_base_role_byte + Render::GL::kArmGuardsRoleCount);
      static const auto k_robes_base_role_byte = static_cast<std::uint8_t>(
          k_headwrap_base_role_byte + k_carthage_headwrap_role_count);
      static const auto k_hammer_base_role_byte = static_cast<std::uint8_t>(
          k_robes_base_role_byte + k_carthage_robes_role_count);
      static const std::array<Render::Creature::StaticAttachmentSpec, 5>
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
      static const Render::Creature::StaticAttachmentSpec k_hammer_spec =
          carthage_hammer_make_static_attachment(k_hammer_base_role_byte);
      static const std::array<Render::Creature::StaticAttachmentSpec, 13>
          k_attachments{k_tool_belt_specs[0],  k_tool_belt_specs[1],
                        k_tool_belt_specs[2],  k_tool_belt_specs[3],
                        k_tool_belt_specs[4],  k_work_apron_specs[0],
                        k_work_apron_specs[1], k_work_apron_specs[2],
                        k_arm_guards_specs[0], k_arm_guards_specs[1],
                        k_headwrap_spec,       k_robes_spec,
                        k_hammer_spec};
      static const auto k_archetype =
          Render::Creature::ArchetypeRegistry::instance()
              .register_unit_archetype(
                  "troops/carthage/builder", CreatureKind::Humanoid,
                  std::span<const Render::Creature::StaticAttachmentSpec>(
                      k_attachments.data(), k_attachments.size()),
                  +[](const void *variant_void, QVector3D *out,
                      std::uint32_t base_count,
                      std::size_t max_count) -> std::uint32_t {
                    if (variant_void == nullptr || max_count <= base_count) {
                      return base_count;
                    }
                    const auto &v =
                        *static_cast<const HumanoidVariant *>(variant_void);
                    auto count = base_count;
                    count += Render::GL::tool_belt_fill_role_colors(
                        v.palette, out + count, max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count += Render::GL::work_apron_fill_role_colors(
                        v.palette, out + count, max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count += Render::GL::arm_guards_fill_role_colors(
                        v.palette, out + count, max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count += carthage_headwrap_fill_role_colors(
                        out + count, max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count += carthage_robes_fill_role_colors(out + count,
                                                             max_count - count);
                    if (max_count <= count) {
                      return count;
                    }
                    count += carthage_hammer_fill_role_colors(
                        v.palette, out + count, max_count - count);
                    return count;
                  });

      UnitVisualSpec s{};
      s.kind = CreatureKind::Humanoid;
      s.debug_name = "troops/carthage/builder";
      s.scaling = ProportionScaling{0.98F, 1.01F, 0.96F};
      s.owned_legacy_slots = LegacySlotMask::AllHumanoid;
      s.archetype_id = k_archetype;
      return s;
    }();
    return spec;
  }

  void get_variant(const DrawContext &ctx, uint32_t seed,
                   HumanoidVariant &v) const override {
    QVector3D const team_tint = resolve_team_tint(ctx);
    v.palette = make_humanoid_palette(team_tint, seed);
    auto const &style = resolve_style(ctx);
    apply_palette_overrides(style, team_tint, v);

    auto next_rand = [](uint32_t &s) -> float {
      s = s * 1664525U + 1013904223U;
      return float(s & 0x7FFFFFU) / float(0x7FFFFFU);
    };

    uint32_t beard_seed = seed ^ 0x0EA101U;
    if (style.force_beard || next_rand(beard_seed) < 0.75F) {
      float const style_roll = next_rand(beard_seed);
      if (style_roll < 0.5F) {
        v.facial_hair.style = FacialHairStyle::ShortBeard;
        v.facial_hair.length = 0.7F + next_rand(beard_seed) * 0.3F;
      } else if (style_roll < 0.8F) {
        v.facial_hair.style = FacialHairStyle::FullBeard;
        v.facial_hair.length = 0.8F + next_rand(beard_seed) * 0.4F;
      } else {
        v.facial_hair.style = FacialHairStyle::Goatee;
        v.facial_hair.length = 0.6F + next_rand(beard_seed) * 0.3F;
      }
      v.facial_hair.color = QVector3D(0.15F + next_rand(beard_seed) * 0.1F,
                                      0.12F + next_rand(beard_seed) * 0.08F,
                                      0.10F + next_rand(beard_seed) * 0.06F);
      v.facial_hair.thickness = 0.8F + next_rand(beard_seed) * 0.2F;
    }
  }

private:
  auto
  resolve_style(const DrawContext &ctx) const -> const BuilderStyleConfig & {
    ensure_builder_styles_registered();
    auto &styles = style_registry();
    std::string nation_id;
    if (ctx.entity != nullptr) {
      if (auto *unit =
              ctx.entity->get_component<Engine::Core::UnitComponent>()) {
        nation_id = Game::Systems::nation_id_to_string(unit->nation_id);
      }
    }
    if (!nation_id.empty()) {
      auto it = styles.find(nation_id);
      if (it != styles.end()) {
        return it->second;
      }
    }
    auto f = styles.find(std::string(k_default_style_key));
    if (f != styles.end()) {
      return f->second;
    }
    static const BuilderStyleConfig def{};
    return def;
  }

  void apply_palette_overrides(const BuilderStyleConfig &style,
                               const QVector3D &team_tint,
                               HumanoidVariant &v) const {
    auto apply = [&](const std::optional<QVector3D> &c, QVector3D &t, float tw,
                     float sw) {
      t = mix_palette_color(t, c, team_tint, tw, sw);
    };
    apply(style.skin_color, v.palette.skin, 0.0F, 1.0F);
    apply(style.cloth_color, v.palette.cloth, 0.0F, 1.0F);
    apply(style.leather_color, v.palette.leather, k_team_mix_weight,
          k_style_mix_weight);
    apply(style.leather_dark_color, v.palette.leather_dark, k_team_mix_weight,
          k_style_mix_weight);
    apply(style.metal_color, v.palette.metal, k_team_mix_weight,
          k_style_mix_weight);
    apply(style.wood_color, v.palette.wood, k_team_mix_weight,
          k_style_mix_weight);
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

} // namespace Render::GL::Carthage
