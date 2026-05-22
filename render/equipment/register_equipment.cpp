#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include "../horse/dimensions.h"
#include "../horse/horse_spec.h"
#include "../humanoid/humanoid_spec.h"
#include "../humanoid/skeleton.h"
#include "armor/arm_guards_renderer.h"
#include "armor/armor_heavy_carthage.h"
#include "armor/armor_light_carthage.h"
#include "armor/carthage_shoulder_cover.h"
#include "armor/cloak_renderer.h"
#include "armor/roman_armor.h"
#include "armor/roman_greaves.h"
#include "armor/roman_shoulder_cover.h"
#include "armor/shoulder_cover_archetype.h"
#include "armor/tool_belt_renderer.h"
#include "armor/work_apron_renderer.h"
#include "equipment_registry.h"
#include "helmets/carthage_heavy_helmet.h"
#include "helmets/carthage_light_helmet.h"
#include "helmets/headwrap.h"
#include "helmets/roman_heavy_helmet.h"
#include "helmets/roman_light_helmet.h"
#include "horse/armor/champion_renderer.h"
#include "horse/armor/crupper_renderer.h"
#include "horse/armor/leather_barding_renderer.h"
#include "horse/armor/scale_barding_renderer.h"
#include "horse/decorations/saddle_bag_renderer.h"
#include "horse/horse_attachment_archetype.h"
#include "horse/saddles/carthage_saddle_renderer.h"
#include "horse/saddles/light_cavalry_saddle_renderer.h"
#include "horse/saddles/roman_saddle_renderer.h"
#include "horse/tack/blanket_renderer.h"
#include "horse/tack/bridle_renderer.h"
#include "horse/tack/reins_renderer.h"
#include "horse_equipment_archetype.h"
#include "humanoid_equipment_archetype.h"
#include "render_archetype_registry.h"
#include "weapons/bow_renderer.h"
#include "weapons/quiver_renderer.h"
#include "weapons/roman_scutum.h"
#include "weapons/shield_carthage.h"
#include "weapons/shield_renderer.h"
#include "weapons/spear_renderer.h"
#include "weapons/sword_carthage.h"
#include "weapons/sword_renderer.h"
#include "weapons/sword_roman.h"

namespace Render::GL {
namespace {

using StaticAttachmentSpec = Render::Creature::StaticAttachmentSpec;

template <std::size_t N>
auto to_vector(const std::array<StaticAttachmentSpec, N>& attachments)
    -> std::vector<StaticAttachmentSpec> {
  return {attachments.begin(), attachments.end()};
}

auto humanoid_head_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head);
}

auto humanoid_chest_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest);
}

auto humanoid_pelvis_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Pelvis);
}

auto humanoid_foot_l_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::FootL);
}

auto humanoid_foot_r_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::FootR);
}

auto humanoid_hip_l_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::HipL);
}

auto humanoid_shoulder_l_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ShoulderL);
}

auto humanoid_shoulder_r_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ShoulderR);
}

auto humanoid_forearm_l_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ForearmL);
}

auto humanoid_forearm_r_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ForearmR);
}

auto humanoid_head_bind_matrix() -> const QMatrix4x4& {
  static const QMatrix4x4 matrix =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
          Render::Humanoid::HumanoidBone::Head)];
  return matrix;
}

auto humanoid_shoulder_bind_matrix(bool left) -> const QMatrix4x4& {
  const auto& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  static const QMatrix4x4 left_matrix =
      Render::GL::make_shoulder_cover_transform(QMatrix4x4{},
                                                bind_frames.shoulder_l.origin,
                                                -bind_frames.torso.right,
                                                bind_frames.torso.up);
  static const QMatrix4x4 right_matrix =
      Render::GL::make_shoulder_cover_transform(QMatrix4x4{},
                                                bind_frames.shoulder_r.origin,
                                                bind_frames.torso.right,
                                                bind_frames.torso.up);
  return left ? left_matrix : right_matrix;
}

auto humanoid_shin_bind_matrix(bool left) -> const QMatrix4x4& {
  const auto& bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  static const auto frame_to_matrix = [](const Render::GL::AttachmentFrame& f) {
    QMatrix4x4 matrix;
    matrix.setColumn(0, QVector4D(f.right, 0.0F));
    matrix.setColumn(1, QVector4D(f.up, 0.0F));
    matrix.setColumn(2, QVector4D(f.forward, 0.0F));
    matrix.setColumn(3, QVector4D(f.origin, 1.0F));
    return matrix;
  };
  static const QMatrix4x4 left_matrix = frame_to_matrix(bind_frames.shin_l);
  static const QMatrix4x4 right_matrix = frame_to_matrix(bind_frames.shin_r);
  return left ? left_matrix : right_matrix;
}

auto horse_root_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Horse::HorseBone::Root);
}

auto horse_head_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Horse::HorseBone::Head);
}

auto horse_neck_top_bone() -> std::uint16_t {
  return static_cast<std::uint16_t>(Render::Horse::HorseBone::NeckTop);
}

auto horse_root_bind_matrix() -> const QMatrix4x4& {
  static const QMatrix4x4 matrix =
      Render::Horse::horse_bind_palette()[static_cast<std::size_t>(
          Render::Horse::HorseBone::Root)];
  return matrix;
}

auto horse_head_bind_matrix() -> const QMatrix4x4& {
  static const QMatrix4x4 matrix =
      Render::Horse::horse_bind_palette()[static_cast<std::size_t>(
          Render::Horse::HorseBone::Head)];
  return matrix;
}

auto horse_neck_top_bind_matrix() -> const QMatrix4x4& {
  static const QMatrix4x4 matrix =
      Render::Horse::horse_bind_palette()[static_cast<std::size_t>(
          Render::Horse::HorseBone::NeckTop)];
  return matrix;
}

auto roman_bow_config() -> const BowRenderConfig& {
  static const BowRenderConfig config = []() {
    BowRenderConfig cfg;
    cfg.bow_depth = 0.22F;
    cfg.bow_curve_factor = 1.0F;
    cfg.bow_height_scale = 1.0F;
    cfg.bow_forward_offset = -0.24F;
    cfg.bow_top_y = HumanProportions::SHOULDER_Y + 0.55F;
    cfg.bow_bot_y = HumanProportions::WAIST_Y - 0.25F;
    return cfg;
  }();
  return config;
}

auto carthage_bow_config() -> const BowRenderConfig& {
  static const BowRenderConfig config = []() {
    BowRenderConfig cfg;
    cfg.bow_depth = 0.28F;
    cfg.bow_curve_factor = 1.2F;
    cfg.bow_height_scale = 0.95F;
    cfg.bow_forward_offset = -0.24F;
    cfg.bow_top_y = HumanProportions::SHOULDER_Y + 0.55F;
    cfg.bow_bot_y = HumanProportions::WAIST_Y - 0.25F;
    return cfg;
  }();
  return config;
}

auto mounted_quiver_config() -> const QuiverRenderConfig& {
  static const QuiverRenderConfig config = []() {
    QuiverRenderConfig cfg;
    cfg.fletching_color = QVector3D(0.85F, 0.40F, 0.40F);
    cfg.quiver_radius = HumanProportions::HEAD_RADIUS * 0.45F;
    return cfg;
  }();
  return config;
}

auto roman_quiver_config() -> const QuiverRenderConfig& {
  static const QuiverRenderConfig config{};
  return config;
}

auto carthage_quiver_config() -> const QuiverRenderConfig& {
  static const QuiverRenderConfig config = []() {
    QuiverRenderConfig cfg;
    cfg.fletching_color = QVector3D(0.90F, 0.82F, 0.28F);
    cfg.quiver_radius = HumanProportions::HEAD_RADIUS * 0.45F;
    return cfg;
  }();
  return config;
}

auto marcellus_bow_config() -> const BowRenderConfig& {
  static const BowRenderConfig config = []() {
    BowRenderConfig cfg = roman_bow_config();
    cfg.bow_depth = 0.18F;
    cfg.bow_curve_factor = 0.92F;
    cfg.bow_height_scale = 0.88F;
    cfg.string_color = QVector3D(0.42F, 0.16F, 0.14F);
    return cfg;
  }();
  return config;
}

auto hasdrubal_bow_config() -> const BowRenderConfig& {
  static const BowRenderConfig config = []() {
    BowRenderConfig cfg = carthage_bow_config();
    cfg.bow_depth = 0.34F;
    cfg.bow_curve_factor = 1.36F;
    cfg.bow_height_scale = 1.04F;
    cfg.string_color = QVector3D(0.26F, 0.18F, 0.12F);
    return cfg;
  }();
  return config;
}

auto roman_sword_config() -> const SwordRenderConfig& {
  static const SwordRenderConfig config = []() {
    SwordRenderConfig cfg;
    cfg.metal_color = QVector3D(0.76F, 0.79F, 0.88F);
    cfg.sword_length = 0.84F;
    cfg.sword_width = 0.088F;
    cfg.guard_half_width = 0.104F;
    cfg.handle_radius = 0.018F;
    cfg.pommel_radius = 0.054F;
    cfg.pommel_length = 0.050F;
    cfg.blade_ricasso = 0.10F;
    cfg.blade_taper_bias = 0.32F;
    cfg.blade_mid_width_scale = 1.18F;
    cfg.blade_tip_width_scale = 0.22F;
    cfg.blade_curve = 0.0F;
    cfg.guard_curve = 0.03F;
    cfg.guard_spike_length = 0.045F;
    cfg.material_id = 3;
    return cfg;
  }();
  return config;
}

auto marcellus_quiver_config() -> const QuiverRenderConfig& {
  static const QuiverRenderConfig config = []() {
    QuiverRenderConfig cfg = roman_quiver_config();
    cfg.quiver_radius = HumanProportions::HEAD_RADIUS * 0.38F;
    cfg.fletching_color = QVector3D(0.74F, 0.22F, 0.20F);
    return cfg;
  }();
  return config;
}

auto hasdrubal_quiver_config() -> const QuiverRenderConfig& {
  static const QuiverRenderConfig config = []() {
    QuiverRenderConfig cfg = carthage_quiver_config();
    cfg.quiver_radius = HumanProportions::HEAD_RADIUS * 0.50F;
    cfg.fletching_color = QVector3D(0.92F, 0.72F, 0.16F);
    return cfg;
  }();
  return config;
}

auto fabius_spear_config() -> const SpearRenderConfig& {
  static const SpearRenderConfig config = []() {
    SpearRenderConfig cfg;
    cfg.shaft_color = QVector3D(0.46F, 0.33F, 0.20F);
    cfg.spearhead_color = QVector3D(0.82F, 0.84F, 0.88F);
    cfg.spear_length = 1.30F;
    cfg.shaft_radius = 0.021F;
    cfg.spearhead_length = 0.20F;
    return cfg;
  }();
  return config;
}

auto hanno_spear_config() -> const SpearRenderConfig& {
  static const SpearRenderConfig config = []() {
    SpearRenderConfig cfg;
    cfg.shaft_color = QVector3D(0.52F, 0.34F, 0.18F);
    cfg.spearhead_color = QVector3D(0.82F, 0.68F, 0.32F);
    cfg.spear_length = 1.08F;
    cfg.shaft_radius = 0.017F;
    cfg.spearhead_length = 0.24F;
    return cfg;
  }();
  return config;
}

auto carthage_sword_config() -> const SwordRenderConfig& {
  static const SwordRenderConfig config = []() {
    SwordRenderConfig cfg;
    cfg.metal_color = QVector3D(0.78F, 0.70F, 0.56F);
    cfg.sword_length = 0.96F;
    cfg.sword_width = 0.076F;
    cfg.guard_half_width = 0.116F;
    cfg.handle_radius = 0.015F;
    cfg.pommel_radius = 0.042F;
    cfg.pommel_length = 0.035F;
    cfg.blade_ricasso = 0.14F;
    cfg.blade_taper_bias = 0.80F;
    cfg.blade_mid_width_scale = 1.10F;
    cfg.blade_tip_width_scale = 0.12F;
    cfg.blade_curve = 0.14F;
    cfg.guard_curve = -0.04F;
    cfg.guard_spike_length = 0.070F;
    cfg.material_id = 3;
    return cfg;
  }();
  return config;
}

auto scipio_sword_config() -> const SwordRenderConfig& {
  static const SwordRenderConfig config = []() {
    SwordRenderConfig cfg = roman_sword_config();
    cfg.metal_color = QVector3D(0.84F, 0.86F, 0.92F);
    cfg.sword_length = 0.92F;
    cfg.sword_width = 0.100F;
    cfg.guard_half_width = 0.140F;
    cfg.handle_radius = 0.020F;
    cfg.pommel_radius = 0.060F;
    cfg.pommel_length = 0.090F;
    cfg.blade_ricasso = 0.08F;
    cfg.blade_taper_bias = 0.40F;
    cfg.blade_mid_width_scale = 1.28F;
    cfg.blade_tip_width_scale = 0.15F;
    cfg.blade_curve = -0.02F;
    cfg.guard_curve = 0.06F;
    cfg.guard_spike_length = 0.090F;
    return cfg;
  }();
  return config;
}

auto hannibal_sword_config() -> const SwordRenderConfig& {
  static const SwordRenderConfig config = []() {
    SwordRenderConfig cfg = carthage_sword_config();
    cfg.metal_color = QVector3D(0.82F, 0.72F, 0.38F);
    cfg.sword_length = 1.00F;
    cfg.sword_width = 0.082F;
    cfg.guard_half_width = 0.122F;
    cfg.handle_radius = 0.019F;
    cfg.pommel_radius = 0.055F;
    cfg.pommel_length = 0.055F;
    cfg.blade_ricasso = 0.06F;
    cfg.blade_taper_bias = 0.88F;
    cfg.blade_mid_width_scale = 1.02F;
    cfg.blade_tip_width_scale = 0.10F;
    cfg.blade_curve = 0.20F;
    cfg.guard_curve = -0.06F;
    cfg.guard_spike_length = 0.105F;
    return cfg;
  }();
  return config;
}

auto sepulcher_sword_config() -> const SwordRenderConfig& {
  static const SwordRenderConfig config = []() {
    SwordRenderConfig cfg = carthage_sword_config();
    cfg.metal_color = QVector3D(0.64F, 0.68F, 0.70F);
    cfg.sword_length = 1.05F;
    cfg.sword_width = 0.058F;
    cfg.guard_half_width = 0.132F;
    cfg.handle_radius = 0.014F;
    cfg.pommel_radius = 0.040F;
    cfg.pommel_length = 0.120F;
    cfg.blade_ricasso = 0.05F;
    cfg.blade_taper_bias = 0.94F;
    cfg.blade_mid_width_scale = 0.78F;
    cfg.blade_tip_width_scale = 0.06F;
    cfg.blade_curve = 0.08F;
    cfg.guard_curve = -0.08F;
    cfg.guard_spike_length = 0.140F;
    cfg.has_scabbard = false;
    return cfg;
  }();
  return config;
}

constexpr float k_scabbard_radius = 0.060F * 0.85F;

auto carthage_cloak_config() -> const CloakConfig& {
  static const CloakConfig config = []() {
    CloakConfig cfg;
    cfg.primary_color = QVector3D(0.14F, 0.38F, 0.54F);
    cfg.trim_color = QVector3D(0.75F, 0.66F, 0.42F);
    return cfg;
  }();
  return config;
}

auto carthage_mounted_cloak_config() -> const CloakConfig& {
  static const CloakConfig config = []() {
    CloakConfig cfg = carthage_cloak_config();
    cfg.length_scale = 0.94F;
    cfg.shoulder_anchor_up = 0.06F;
    cfg.drape_anchor_up = 0.00F;
    cfg.drape_anchor_back = 0.62F;
    cfg.clasp_anchor_up = 0.06F;
    return cfg;
  }();
  return config;
}

auto sepulcher_cloak_config() -> const CloakConfig& {
  static const CloakConfig config = []() {
    CloakConfig cfg;
    cfg.primary_color = QVector3D(0.095F, 0.095F, 0.11F);
    cfg.trim_color = QVector3D(0.50F, 0.53F, 0.56F);
    cfg.length_scale = 1.08F;
    cfg.width_scale = 0.92F;
    cfg.shoulder_anchor_up = 0.10F;
    cfg.drape_anchor_up = 0.02F;
    cfg.drape_anchor_back = 0.60F;
    cfg.clasp_anchor_up = 0.08F;
    cfg.back_material_id = 12;
    cfg.shoulder_material_id = 13;
    return cfg;
  }();
  return config;
}

auto roman_cloak_config() -> const CloakConfig& {
  static const CloakConfig config = []() {
    CloakConfig cfg;
    cfg.primary_color = QVector3D(0.70F, 0.15F, 0.18F);
    cfg.trim_color = QVector3D(0.78F, 0.72F, 0.58F);
    cfg.back_material_id = 12;
    cfg.shoulder_material_id = 13;
    return cfg;
  }();
  return config;
}

auto roman_mounted_cloak_config() -> const CloakConfig& {
  static const CloakConfig config = []() {
    CloakConfig cfg = roman_cloak_config();
    cfg.length_scale = 0.94F;
    cfg.shoulder_anchor_up = 0.06F;
    cfg.drape_anchor_up = 0.00F;
    cfg.drape_anchor_back = 0.62F;
    cfg.clasp_anchor_up = 0.06F;
    return cfg;
  }();
  return config;
}

auto build_carthage_light_helmet_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_shell_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_neck_guard_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_cheek_guards_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_nasal_guard_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_crest_archetype(true),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_rivets_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
  };
}

auto build_carthage_heavy_helmet_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::carthage_heavy_helmet_make_static_attachment(
          Render::GL::carthage_heavy_helmet_shell_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_heavy_helmet_make_static_attachment(
          Render::GL::carthage_heavy_helmet_neck_guard_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_heavy_helmet_make_static_attachment(
          Render::GL::carthage_heavy_helmet_cheek_guards_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_heavy_helmet_make_static_attachment(
          Render::GL::carthage_heavy_helmet_face_plate_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_heavy_helmet_make_static_attachment(
          Render::GL::carthage_heavy_helmet_crest_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_heavy_helmet_make_static_attachment(
          Render::GL::carthage_heavy_helmet_rivets_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
  };
}

auto build_roman_heavy_helmet_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::roman_heavy_helmet_make_static_attachment(
      humanoid_head_bone(), base_role_byte, humanoid_head_bind_matrix())};
}

auto build_roman_light_helmet_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::roman_light_helmet_make_static_attachment(
      humanoid_head_bone(), base_role_byte, humanoid_head_bind_matrix())};
}

auto build_headwrap_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::headwrap_make_static_attachment(
      humanoid_head_bone(), base_role_byte, humanoid_head_bind_matrix())};
}

auto build_roman_light_armor_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::roman_light_armor_make_static_attachment(humanoid_chest_bone(),
                                                               base_role_byte)};
}

auto build_roman_heavy_armor_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::roman_heavy_armor_make_static_attachment(humanoid_chest_bone(),
                                                               base_role_byte)};
}

auto build_carthage_light_armor_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::armor_light_carthage_make_static_attachment(humanoid_chest_bone(),
                                                                  base_role_byte)};
}

auto build_carthage_heavy_armor_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::armor_heavy_carthage_make_static_attachment(humanoid_chest_bone(),
                                                                  base_role_byte)};
}

auto build_roman_greaves_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::roman_greaves_make_static_attachment(
          humanoid_foot_l_bone(), base_role_byte, humanoid_shin_bind_matrix(true)),
      Render::GL::roman_greaves_make_static_attachment(
          humanoid_foot_r_bone(), base_role_byte, humanoid_shin_bind_matrix(false)),
  };
}

auto build_roman_shoulder_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::roman_shoulder_cover_make_static_attachment(
          humanoid_shoulder_l_bone(),
          base_role_byte,
          humanoid_shoulder_bind_matrix(true)),
      Render::GL::roman_shoulder_cover_make_static_attachment(
          humanoid_shoulder_r_bone(),
          base_role_byte,
          humanoid_shoulder_bind_matrix(false)),
  };
}

auto build_carthage_shoulder_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::carthage_shoulder_cover_make_static_attachment(
          humanoid_shoulder_l_bone(),
          base_role_byte,
          humanoid_shoulder_bind_matrix(true)),
      Render::GL::carthage_shoulder_cover_make_static_attachment(
          humanoid_shoulder_r_bone(),
          base_role_byte,
          humanoid_shoulder_bind_matrix(false)),
  };
}

auto build_tool_belt_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return to_vector(Render::GL::tool_belt_make_static_attachments(humanoid_pelvis_bone(),
                                                                 base_role_byte));
}

auto build_work_apron_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return to_vector(Render::GL::work_apron_make_static_attachments(
      humanoid_pelvis_bone(), humanoid_chest_bone(), base_role_byte));
}

auto build_arm_guards_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return to_vector(Render::GL::arm_guards_make_static_attachments(
      humanoid_forearm_l_bone(), humanoid_forearm_r_bone(), base_role_byte));
}

auto build_spear_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  static const SpearRenderConfig config = []() {
    SpearRenderConfig cfg;
    cfg.shaft_color = QVector3D(0.5F, 0.3F, 0.2F) * QVector3D(0.85F, 0.75F, 0.65F);
    cfg.spearhead_color = QVector3D(0.75F, 0.76F, 0.80F);
    cfg.spear_length = 1.15F;
    cfg.shaft_radius = 0.018F;
    cfg.spearhead_length = 0.16F;
    return cfg;
  }();
  return to_vector(Render::GL::spear_make_static_attachments(config, base_role_byte));
}

auto build_fabius_spear_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return to_vector(
      Render::GL::spear_make_static_attachments(fabius_spear_config(), base_role_byte));
}

auto build_hanno_spear_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return to_vector(
      Render::GL::spear_make_static_attachments(hanno_spear_config(), base_role_byte));
}

auto build_roman_scutum_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::roman_scutum_make_static_attachment(base_role_byte)};
}

auto build_carthage_shield_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::carthage_shield_make_static_attachment(
      Render::GL::CarthageShieldConfig{}, base_role_byte)};
}

auto build_roman_sword_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::sword_make_static_attachment(roman_sword_config(), base_role_byte),
      Render::GL::scabbard_make_static_attachment(
          k_scabbard_radius,
          humanoid_hip_l_bone(),
          static_cast<std::uint8_t>(base_role_byte + Render::GL::k_sword_role_count)),
  };
}

auto build_scipio_sword_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::sword_make_static_attachment(scipio_sword_config(), base_role_byte),
      Render::GL::scabbard_make_static_attachment(
          k_scabbard_radius * 1.08F,
          humanoid_hip_l_bone(),
          static_cast<std::uint8_t>(base_role_byte + Render::GL::k_sword_role_count)),
  };
}

auto build_carthage_sword_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::sword_make_static_attachment(carthage_sword_config(), base_role_byte),
      Render::GL::scabbard_make_static_attachment(
          k_scabbard_radius,
          humanoid_hip_l_bone(),
          static_cast<std::uint8_t>(base_role_byte + Render::GL::k_sword_role_count)),
  };
}

auto build_hannibal_sword_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::sword_make_static_attachment(hannibal_sword_config(), base_role_byte),
      Render::GL::scabbard_make_static_attachment(
          k_scabbard_radius * 1.02F,
          humanoid_hip_l_bone(),
          static_cast<std::uint8_t>(base_role_byte + Render::GL::k_sword_role_count)),
  };
}

auto build_sepulcher_sword_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::sword_make_static_attachment(
      sepulcher_sword_config(), base_role_byte, QVector3D(0.02F, 0.90F, 0.36F))};
}

auto build_roman_bow_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return to_vector(
      Render::GL::bow_make_static_attachments(roman_bow_config(), base_role_byte));
}

auto build_marcellus_bow_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return to_vector(
      Render::GL::bow_make_static_attachments(marcellus_bow_config(), base_role_byte));
}

auto build_carthage_bow_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return to_vector(
      Render::GL::bow_make_static_attachments(carthage_bow_config(), base_role_byte));
}

auto build_hasdrubal_bow_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return to_vector(
      Render::GL::bow_make_static_attachments(hasdrubal_bow_config(), base_role_byte));
}

auto build_quiver_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return to_vector(Render::GL::quiver_make_static_attachments(
      mounted_quiver_config(), humanoid_pelvis_bone(), base_role_byte));
}

auto build_roman_quiver_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return to_vector(Render::GL::quiver_make_static_attachments(
      roman_quiver_config(), humanoid_pelvis_bone(), base_role_byte));
}

auto build_marcellus_quiver_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return to_vector(Render::GL::quiver_make_static_attachments(
      marcellus_quiver_config(), humanoid_pelvis_bone(), base_role_byte));
}

auto build_carthage_quiver_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return to_vector(Render::GL::quiver_make_static_attachments(
      carthage_quiver_config(), humanoid_pelvis_bone(), base_role_byte));
}

auto build_hasdrubal_quiver_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return to_vector(Render::GL::quiver_make_static_attachments(
      hasdrubal_quiver_config(), humanoid_pelvis_bone(), base_role_byte));
}

auto build_carthage_cloak_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::cloak_make_static_attachment(carthage_cloak_config(),
                                                   Render::GL::shared_cloak_meshes(),
                                                   humanoid_chest_bone(),
                                                   base_role_byte)};
}

auto build_carthage_mounted_cloak_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::cloak_make_static_attachment(carthage_mounted_cloak_config(),
                                                   Render::GL::shared_cloak_meshes(),
                                                   humanoid_chest_bone(),
                                                   base_role_byte)};
}

auto build_sepulcher_cloak_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::cloak_make_static_attachment(sepulcher_cloak_config(),
                                                   Render::GL::shared_cloak_meshes(),
                                                   humanoid_chest_bone(),
                                                   base_role_byte)};
}

auto build_roman_cloak_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::cloak_make_static_attachment(roman_cloak_config(),
                                                   Render::GL::shared_cloak_meshes(),
                                                   humanoid_chest_bone(),
                                                   base_role_byte)};
}

auto build_roman_mounted_cloak_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::cloak_make_static_attachment(roman_mounted_cloak_config(),
                                                   Render::GL::shared_cloak_meshes(),
                                                   humanoid_chest_bone(),
                                                   base_role_byte)};
}

auto build_roman_horse_saddle_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::roman_saddle_make_static_attachment(
      horse_root_bone(),
      base_role_byte,
      horse_baseline_back_center_frame(),
      horse_root_bind_matrix())};
}

auto build_carthage_horse_saddle_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::carthage_saddle_make_static_attachment(
      horse_root_bone(),
      base_role_byte,
      horse_baseline_back_center_frame(),
      horse_root_bind_matrix())};
}

auto build_light_cavalry_saddle_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::light_cavalry_saddle_make_static_attachment(
      horse_root_bone(),
      base_role_byte,
      horse_baseline_back_center_frame(),
      horse_root_bind_matrix())};
}

auto build_horse_bridle_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::bridle_make_static_attachment(horse_head_bone(),
                                                    base_role_byte,
                                                    horse_baseline_head_frame(),
                                                    horse_head_bind_matrix())};
}

auto build_horse_reins_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::reins_make_static_attachment(horse_root_bone(),
                                                   base_role_byte,
                                                   horse_baseline_back_center_frame(),
                                                   horse_root_bind_matrix())};
}

auto build_horse_blanket_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::blanket_make_static_attachment(horse_root_bone(),
                                                     base_role_byte,
                                                     horse_baseline_back_center_frame(),
                                                     horse_root_bind_matrix())};
}

auto build_leather_barding_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::leather_barding_make_static_attachment(
          Render::GL::leather_barding_chest_archetype(),
          horse_root_bone(),
          base_role_byte,
          horse_baseline_chest_frame(),
          horse_root_bind_matrix()),
      Render::GL::leather_barding_make_static_attachment(
          Render::GL::leather_barding_barrel_archetype(),
          horse_root_bone(),
          base_role_byte,
          horse_baseline_barrel_frame(),
          horse_root_bind_matrix()),
  };
}

auto build_scale_barding_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::scale_barding_make_static_attachment(
          Render::GL::scale_barding_chest_archetype(),
          horse_root_bone(),
          base_role_byte,
          horse_baseline_chest_frame(),
          horse_root_bind_matrix()),
      Render::GL::scale_barding_make_static_attachment(
          Render::GL::scale_barding_barrel_archetype(),
          horse_root_bone(),
          base_role_byte,
          horse_baseline_barrel_frame(),
          horse_root_bind_matrix()),
      Render::GL::scale_barding_make_static_attachment(
          Render::GL::scale_barding_neck_archetype(),
          horse_neck_top_bone(),
          base_role_byte,
          horse_baseline_neck_base_frame(),
          horse_neck_top_bind_matrix()),
  };
}

auto build_champion_barding_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::champion_make_static_attachment(horse_root_bone(),
                                                      base_role_byte,
                                                      horse_baseline_chest_frame(),
                                                      horse_root_bind_matrix())};
}

auto build_crupper_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::crupper_make_static_attachment(horse_root_bone(),
                                                     base_role_byte,
                                                     horse_baseline_rump_frame(),
                                                     horse_root_bind_matrix())};
}

auto build_saddle_bag_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::saddle_bag_make_static_attachment(horse_root_bone(),
                                                    base_role_byte,
                                                    horse_baseline_back_center_frame(),
                                                    horse_root_bind_matrix())};
}

auto with_variant_palette(
    const void* variant_void,
    const std::function<std::uint32_t(
        const HumanoidVariant&, QVector3D*, std::uint32_t, std::size_t)>& fn,
    QVector3D* out,
    std::uint32_t base_count,
    std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  auto clamp_color = [](const QVector3D& color) -> QVector3D {
    return {std::clamp(color.x(), 0.0F, 1.0F),
            std::clamp(color.y(), 0.0F, 1.0F),
            std::clamp(color.z(), 0.0F, 1.0F)};
  };
  auto hash01 = [](float value) -> float {
    return value - std::floor(value);
  };
  auto worn_equipment_color = [&](const HumanoidVariant& variant,
                                  const QVector3D& base,
                                  std::uint32_t slot_index) -> QVector3D {
    float const max_component = std::max({base.x(), base.y(), base.z()});
    float const min_component = std::min({base.x(), base.y(), base.z()});
    float const brightness = (base.x() + base.y() + base.z()) / 3.0F;
    float const saturation = max_component - min_component;
    float const grayscale = brightness;
    float const metallicness =
        std::clamp((max_component - 0.22F) * 1.6F, 0.0F, 1.0F) *
        (1.0F - std::clamp((saturation - 0.08F) * 2.8F, 0.0F, 1.0F));
    float const leatherness =
        (1.0F - metallicness) *
        std::clamp((base.x() - base.z()) * 1.6F + 0.15F, 0.0F, 1.0F);
    float const clothness = std::clamp(1.0F - metallicness * 0.75F, 0.0F, 1.0F);

    float const seed =
        variant.pattern_seed * 131.0F + static_cast<float>(slot_index) * 17.0F;
    float const wear_noise = hash01(std::sin(seed + 0.37F) * 43758.5453F);
    float const grime_noise = hash01(std::sin(seed * 1.73F + 1.91F) * 24634.6345F);
    float const blood_noise = hash01(std::sin(seed * 2.17F + 5.13F) * 15384.1837F);
    float const fade_noise = hash01(std::sin(seed * 2.81F + 0.83F) * 31415.9265F);
    float const contrast_noise = hash01(std::sin(seed * 3.71F + 2.63F) * 27182.8183F);

    float const wear =
        std::clamp(variant.weathering * (0.75F + 0.55F * wear_noise), 0.0F, 1.0F);
    float const grime =
        std::clamp(variant.grime * (0.65F + 0.65F * grime_noise), 0.0F, 1.0F);
    float const blood = std::clamp(
        variant.bloodiness * std::max(0.0F, blood_noise - 0.58F) / 0.42F, 0.0F, 1.0F);
    float const fading = wear * (0.45F + 0.55F * fade_noise);
    float const slot_variation = 0.82F + 0.36F * contrast_noise;

    QVector3D color = base;
    QVector3D const grayscale_vec(grayscale, grayscale, grayscale);
    color *= QVector3D(slot_variation, slot_variation, slot_variation);
    color = color * (1.0F - wear * (0.18F + 0.18F * clothness + 0.10F * leatherness));
    color += (grayscale_vec - color) * (wear * (0.28F + 0.28F * metallicness));
    color += (color * QVector3D(0.78F, 0.72F, 0.62F) - color) *
             (fading * (0.30F * clothness + 0.12F * leatherness));
    color *= 1.0F - grime * (0.24F + 0.18F * leatherness + 0.16F * clothness);
    color += (color * QVector3D(0.56F, 0.72F, 0.52F) - color) *
             (wear * metallicness * 0.34F);
    color += (color * QVector3D(0.70F, 0.54F, 0.40F) - color) *
             (grime * leatherness * 0.28F);
    color += (QVector3D(0.45F, 0.08F, 0.06F) - color) *
             (blood * (0.26F + 0.50F * clothness + 0.22F * leatherness));
    return clamp_color(color);
  };

  auto const& variant = *static_cast<const HumanoidVariant*>(variant_void);
  std::uint32_t const count = fn(variant, out, base_count, max_count);
  for (std::uint32_t i = base_count; i < count; ++i) {
    out[i] = worn_equipment_color(variant, out[i], i - base_count);
  }
  return count;
}

auto with_horse_variant(
    const void* variant_void,
    const std::function<
        std::uint32_t(const HorseVariant&, QVector3D*, std::uint32_t, std::size_t)>& fn,
    QVector3D* out,
    std::uint32_t base_count,
    std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  return fn(
      *static_cast<const HorseVariant*>(variant_void), out, base_count, max_count);
}

auto roman_horse_saddle_role_colors(const void* variant_void,
                                    QVector3D* out,
                                    std::uint32_t base_count,
                                    std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::roman_saddle_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto carthage_horse_saddle_role_colors(const void* variant_void,
                                       QVector3D* out,
                                       std::uint32_t base_count,
                                       std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::carthage_saddle_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto light_cavalry_saddle_role_colors(const void* variant_void,
                                      QVector3D* out,
                                      std::uint32_t base_count,
                                      std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::light_cavalry_saddle_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto horse_bridle_role_colors(const void* variant_void,
                              QVector3D* out,
                              std::uint32_t base_count,
                              std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::bridle_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto horse_reins_role_colors(const void* variant_void,
                             QVector3D* out,
                             std::uint32_t base_count,
                             std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count +
               Render::GL::reins_fill_role_colors(variant, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto horse_blanket_role_colors(const void* variant_void,
                               QVector3D* out,
                               std::uint32_t base_count,
                               std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::blanket_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto leather_barding_role_colors(const void* variant_void,
                                 QVector3D* out,
                                 std::uint32_t base_count,
                                 std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::leather_barding_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto scale_barding_role_colors(const void* variant_void,
                               QVector3D* out,
                               std::uint32_t base_count,
                               std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::scale_barding_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto champion_barding_role_colors(const void* variant_void,
                                  QVector3D* out,
                                  std::uint32_t base_count,
                                  std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::champion_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto horse_crupper_role_colors(const void* variant_void,
                               QVector3D* out,
                               std::uint32_t base_count,
                               std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::crupper_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto saddle_bag_role_colors(const void* variant_void,
                            QVector3D* out,
                            std::uint32_t base_count,
                            std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::saddle_bag_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto carthage_light_helmet_role_colors(const void* variant_void,
                                       QVector3D* out,
                                       std::uint32_t base_count,
                                       std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::carthage_light_helmet_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto carthage_heavy_helmet_role_colors(const void* variant_void,
                                       QVector3D* out,
                                       std::uint32_t base_count,
                                       std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::carthage_heavy_helmet_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto roman_heavy_helmet_role_colors(const void* variant_void,
                                    QVector3D* out,
                                    std::uint32_t base_count,
                                    std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::roman_heavy_helmet_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto roman_light_helmet_role_colors(const void* variant_void,
                                    QVector3D* out,
                                    std::uint32_t base_count,
                                    std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::roman_light_helmet_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto roman_light_armor_role_colors(const void* variant_void,
                                   QVector3D* out,
                                   std::uint32_t base_count,
                                   std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::roman_light_armor_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto roman_heavy_armor_role_colors(const void* variant_void,
                                   QVector3D* out,
                                   std::uint32_t base_count,
                                   std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::roman_heavy_armor_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto carthage_light_armor_role_colors(const void* variant_void,
                                      QVector3D* out,
                                      std::uint32_t base_count,
                                      std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::armor_light_carthage_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto carthage_heavy_armor_role_colors(const void* variant_void,
                                      QVector3D* out,
                                      std::uint32_t base_count,
                                      std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::armor_heavy_carthage_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto roman_greaves_role_colors(const void* variant_void,
                               QVector3D* out,
                               std::uint32_t base_count,
                               std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::roman_greaves_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto roman_shoulder_role_colors(const void* variant_void,
                                QVector3D* out,
                                std::uint32_t base_count,
                                std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::roman_shoulder_cover_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto carthage_shoulder_role_colors(const void* variant_void,
                                   QVector3D* out,
                                   std::uint32_t base_count,
                                   std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::carthage_shoulder_cover_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto spear_role_colors(const void* variant_void,
                       QVector3D* out,
                       std::uint32_t base_count,
                       std::size_t max_count) -> std::uint32_t {
  static const SpearRenderConfig config = []() {
    SpearRenderConfig cfg;
    cfg.shaft_color = QVector3D(0.5F, 0.3F, 0.2F) * QVector3D(0.85F, 0.75F, 0.65F);
    cfg.spearhead_color = QVector3D(0.75F, 0.76F, 0.80F);
    cfg.spear_length = 1.15F;
    cfg.shaft_radius = 0.018F;
    cfg.spearhead_length = 0.16F;
    return cfg;
  }();
  return with_variant_palette(
      variant_void,
      [&](const HumanoidVariant& variant,
          QVector3D* colors,
          std::uint32_t count,
          std::size_t max) {
        return count + Render::GL::spear_fill_role_colors(
                           variant.palette, config, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto fabius_spear_role_colors(const void* variant_void,
                              QVector3D* out,
                              std::uint32_t base_count,
                              std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [&](const HumanoidVariant& variant,
          QVector3D* colors,
          std::uint32_t count,
          std::size_t max) {
        return count +
               Render::GL::spear_fill_role_colors(
                   variant.palette, fabius_spear_config(), colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto hanno_spear_role_colors(const void* variant_void,
                             QVector3D* out,
                             std::uint32_t base_count,
                             std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [&](const HumanoidVariant& variant,
          QVector3D* colors,
          std::uint32_t count,
          std::size_t max) {
        return count +
               Render::GL::spear_fill_role_colors(
                   variant.palette, hanno_spear_config(), colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto roman_scutum_role_colors(const void* variant_void,
                              QVector3D* out,
                              std::uint32_t base_count,
                              std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::roman_scutum_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto carthage_shield_role_colors(const void* variant_void,
                                 QVector3D* out,
                                 std::uint32_t base_count,
                                 std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::carthage_shield_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto roman_sword_role_colors(const void* variant_void,
                             QVector3D* out,
                             std::uint32_t base_count,
                             std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        count += Render::GL::sword_fill_role_colors(
            variant.palette, roman_sword_config(), colors + count, max - count);
        if (max <= count) {
          return count;
        }
        return count + Render::GL::scabbard_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto scipio_sword_role_colors(const void* variant_void,
                              QVector3D* out,
                              std::uint32_t base_count,
                              std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        count += Render::GL::sword_fill_role_colors(
            variant.palette, scipio_sword_config(), colors + count, max - count);
        if (max <= count) {
          return count;
        }
        return count + Render::GL::scabbard_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto carthage_sword_role_colors(const void* variant_void,
                                QVector3D* out,
                                std::uint32_t base_count,
                                std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        count += Render::GL::sword_fill_role_colors(
            variant.palette, carthage_sword_config(), colors + count, max - count);
        if (max <= count) {
          return count;
        }
        return count + Render::GL::scabbard_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto hannibal_sword_role_colors(const void* variant_void,
                                QVector3D* out,
                                std::uint32_t base_count,
                                std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        count += Render::GL::sword_fill_role_colors(
            variant.palette, hannibal_sword_config(), colors + count, max - count);
        if (max <= count) {
          return count;
        }
        return count + Render::GL::scabbard_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto sepulcher_sword_role_colors(const void* variant_void,
                                 QVector3D* out,
                                 std::uint32_t base_count,
                                 std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::sword_fill_role_colors(variant.palette,
                                                          sepulcher_sword_config(),
                                                          colors + count,
                                                          max - count);
      },
      out,
      base_count,
      max_count);
}

auto bow_role_colors(const void* variant_void,
                     QVector3D* out,
                     std::uint32_t base_count,
                     std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::bow_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto marcellus_bow_role_colors(const void* variant_void,
                               QVector3D* out,
                               std::uint32_t base_count,
                               std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant&,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        if (max - count < Render::GL::k_bow_role_count) {
          return count;
        }
        colors[count] = QVector3D(0.07F, 0.04F, 0.02F);
        colors[count + 1] = marcellus_bow_config().string_color;
        return count + static_cast<std::uint32_t>(Render::GL::k_bow_role_count);
      },
      out,
      base_count,
      max_count);
}

auto hasdrubal_bow_role_colors(const void* variant_void,
                               QVector3D* out,
                               std::uint32_t base_count,
                               std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant&,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        if (max - count < Render::GL::k_bow_role_count) {
          return count;
        }
        colors[count] = QVector3D(0.09F, 0.05F, 0.02F);
        colors[count + 1] = hasdrubal_bow_config().string_color;
        return count + static_cast<std::uint32_t>(Render::GL::k_bow_role_count);
      },
      out,
      base_count,
      max_count);
}

auto quiver_role_colors(const void* variant_void,
                        QVector3D* out,
                        std::uint32_t base_count,
                        std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::quiver_fill_role_colors(variant.palette,
                                                           mounted_quiver_config(),
                                                           colors + count,
                                                           max - count);
      },
      out,
      base_count,
      max_count);
}

auto marcellus_quiver_role_colors(const void* variant_void,
                                  QVector3D* out,
                                  std::uint32_t base_count,
                                  std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::quiver_fill_role_colors(variant.palette,
                                                           marcellus_quiver_config(),
                                                           colors + count,
                                                           max - count);
      },
      out,
      base_count,
      max_count);
}

auto roman_quiver_role_colors(const void* variant_void,
                              QVector3D* out,
                              std::uint32_t base_count,
                              std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count +
               Render::GL::quiver_fill_role_colors(
                   variant.palette, roman_quiver_config(), colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto hasdrubal_quiver_role_colors(const void* variant_void,
                                  QVector3D* out,
                                  std::uint32_t base_count,
                                  std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::quiver_fill_role_colors(variant.palette,
                                                           hasdrubal_quiver_config(),
                                                           colors + count,
                                                           max - count);
      },
      out,
      base_count,
      max_count);
}

auto carthage_quiver_role_colors(const void* variant_void,
                                 QVector3D* out,
                                 std::uint32_t base_count,
                                 std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::quiver_fill_role_colors(variant.palette,
                                                           carthage_quiver_config(),
                                                           colors + count,
                                                           max - count);
      },
      out,
      base_count,
      max_count);
}

auto carthage_cloak_role_colors(const void* variant_void,
                                QVector3D* out,
                                std::uint32_t base_count,
                                std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::cloak_fill_role_colors_with_primary(
                           carthage_cloak_config().primary_color,
                           variant.palette,
                           colors + count,
                           max - count);
      },
      out,
      base_count,
      max_count);
}

auto sepulcher_cloak_role_colors(const void* variant_void,
                                 QVector3D* out,
                                 std::uint32_t base_count,
                                 std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::cloak_fill_role_colors_with_primary(
                           sepulcher_cloak_config().primary_color,
                           variant.palette,
                           colors + count,
                           max - count);
      },
      out,
      base_count,
      max_count);
}

auto roman_cloak_role_colors(const void* variant_void,
                             QVector3D* out,
                             std::uint32_t base_count,
                             std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::cloak_fill_role_colors_with_primary(
                           roman_cloak_config().primary_color,
                           variant.palette,
                           colors + count,
                           max - count);
      },
      out,
      base_count,
      max_count);
}

auto roman_tool_belt_role_colors(const void* variant_void,
                                 QVector3D* out,
                                 std::uint32_t base_count,
                                 std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::tool_belt_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto carthage_tool_belt_role_colors(const void* variant_void,
                                    QVector3D* out,
                                    std::uint32_t base_count,
                                    std::size_t max_count) -> std::uint32_t {
  return roman_tool_belt_role_colors(variant_void, out, base_count, max_count);
}

auto roman_work_apron_role_colors(const void* variant_void,
                                  QVector3D* out,
                                  std::uint32_t base_count,
                                  std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        if (max - count < Render::GL::k_work_apron_role_count) {
          return count;
        }
        QVector3D const apron =
            variant.palette.leather * 0.78F + variant.palette.cloth * 0.22F;
        for (std::uint32_t i = 0; i < 7U; ++i) {
          float const t = static_cast<float>(i) / 6.0F * 0.16F;
          colors[count + i] = apron * (1.0F - t);
        }
        colors[count + 7U] = apron * 0.84F;
        colors[count + 8U] = variant.palette.leather_dark;
        return count + Render::GL::k_work_apron_role_count;
      },
      out,
      base_count,
      max_count);
}

auto carthage_work_apron_role_colors(const void* variant_void,
                                     QVector3D* out,
                                     std::uint32_t base_count,
                                     std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        if (max - count < Render::GL::k_work_apron_role_count) {
          return count;
        }
        QVector3D const apron =
            variant.palette.leather * 0.74F + variant.palette.cloth * 0.26F;
        for (std::uint32_t i = 0; i < 7U; ++i) {
          float const t = static_cast<float>(i) / 6.0F * 0.18F;
          colors[count + i] = apron * (1.0F - t);
        }
        colors[count + 7U] = apron * 0.82F;
        colors[count + 8U] = variant.palette.leather_dark;
        return count + Render::GL::k_work_apron_role_count;
      },
      out,
      base_count,
      max_count);
}

auto roman_arm_guards_role_colors(const void* variant_void,
                                  QVector3D* out,
                                  std::uint32_t base_count,
                                  std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        if (max - count < Render::GL::k_arm_guards_role_count) {
          return count;
        }
        colors[count] = variant.palette.leather_dark;
        colors[count + 1U] =
            variant.palette.leather_dark * 0.78F + variant.palette.metal * 0.22F;
        return count + Render::GL::k_arm_guards_role_count;
      },
      out,
      base_count,
      max_count);
}

auto carthage_arm_guards_role_colors(const void* variant_void,
                                     QVector3D* out,
                                     std::uint32_t base_count,
                                     std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        if (max - count < Render::GL::k_arm_guards_role_count) {
          return count;
        }
        colors[count] = variant.palette.leather_dark;
        colors[count + 1U] =
            variant.palette.leather_dark * 0.80F + variant.palette.metal * 0.20F;
        return count + Render::GL::k_arm_guards_role_count;
      },
      out,
      base_count,
      max_count);
}

void register_humanoid_descriptor(EquipmentCategory category,
                                  const char* id,
                                  HumanoidEquipmentContribution contribution) {
  const auto handle = EquipmentRegistry::instance().resolve_handle(category, id);
  register_humanoid_equipment_contribution(handle, contribution);
}

void register_horse_descriptor(EquipmentCategory category,
                               const char* id,
                               HorseEquipmentContribution contribution) {
  const auto handle = EquipmentRegistry::instance().resolve_handle(category, id);
  register_horse_equipment_contribution(handle, contribution);
}

} // namespace

void register_built_in_equipment() {
  auto& registry = EquipmentRegistry::instance();
  registry.register_equipment_id(EquipmentCategory::Weapon, "bow");
  registry.register_equipment_id(EquipmentCategory::Weapon, "bow_carthage");
  registry.register_equipment_id(EquipmentCategory::Weapon, "bow_roman");
  registry.register_equipment_id(EquipmentCategory::Weapon, "bow_marcellus");
  registry.register_equipment_id(EquipmentCategory::Weapon, "bow_hasdrubal");
  registry.register_equipment_id(EquipmentCategory::Weapon, "quiver");
  registry.register_equipment_id(EquipmentCategory::Weapon, "quiver_roman");
  registry.register_equipment_id(EquipmentCategory::Weapon, "quiver_carthage");
  registry.register_equipment_id(EquipmentCategory::Weapon, "quiver_marcellus");
  registry.register_equipment_id(EquipmentCategory::Weapon, "quiver_hasdrubal");
  registry.register_equipment_id(EquipmentCategory::Weapon, "roman_scutum");
  registry.register_equipment_id(EquipmentCategory::Weapon, "sword");
  registry.register_equipment_id(EquipmentCategory::Weapon, "sword_carthage");
  registry.register_equipment_id(EquipmentCategory::Weapon, "sword_sepulcher");
  registry.register_equipment_id(EquipmentCategory::Weapon, "sword_roman");
  registry.register_equipment_id(EquipmentCategory::Weapon, "sword_scipio");
  registry.register_equipment_id(EquipmentCategory::Weapon, "sword_hannibal");
  registry.register_equipment_id(EquipmentCategory::Weapon, "spear");
  registry.register_equipment_id(EquipmentCategory::Weapon, "spear_fabius");
  registry.register_equipment_id(EquipmentCategory::Weapon, "spear_hanno");
  registry.register_equipment_id(EquipmentCategory::Weapon, "shield");
  registry.register_equipment_id(EquipmentCategory::Weapon, "shield_carthage");
  registry.register_equipment_id(EquipmentCategory::Weapon, "shield_carthage_cavalry");

  registry.register_equipment_id(EquipmentCategory::Helmet, "carthage_heavy");
  registry.register_equipment_id(EquipmentCategory::Helmet, "carthage_light");
  registry.register_equipment_id(EquipmentCategory::Helmet, "roman_heavy");
  registry.register_equipment_id(EquipmentCategory::Helmet, "roman_light");
  registry.register_equipment_id(EquipmentCategory::Helmet, "headwrap");

  registry.register_equipment_id(EquipmentCategory::Armor, "roman_heavy_armor");
  registry.register_equipment_id(EquipmentCategory::Armor, "roman_light_armor");
  registry.register_equipment_id(EquipmentCategory::Armor, "armor_light_carthage");
  registry.register_equipment_id(EquipmentCategory::Armor, "armor_heavy_carthage");
  registry.register_equipment_id(EquipmentCategory::Armor, "roman_shoulder_cover");
  registry.register_equipment_id(EquipmentCategory::Armor,
                                 "roman_shoulder_cover_cavalry");
  registry.register_equipment_id(EquipmentCategory::Armor, "roman_greaves");
  registry.register_equipment_id(EquipmentCategory::Armor, "carthage_shoulder_cover");
  registry.register_equipment_id(EquipmentCategory::Armor,
                                 "carthage_shoulder_cover_cavalry");
  registry.register_equipment_id(EquipmentCategory::Armor, "cloak_carthage");
  registry.register_equipment_id(EquipmentCategory::Armor, "cloak_carthage_mounted");
  registry.register_equipment_id(EquipmentCategory::Armor, "cloak_sepulcher");
  registry.register_equipment_id(EquipmentCategory::Armor, "cloak_roman");
  registry.register_equipment_id(EquipmentCategory::Armor, "cloak_roman_mounted");
  registry.register_equipment_id(EquipmentCategory::Armor, "work_apron_roman");
  registry.register_equipment_id(EquipmentCategory::Armor, "work_apron_carthage");
  registry.register_equipment_id(EquipmentCategory::Armor, "tool_belt_roman");
  registry.register_equipment_id(EquipmentCategory::Armor, "tool_belt_carthage");
  registry.register_equipment_id(EquipmentCategory::Armor, "arm_guards");
  registry.register_equipment_id(EquipmentCategory::Armor, "arm_guards_roman");
  registry.register_equipment_id(EquipmentCategory::Armor, "arm_guards_carthage");

  registry.register_placeholder_equipment(EquipmentCategory::Armor,
                                          "builder_tunic_roman");
  registry.register_placeholder_equipment(EquipmentCategory::Armor,
                                          "roman_civilian_mantle");
  registry.register_placeholder_equipment(EquipmentCategory::Helmet,
                                          "headwrap_carthage_civilian");
  registry.register_placeholder_equipment(EquipmentCategory::Armor, "carthage_robes");
  registry.register_placeholder_equipment(EquipmentCategory::Armor,
                                          "carthage_civilian_sash");

  registry.register_equipment_id(EquipmentCategory::HorseTack, "roman_horse_saddle");
  registry.register_equipment_id(EquipmentCategory::HorseTack, "carthage_horse_saddle");
  registry.register_equipment_id(EquipmentCategory::HorseTack, "light_cavalry_saddle");
  registry.register_equipment_id(EquipmentCategory::HorseTack, "horse_bridle");
  registry.register_equipment_id(EquipmentCategory::HorseTack, "horse_reins");
  registry.register_equipment_id(EquipmentCategory::HorseTack, "horse_blanket");
  registry.register_equipment_id(EquipmentCategory::HorseArmor,
                                 "horse_leather_barding");
  registry.register_equipment_id(EquipmentCategory::HorseArmor, "horse_scale_barding");
  registry.register_equipment_id(EquipmentCategory::HorseArmor,
                                 "horse_champion_barding");
  registry.register_equipment_id(EquipmentCategory::HorseArmor, "horse_crupper");
  registry.register_equipment_id(EquipmentCategory::HorseDecoration,
                                 "horse_saddle_bag");

  register_horse_descriptor(
      EquipmentCategory::HorseTack,
      "roman_horse_saddle",
      {.build_attachments = &build_roman_horse_saddle_attachment,
       .append_role_colors = &roman_horse_saddle_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_roman_saddle_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseTack,
      "carthage_horse_saddle",
      {.build_attachments = &build_carthage_horse_saddle_attachment,
       .append_role_colors = &carthage_horse_saddle_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_carthage_saddle_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseTack,
      "light_cavalry_saddle",
      {.build_attachments = &build_light_cavalry_saddle_attachment,
       .append_role_colors = &light_cavalry_saddle_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_light_cavalry_saddle_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseTack,
      "horse_bridle",
      {.build_attachments = &build_horse_bridle_attachment,
       .append_role_colors = &horse_bridle_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_bridle_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseTack,
      "horse_reins",
      {.build_attachments = &build_horse_reins_attachment,
       .append_role_colors = &horse_reins_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_reins_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseTack,
      "horse_blanket",
      {.build_attachments = &build_horse_blanket_attachment,
       .append_role_colors = &horse_blanket_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_blanket_role_count)});
  register_horse_descriptor(EquipmentCategory::HorseArmor,
                            "horse_leather_barding",
                            {.build_attachments = &build_leather_barding_attachments,
                             .append_role_colors = &leather_barding_role_colors,
                             .role_count = static_cast<std::uint8_t>(
                                 Render::GL::k_leather_barding_role_count)});
  register_horse_descriptor(EquipmentCategory::HorseArmor,
                            "horse_scale_barding",
                            {.build_attachments = &build_scale_barding_attachments,
                             .append_role_colors = &scale_barding_role_colors,
                             .role_count = static_cast<std::uint8_t>(
                                 Render::GL::k_scale_barding_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseArmor,
      "horse_champion_barding",
      {.build_attachments = &build_champion_barding_attachment,
       .append_role_colors = &champion_barding_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_champion_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseArmor,
      "horse_crupper",
      {.build_attachments = &build_crupper_attachment,
       .append_role_colors = &horse_crupper_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_crupper_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseDecoration,
      "horse_saddle_bag",
      {.build_attachments = &build_saddle_bag_attachment,
       .append_role_colors = &saddle_bag_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_saddle_bag_role_count)});

  register_humanoid_descriptor(
      EquipmentCategory::Helmet,
      "carthage_light",
      {.build_attachments = &build_carthage_light_helmet_attachments,
       .append_role_colors = &carthage_light_helmet_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_carthage_light_helmet_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Helmet,
      "carthage_heavy",
      {.build_attachments = &build_carthage_heavy_helmet_attachments,
       .append_role_colors = &carthage_heavy_helmet_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_carthage_heavy_helmet_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Helmet,
      "roman_heavy",
      {.build_attachments = &build_roman_heavy_helmet_attachment,
       .append_role_colors = &roman_heavy_helmet_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_roman_heavy_helmet_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Helmet,
      "roman_light",
      {.build_attachments = &build_roman_light_helmet_attachment,
       .append_role_colors = &roman_light_helmet_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_roman_light_helmet_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Helmet,
      "headwrap",
      {.build_attachments = &build_headwrap_attachment,
       .append_role_colors = [](const void* variant_void,
                                QVector3D* out,
                                std::uint32_t base_count,
                                std::size_t max_count) -> std::uint32_t {
         return with_variant_palette(
             variant_void,
             [](const HumanoidVariant& variant,
                QVector3D* colors,
                std::uint32_t count,
                std::size_t max) {
               return count + Render::GL::headwrap_fill_role_colors(
                                  variant.palette, colors + count, max - count);
             },
             out,
             base_count,
             max_count);
       },
       .role_count = static_cast<std::uint8_t>(Render::GL::k_headwrap_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "roman_light_armor",
      {.build_attachments = &build_roman_light_armor_attachment,
       .append_role_colors = &roman_light_armor_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_roman_light_armor_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "roman_heavy_armor",
      {.build_attachments = &build_roman_heavy_armor_attachment,
       .append_role_colors = &roman_heavy_armor_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_roman_heavy_armor_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "armor_light_carthage",
      {.build_attachments = &build_carthage_light_armor_attachment,
       .append_role_colors = &carthage_light_armor_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_armor_light_carthage_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "armor_heavy_carthage",
      {.build_attachments = &build_carthage_heavy_armor_attachment,
       .append_role_colors = &carthage_heavy_armor_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_armor_heavy_carthage_role_count)});
  register_humanoid_descriptor(EquipmentCategory::Armor,
                               "roman_greaves",
                               {.build_attachments = &build_roman_greaves_attachments,
                                .append_role_colors = &roman_greaves_role_colors,
                                .role_count = static_cast<std::uint8_t>(
                                    Render::GL::k_roman_greaves_role_count)});
  register_humanoid_descriptor(EquipmentCategory::Armor,
                               "roman_shoulder_cover",
                               {.build_attachments = &build_roman_shoulder_attachments,
                                .append_role_colors = &roman_shoulder_role_colors,
                                .role_count = static_cast<std::uint8_t>(
                                    Render::GL::k_roman_shoulder_cover_role_count)});
  register_humanoid_descriptor(EquipmentCategory::Armor,
                               "roman_shoulder_cover_cavalry",
                               {.build_attachments = &build_roman_shoulder_attachments,
                                .append_role_colors = &roman_shoulder_role_colors,
                                .role_count = static_cast<std::uint8_t>(
                                    Render::GL::k_roman_shoulder_cover_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "carthage_shoulder_cover",
      {.build_attachments = &build_carthage_shoulder_attachments,
       .append_role_colors = &carthage_shoulder_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_carthage_shoulder_cover_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "carthage_shoulder_cover_cavalry",
      {.build_attachments = &build_carthage_shoulder_attachments,
       .append_role_colors = &carthage_shoulder_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_carthage_shoulder_cover_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "spear",
      {.build_attachments = &build_spear_attachments,
       .append_role_colors = &spear_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_spear_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "spear_fabius",
      {.build_attachments = &build_fabius_spear_attachments,
       .append_role_colors = &fabius_spear_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_spear_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "spear_hanno",
      {.build_attachments = &build_hanno_spear_attachments,
       .append_role_colors = &hanno_spear_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_spear_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "roman_scutum",
      {.build_attachments = &build_roman_scutum_attachment,
       .append_role_colors = &roman_scutum_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_roman_scutum_role_count)});
  register_humanoid_descriptor(EquipmentCategory::Weapon,
                               "shield_carthage",
                               {.build_attachments = &build_carthage_shield_attachment,
                                .append_role_colors = &carthage_shield_role_colors,
                                .role_count = static_cast<std::uint8_t>(
                                    Render::GL::k_carthage_shield_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "sword_roman",
      {.build_attachments = &build_roman_sword_attachments,
       .append_role_colors = &roman_sword_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_sword_role_count +
                                               Render::GL::k_scabbard_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "sword_scipio",
      {.build_attachments = &build_scipio_sword_attachments,
       .append_role_colors = &scipio_sword_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_sword_role_count +
                                               Render::GL::k_scabbard_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "sword_carthage",
      {.build_attachments = &build_carthage_sword_attachments,
       .append_role_colors = &carthage_sword_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_sword_role_count +
                                               Render::GL::k_scabbard_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "sword_hannibal",
      {.build_attachments = &build_hannibal_sword_attachments,
       .append_role_colors = &hannibal_sword_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_sword_role_count +
                                               Render::GL::k_scabbard_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "sword_sepulcher",
      {.build_attachments = &build_sepulcher_sword_attachments,
       .append_role_colors = &sepulcher_sword_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_sword_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "bow_roman",
      {.build_attachments = &build_roman_bow_attachments,
       .append_role_colors = &bow_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_bow_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "bow_marcellus",
      {.build_attachments = &build_marcellus_bow_attachments,
       .append_role_colors = &marcellus_bow_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_bow_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "bow_carthage",
      {.build_attachments = &build_carthage_bow_attachments,
       .append_role_colors = &bow_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_bow_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "bow_hasdrubal",
      {.build_attachments = &build_hasdrubal_bow_attachments,
       .append_role_colors = &hasdrubal_bow_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_bow_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "quiver",
      {.build_attachments = &build_quiver_attachments,
       .append_role_colors = &quiver_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_quiver_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "quiver_roman",
      {.build_attachments = &build_roman_quiver_attachments,
       .append_role_colors = &roman_quiver_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_quiver_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "quiver_marcellus",
      {.build_attachments = &build_marcellus_quiver_attachments,
       .append_role_colors = &marcellus_quiver_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_quiver_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "quiver_carthage",
      {.build_attachments = &build_carthage_quiver_attachments,
       .append_role_colors = &carthage_quiver_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_quiver_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "quiver_hasdrubal",
      {.build_attachments = &build_hasdrubal_quiver_attachments,
       .append_role_colors = &hasdrubal_quiver_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_quiver_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "cloak_carthage",
      {.build_attachments = &build_carthage_cloak_attachment,
       .append_role_colors = &carthage_cloak_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_cloak_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "cloak_carthage_mounted",
      {.build_attachments = &build_carthage_mounted_cloak_attachment,
       .append_role_colors = &carthage_cloak_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_cloak_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "cloak_sepulcher",
      {.build_attachments = &build_sepulcher_cloak_attachment,
       .append_role_colors = &sepulcher_cloak_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_cloak_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "cloak_roman",
      {.build_attachments = &build_roman_cloak_attachment,
       .append_role_colors = &roman_cloak_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_cloak_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "cloak_roman_mounted",
      {.build_attachments = &build_roman_mounted_cloak_attachment,
       .append_role_colors = &roman_cloak_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_cloak_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "tool_belt_roman",
      {.build_attachments = &build_tool_belt_attachments,
       .append_role_colors = &roman_tool_belt_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_tool_belt_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "tool_belt_carthage",
      {.build_attachments = &build_tool_belt_attachments,
       .append_role_colors = &carthage_tool_belt_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_tool_belt_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "work_apron_roman",
      {.build_attachments = &build_work_apron_attachments,
       .append_role_colors = &roman_work_apron_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_work_apron_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "work_apron_carthage",
      {.build_attachments = &build_work_apron_attachments,
       .append_role_colors = &carthage_work_apron_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_work_apron_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "arm_guards_roman",
      {.build_attachments = &build_arm_guards_attachments,
       .append_role_colors = &roman_arm_guards_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_arm_guards_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "arm_guards_carthage",
      {.build_attachments = &build_arm_guards_attachments,
       .append_role_colors = &carthage_arm_guards_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_arm_guards_role_count)});

  auto& ar = RenderArchetypeRegistry::instance();

  ar.register_archetype("roman_light_helmet",
                        [] { (void)roman_light_helmet_archetype(); });
  ar.register_archetype("roman_heavy_helmet",
                        [] { (void)roman_heavy_helmet_archetype(); });
  ar.register_archetype("headwrap_helmet", [] { (void)headwrap_helmet_archetype(); });
  ar.register_archetype("carthage_light_helmet_shell",
                        [] { (void)carthage_light_helmet_shell_archetype(); });
  ar.register_archetype("carthage_light_helmet_neck_guard",
                        [] { (void)carthage_light_helmet_neck_guard_archetype(); });
  ar.register_archetype("carthage_light_helmet_cheek_guards",
                        [] { (void)carthage_light_helmet_cheek_guards_archetype(); });
  ar.register_archetype("carthage_light_helmet_nasal_guard",
                        [] { (void)carthage_light_helmet_nasal_guard_archetype(); });
  ar.register_archetype("carthage_light_helmet_crest_low",
                        [] { (void)carthage_light_helmet_crest_archetype(false); });
  ar.register_archetype("carthage_light_helmet_crest_high",
                        [] { (void)carthage_light_helmet_crest_archetype(true); });
  ar.register_archetype("carthage_light_helmet_rivets",
                        [] { (void)carthage_light_helmet_rivets_archetype(); });
  ar.register_archetype("carthage_heavy_helmet_shell",
                        [] { (void)carthage_heavy_helmet_shell_archetype(); });
  ar.register_archetype("carthage_heavy_helmet_neck_guard",
                        [] { (void)carthage_heavy_helmet_neck_guard_archetype(); });
  ar.register_archetype("carthage_heavy_helmet_cheek_guards",
                        [] { (void)carthage_heavy_helmet_cheek_guards_archetype(); });
  ar.register_archetype("carthage_heavy_helmet_face_plate",
                        [] { (void)carthage_heavy_helmet_face_plate_archetype(); });
  ar.register_archetype("carthage_heavy_helmet_crest",
                        [] { (void)carthage_heavy_helmet_crest_archetype(); });
  ar.register_archetype("carthage_heavy_helmet_rivets",
                        [] { (void)carthage_heavy_helmet_rivets_archetype(); });

  ar.register_archetype("roman_shoulder_cover",
                        [] { (void)roman_shoulder_cover_archetype(); });
  ar.register_archetype("carthage_shoulder_cover",
                        [] { (void)carthage_shoulder_cover_archetype(); });
  ar.register_archetype("roman_greaves", [] { (void)roman_greaves_archetype(); });

  ar.register_archetype("roman_scutum", [] { (void)roman_scutum_archetype(); });

  ar.register_archetype("tool_belt_ring", [] { (void)tool_belt_ring_archetype(); });
  ar.register_archetype("tool_belt_buckle", [] { (void)tool_belt_buckle_archetype(); });
  ar.register_archetype("tool_belt_hammer", [] { (void)tool_belt_hammer_archetype(); });
  ar.register_archetype("tool_belt_chisel", [] { (void)tool_belt_chisel_archetype(); });
  ar.register_archetype("tool_belt_saw", [] { (void)tool_belt_saw_archetype(); });
  ar.register_archetype("tool_belt_pouches",
                        [] { (void)tool_belt_pouches_archetype(); });

  ar.register_archetype("reins", [] { (void)reins_archetype(); });
  ar.register_archetype("blanket", [] { (void)blanket_archetype(); });
  ar.register_archetype("bridle", [] { (void)bridle_archetype(); });

  ar.register_archetype("roman_saddle", [] { (void)roman_saddle_archetype(); });
  ar.register_archetype("carthage_saddle", [] { (void)carthage_saddle_archetype(); });
  ar.register_archetype("light_cavalry_saddle",
                        [] { (void)light_cavalry_saddle_archetype(); });

  ar.register_archetype("champion_barding", [] { (void)champion_archetype(); });
  ar.register_archetype("crupper", [] { (void)crupper_archetype(); });
  ar.register_archetype("leather_barding_chest",
                        [] { (void)leather_barding_chest_archetype(); });
  ar.register_archetype("leather_barding_barrel",
                        [] { (void)leather_barding_barrel_archetype(); });
  ar.register_archetype("scale_barding_chest",
                        [] { (void)scale_barding_chest_archetype(); });
  ar.register_archetype("scale_barding_barrel",
                        [] { (void)scale_barding_barrel_archetype(); });
  ar.register_archetype("scale_barding_neck",
                        [] { (void)scale_barding_neck_archetype(); });

  ar.register_archetype("saddle_bag", [] { (void)saddle_bag_archetype(); });
}

} // namespace Render::GL
