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
#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace Render::GL {
namespace {

using StaticAttachmentSpec = Render::Creature::StaticAttachmentSpec;

template <std::size_t N>
auto to_vector(const std::array<StaticAttachmentSpec, N> &attachments)
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

auto humanoid_head_bind_matrix() -> const QMatrix4x4 & {
  static const QMatrix4x4 matrix =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
          Render::Humanoid::HumanoidBone::Head)];
  return matrix;
}

auto humanoid_shoulder_bind_matrix(bool left) -> const QMatrix4x4 & {
  const auto &bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  static const QMatrix4x4 left_matrix =
      Render::GL::make_shoulder_cover_transform(
          QMatrix4x4{}, bind_frames.shoulder_l.origin, -bind_frames.torso.right,
          bind_frames.torso.up);
  static const QMatrix4x4 right_matrix =
      Render::GL::make_shoulder_cover_transform(
          QMatrix4x4{}, bind_frames.shoulder_r.origin, bind_frames.torso.right,
          bind_frames.torso.up);
  return left ? left_matrix : right_matrix;
}

auto humanoid_shin_bind_matrix(bool left) -> const QMatrix4x4 & {
  const auto &bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  static const auto frame_to_matrix = [](const Render::GL::AttachmentFrame &f) {
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

auto horse_root_bind_matrix() -> const QMatrix4x4 & {
  static const QMatrix4x4 matrix =
      Render::Horse::horse_bind_palette()[static_cast<std::size_t>(
          Render::Horse::HorseBone::Root)];
  return matrix;
}

auto horse_head_bind_matrix() -> const QMatrix4x4 & {
  static const QMatrix4x4 matrix =
      Render::Horse::horse_bind_palette()[static_cast<std::size_t>(
          Render::Horse::HorseBone::Head)];
  return matrix;
}

auto horse_neck_top_bind_matrix() -> const QMatrix4x4 & {
  static const QMatrix4x4 matrix =
      Render::Horse::horse_bind_palette()[static_cast<std::size_t>(
          Render::Horse::HorseBone::NeckTop)];
  return matrix;
}

auto roman_bow_config() -> const BowRenderConfig & {
  static const BowRenderConfig config = []() {
    BowRenderConfig cfg;
    cfg.bow_depth = 0.22F;
    cfg.bow_curve_factor = 1.0F;
    cfg.bow_height_scale = 1.0F;
    cfg.bow_top_y = HumanProportions::SHOULDER_Y + 0.55F;
    cfg.bow_bot_y = HumanProportions::WAIST_Y - 0.25F;
    return cfg;
  }();
  return config;
}

auto carthage_bow_config() -> const BowRenderConfig & {
  static const BowRenderConfig config = []() {
    BowRenderConfig cfg;
    cfg.bow_depth = 0.28F;
    cfg.bow_curve_factor = 1.2F;
    cfg.bow_height_scale = 0.95F;
    cfg.bow_top_y = HumanProportions::SHOULDER_Y + 0.55F;
    cfg.bow_bot_y = HumanProportions::WAIST_Y - 0.25F;
    return cfg;
  }();
  return config;
}

auto mounted_quiver_config() -> const QuiverRenderConfig & {
  static const QuiverRenderConfig config = []() {
    QuiverRenderConfig cfg;
    cfg.fletching_color = QVector3D(0.85F, 0.40F, 0.40F);
    cfg.quiver_radius = HumanProportions::HEAD_RADIUS * 0.45F;
    return cfg;
  }();
  return config;
}

auto roman_quiver_config() -> const QuiverRenderConfig & {
  static const QuiverRenderConfig config{};
  return config;
}

auto carthage_quiver_config() -> const QuiverRenderConfig & {
  static const QuiverRenderConfig config = []() {
    QuiverRenderConfig cfg;
    cfg.fletching_color = QVector3D(0.90F, 0.82F, 0.28F);
    cfg.quiver_radius = HumanProportions::HEAD_RADIUS * 0.45F;
    return cfg;
  }();
  return config;
}

auto roman_sword_config() -> const SwordRenderConfig & {
  static const SwordRenderConfig config = []() {
    SwordRenderConfig cfg;
    cfg.sword_length = 0.80F;
    cfg.sword_width = 0.060F;
    cfg.guard_half_width = 0.120F;
    cfg.handle_radius = 0.016F;
    cfg.pommel_radius = 0.045F;
    cfg.blade_ricasso = 0.14F;
    cfg.material_id = 3;
    return cfg;
  }();
  return config;
}

auto carthage_sword_config() -> const SwordRenderConfig & {
  static const SwordRenderConfig config = []() {
    SwordRenderConfig cfg;
    cfg.metal_color = QVector3D(0.70F, 0.68F, 0.52F);
    cfg.sword_length = 0.80F;
    cfg.sword_width = 0.060F;
    cfg.guard_half_width = 0.120F;
    cfg.handle_radius = 0.016F;
    cfg.pommel_radius = 0.045F;
    cfg.blade_ricasso = 0.14F;
    cfg.material_id = 3;
    return cfg;
  }();
  return config;
}

constexpr float k_scabbard_radius = 0.060F * 0.85F;

auto cloak_meshes_for(const char *id) -> CloakMeshes {
  auto &registry = EquipmentRegistry::instance();
  const auto handle = registry.resolve_handle(EquipmentCategory::Armor, id);
  auto renderer = registry.get(handle);
  if (renderer == nullptr) {
    return {};
  }
  if (const auto *cloak = dynamic_cast<CloakRenderer *>(renderer.get())) {
    return cloak->meshes();
  }
  return {};
}

auto carthage_cloak_config() -> const CloakConfig & {
  static const CloakConfig config = []() {
    CloakConfig cfg;
    cfg.primary_color = QVector3D(0.14F, 0.38F, 0.54F);
    cfg.trim_color = QVector3D(0.75F, 0.66F, 0.42F);
    return cfg;
  }();
  return config;
}

auto roman_cloak_config() -> const CloakConfig & {
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

auto build_carthage_light_helmet_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_shell_archetype(),
          humanoid_head_bone(), base_role_byte, humanoid_head_bind_matrix()),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_neck_guard_archetype(),
          humanoid_head_bone(), base_role_byte, humanoid_head_bind_matrix()),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_cheek_guards_archetype(),
          humanoid_head_bone(), base_role_byte, humanoid_head_bind_matrix()),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_nasal_guard_archetype(),
          humanoid_head_bone(), base_role_byte, humanoid_head_bind_matrix()),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_crest_archetype(true),
          humanoid_head_bone(), base_role_byte, humanoid_head_bind_matrix()),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_rivets_archetype(),
          humanoid_head_bone(), base_role_byte, humanoid_head_bind_matrix()),
  };
}

auto build_carthage_heavy_helmet_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::carthage_heavy_helmet_make_static_attachment(
          Render::GL::carthage_heavy_helmet_shell_archetype(),
          humanoid_head_bone(), base_role_byte, humanoid_head_bind_matrix()),
      Render::GL::carthage_heavy_helmet_make_static_attachment(
          Render::GL::carthage_heavy_helmet_neck_guard_archetype(),
          humanoid_head_bone(), base_role_byte, humanoid_head_bind_matrix()),
      Render::GL::carthage_heavy_helmet_make_static_attachment(
          Render::GL::carthage_heavy_helmet_cheek_guards_archetype(),
          humanoid_head_bone(), base_role_byte, humanoid_head_bind_matrix()),
      Render::GL::carthage_heavy_helmet_make_static_attachment(
          Render::GL::carthage_heavy_helmet_face_plate_archetype(),
          humanoid_head_bone(), base_role_byte, humanoid_head_bind_matrix()),
      Render::GL::carthage_heavy_helmet_make_static_attachment(
          Render::GL::carthage_heavy_helmet_crest_archetype(),
          humanoid_head_bone(), base_role_byte, humanoid_head_bind_matrix()),
      Render::GL::carthage_heavy_helmet_make_static_attachment(
          Render::GL::carthage_heavy_helmet_rivets_archetype(),
          humanoid_head_bone(), base_role_byte, humanoid_head_bind_matrix()),
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

auto build_roman_light_armor_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::roman_light_armor_make_static_attachment(
      humanoid_chest_bone(), base_role_byte)};
}

auto build_roman_heavy_armor_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::roman_heavy_armor_make_static_attachment(
      humanoid_chest_bone(), base_role_byte)};
}

auto build_carthage_light_armor_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::armor_light_carthage_make_static_attachment(
      humanoid_chest_bone(), base_role_byte)};
}

auto build_carthage_heavy_armor_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::armor_heavy_carthage_make_static_attachment(
      humanoid_chest_bone(), base_role_byte)};
}

auto build_roman_greaves_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::roman_greaves_make_static_attachment(
          humanoid_foot_l_bone(), base_role_byte,
          humanoid_shin_bind_matrix(true)),
      Render::GL::roman_greaves_make_static_attachment(
          humanoid_foot_r_bone(), base_role_byte,
          humanoid_shin_bind_matrix(false)),
  };
}

auto build_roman_shoulder_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::roman_shoulder_cover_make_static_attachment(
          humanoid_shoulder_l_bone(), base_role_byte,
          humanoid_shoulder_bind_matrix(true)),
      Render::GL::roman_shoulder_cover_make_static_attachment(
          humanoid_shoulder_r_bone(), base_role_byte,
          humanoid_shoulder_bind_matrix(false)),
  };
}

auto build_carthage_shoulder_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::carthage_shoulder_cover_make_static_attachment(
          humanoid_shoulder_l_bone(), base_role_byte,
          humanoid_shoulder_bind_matrix(true)),
      Render::GL::carthage_shoulder_cover_make_static_attachment(
          humanoid_shoulder_r_bone(), base_role_byte,
          humanoid_shoulder_bind_matrix(false)),
  };
}

auto build_tool_belt_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return to_vector(Render::GL::tool_belt_make_static_attachments(
      humanoid_pelvis_bone(), base_role_byte));
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
    cfg.shaft_color =
        QVector3D(0.5F, 0.3F, 0.2F) * QVector3D(0.85F, 0.75F, 0.65F);
    cfg.spearhead_color = QVector3D(0.75F, 0.76F, 0.80F);
    cfg.spear_length = 1.15F;
    cfg.shaft_radius = 0.018F;
    cfg.spearhead_length = 0.16F;
    return cfg;
  }();
  return to_vector(
      Render::GL::spear_make_static_attachments(config, base_role_byte));
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
      Render::GL::sword_make_static_attachment(roman_sword_config(),
                                               base_role_byte),
      Render::GL::scabbard_make_static_attachment(
          k_scabbard_radius, humanoid_hip_l_bone(),
          static_cast<std::uint8_t>(base_role_byte +
                                    Render::GL::k_sword_role_count)),
  };
}

auto build_carthage_sword_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::sword_make_static_attachment(carthage_sword_config(),
                                               base_role_byte),
      Render::GL::scabbard_make_static_attachment(
          k_scabbard_radius, humanoid_hip_l_bone(),
          static_cast<std::uint8_t>(base_role_byte +
                                    Render::GL::k_sword_role_count)),
  };
}

auto build_roman_bow_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return to_vector(Render::GL::bow_make_static_attachments(roman_bow_config(),
                                                           base_role_byte));
}

auto build_carthage_bow_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return to_vector(Render::GL::bow_make_static_attachments(
      carthage_bow_config(), base_role_byte));
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

auto build_carthage_quiver_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return to_vector(Render::GL::quiver_make_static_attachments(
      carthage_quiver_config(), humanoid_pelvis_bone(), base_role_byte));
}

auto build_carthage_cloak_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::cloak_make_static_attachment(
      carthage_cloak_config(), cloak_meshes_for("cloak_carthage"),
      humanoid_chest_bone(), base_role_byte)};
}

auto build_roman_cloak_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::cloak_make_static_attachment(
      roman_cloak_config(), cloak_meshes_for("cloak_roman"),
      humanoid_chest_bone(), base_role_byte)};
}

auto build_roman_horse_saddle_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::roman_saddle_make_static_attachment(
      horse_root_bone(), base_role_byte, horse_baseline_back_center_frame(),
      horse_root_bind_matrix())};
}

auto build_carthage_horse_saddle_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::carthage_saddle_make_static_attachment(
      horse_root_bone(), base_role_byte, horse_baseline_back_center_frame(),
      horse_root_bind_matrix())};
}

auto build_light_cavalry_saddle_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::light_cavalry_saddle_make_static_attachment(
      horse_root_bone(), base_role_byte, horse_baseline_back_center_frame(),
      horse_root_bind_matrix())};
}

auto build_horse_bridle_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::bridle_make_static_attachment(
      horse_head_bone(), base_role_byte, horse_baseline_head_frame(),
      horse_head_bind_matrix())};
}

auto build_horse_reins_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::reins_make_static_attachment(
      horse_root_bone(), base_role_byte, horse_baseline_back_center_frame(),
      horse_root_bind_matrix())};
}

auto build_horse_blanket_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::blanket_make_static_attachment(
      horse_root_bone(), base_role_byte, horse_baseline_back_center_frame(),
      horse_root_bind_matrix())};
}

auto build_leather_barding_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::leather_barding_make_static_attachment(
          Render::GL::leather_barding_chest_archetype(), horse_root_bone(),
          base_role_byte, horse_baseline_chest_frame(),
          horse_root_bind_matrix()),
      Render::GL::leather_barding_make_static_attachment(
          Render::GL::leather_barding_barrel_archetype(), horse_root_bone(),
          base_role_byte, horse_baseline_barrel_frame(),
          horse_root_bind_matrix()),
  };
}

auto build_scale_barding_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::scale_barding_make_static_attachment(
          Render::GL::scale_barding_chest_archetype(), horse_root_bone(),
          base_role_byte, horse_baseline_chest_frame(),
          horse_root_bind_matrix()),
      Render::GL::scale_barding_make_static_attachment(
          Render::GL::scale_barding_barrel_archetype(), horse_root_bone(),
          base_role_byte, horse_baseline_barrel_frame(),
          horse_root_bind_matrix()),
      Render::GL::scale_barding_make_static_attachment(
          Render::GL::scale_barding_neck_archetype(), horse_neck_top_bone(),
          base_role_byte, horse_baseline_neck_base_frame(),
          horse_neck_top_bind_matrix()),
  };
}

auto build_champion_barding_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::champion_make_static_attachment(
      horse_root_bone(), base_role_byte, horse_baseline_chest_frame(),
      horse_root_bind_matrix())};
}

auto build_crupper_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::crupper_make_static_attachment(
      horse_root_bone(), base_role_byte, horse_baseline_rump_frame(),
      horse_root_bind_matrix())};
}

auto build_saddle_bag_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::saddle_bag_make_static_attachment(
      horse_root_bone(), base_role_byte, horse_baseline_back_center_frame(),
      horse_root_bind_matrix())};
}

auto with_variant_palette(
    const void *variant_void,
    const std::function<std::uint32_t(const HumanoidVariant &, QVector3D *,
                                      std::uint32_t, std::size_t)> &fn,
    QVector3D *out, std::uint32_t base_count,
    std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  return fn(*static_cast<const HumanoidVariant *>(variant_void), out,
            base_count, max_count);
}

auto with_horse_variant(
    const void *variant_void,
    const std::function<std::uint32_t(const HorseVariant &, QVector3D *,
                                      std::uint32_t, std::size_t)> &fn,
    QVector3D *out, std::uint32_t base_count,
    std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  return fn(*static_cast<const HorseVariant *>(variant_void), out, base_count,
            max_count);
}

auto roman_horse_saddle_role_colors(const void *variant_void, QVector3D *out,
                                    std::uint32_t base_count,
                                    std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::roman_saddle_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto carthage_horse_saddle_role_colors(const void *variant_void, QVector3D *out,
                                       std::uint32_t base_count,
                                       std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::carthage_saddle_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto light_cavalry_saddle_role_colors(const void *variant_void, QVector3D *out,
                                      std::uint32_t base_count,
                                      std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::light_cavalry_saddle_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto horse_bridle_role_colors(const void *variant_void, QVector3D *out,
                              std::uint32_t base_count,
                              std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::bridle_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto horse_reins_role_colors(const void *variant_void, QVector3D *out,
                             std::uint32_t base_count,
                             std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::reins_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto horse_blanket_role_colors(const void *variant_void, QVector3D *out,
                               std::uint32_t base_count,
                               std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::blanket_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto leather_barding_role_colors(const void *variant_void, QVector3D *out,
                                 std::uint32_t base_count,
                                 std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::leather_barding_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto scale_barding_role_colors(const void *variant_void, QVector3D *out,
                               std::uint32_t base_count,
                               std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::scale_barding_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto champion_barding_role_colors(const void *variant_void, QVector3D *out,
                                  std::uint32_t base_count,
                                  std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::champion_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto horse_crupper_role_colors(const void *variant_void, QVector3D *out,
                               std::uint32_t base_count,
                               std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::crupper_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto saddle_bag_role_colors(const void *variant_void, QVector3D *out,
                            std::uint32_t base_count,
                            std::size_t max_count) -> std::uint32_t {
  return with_horse_variant(
      variant_void,
      [](const HorseVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::saddle_bag_fill_role_colors(
                           variant, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto carthage_light_helmet_role_colors(const void *variant_void, QVector3D *out,
                                       std::uint32_t base_count,
                                       std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::carthage_light_helmet_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto carthage_heavy_helmet_role_colors(const void *variant_void, QVector3D *out,
                                       std::uint32_t base_count,
                                       std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::carthage_heavy_helmet_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto roman_heavy_helmet_role_colors(const void *variant_void, QVector3D *out,
                                    std::uint32_t base_count,
                                    std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::roman_heavy_helmet_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto roman_light_helmet_role_colors(const void *variant_void, QVector3D *out,
                                    std::uint32_t base_count,
                                    std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::roman_light_helmet_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto roman_light_armor_role_colors(const void *variant_void, QVector3D *out,
                                   std::uint32_t base_count,
                                   std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::roman_light_armor_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto roman_heavy_armor_role_colors(const void *variant_void, QVector3D *out,
                                   std::uint32_t base_count,
                                   std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::roman_heavy_armor_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto carthage_light_armor_role_colors(const void *variant_void, QVector3D *out,
                                      std::uint32_t base_count,
                                      std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::armor_light_carthage_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto carthage_heavy_armor_role_colors(const void *variant_void, QVector3D *out,
                                      std::uint32_t base_count,
                                      std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::armor_heavy_carthage_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto roman_greaves_role_colors(const void *variant_void, QVector3D *out,
                               std::uint32_t base_count,
                               std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::roman_greaves_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto roman_shoulder_role_colors(const void *variant_void, QVector3D *out,
                                std::uint32_t base_count,
                                std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::roman_shoulder_cover_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto carthage_shoulder_role_colors(const void *variant_void, QVector3D *out,
                                   std::uint32_t base_count,
                                   std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::carthage_shoulder_cover_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto spear_role_colors(const void *variant_void, QVector3D *out,
                       std::uint32_t base_count,
                       std::size_t max_count) -> std::uint32_t {
  static const SpearRenderConfig config = []() {
    SpearRenderConfig cfg;
    cfg.shaft_color =
        QVector3D(0.5F, 0.3F, 0.2F) * QVector3D(0.85F, 0.75F, 0.65F);
    cfg.spearhead_color = QVector3D(0.75F, 0.76F, 0.80F);
    cfg.spear_length = 1.15F;
    cfg.shaft_radius = 0.018F;
    cfg.spearhead_length = 0.16F;
    return cfg;
  }();
  return with_variant_palette(
      variant_void,
      [&](const HumanoidVariant &variant, QVector3D *colors,
          std::uint32_t count, std::size_t max) {
        return count +
               Render::GL::spear_fill_role_colors(variant.palette, config,
                                                  colors + count, max - count);
      },
      out, base_count, max_count);
}

auto roman_scutum_role_colors(const void *variant_void, QVector3D *out,
                              std::uint32_t base_count,
                              std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::roman_scutum_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto carthage_shield_role_colors(const void *variant_void, QVector3D *out,
                                 std::uint32_t base_count,
                                 std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::carthage_shield_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto roman_sword_role_colors(const void *variant_void, QVector3D *out,
                             std::uint32_t base_count,
                             std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        count += Render::GL::sword_fill_role_colors(
            variant.palette, roman_sword_config(), colors + count, max - count);
        if (max <= count) {
          return count;
        }
        return count + Render::GL::scabbard_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto carthage_sword_role_colors(const void *variant_void, QVector3D *out,
                                std::uint32_t base_count,
                                std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        count += Render::GL::sword_fill_role_colors(
            variant.palette, carthage_sword_config(), colors + count,
            max - count);
        if (max <= count) {
          return count;
        }
        return count + Render::GL::scabbard_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto bow_role_colors(const void *variant_void, QVector3D *out,
                     std::uint32_t base_count,
                     std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::bow_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto quiver_role_colors(const void *variant_void, QVector3D *out,
                        std::uint32_t base_count,
                        std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::quiver_fill_role_colors(
                           variant.palette, mounted_quiver_config(),
                           colors + count, max - count);
      },
      out, base_count, max_count);
}

auto roman_quiver_role_colors(const void *variant_void, QVector3D *out,
                              std::uint32_t base_count,
                              std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::quiver_fill_role_colors(
                           variant.palette, roman_quiver_config(),
                           colors + count, max - count);
      },
      out, base_count, max_count);
}

auto carthage_quiver_role_colors(const void *variant_void, QVector3D *out,
                                 std::uint32_t base_count,
                                 std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::quiver_fill_role_colors(
                           variant.palette, carthage_quiver_config(),
                           colors + count, max - count);
      },
      out, base_count, max_count);
}

auto carthage_cloak_role_colors(const void *variant_void, QVector3D *out,
                                std::uint32_t base_count,
                                std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::cloak_fill_role_colors_with_primary(
                           carthage_cloak_config().primary_color,
                           variant.palette, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto roman_cloak_role_colors(const void *variant_void, QVector3D *out,
                             std::uint32_t base_count,
                             std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::cloak_fill_role_colors_with_primary(
                           roman_cloak_config().primary_color, variant.palette,
                           colors + count, max - count);
      },
      out, base_count, max_count);
}

auto roman_tool_belt_role_colors(const void *variant_void, QVector3D *out,
                                 std::uint32_t base_count,
                                 std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::tool_belt_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out, base_count, max_count);
}

auto carthage_tool_belt_role_colors(const void *variant_void, QVector3D *out,
                                    std::uint32_t base_count,
                                    std::size_t max_count) -> std::uint32_t {
  return roman_tool_belt_role_colors(variant_void, out, base_count, max_count);
}

auto roman_work_apron_role_colors(const void *variant_void, QVector3D *out,
                                  std::uint32_t base_count,
                                  std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
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
      out, base_count, max_count);
}

auto carthage_work_apron_role_colors(const void *variant_void, QVector3D *out,
                                     std::uint32_t base_count,
                                     std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
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
      out, base_count, max_count);
}

auto roman_arm_guards_role_colors(const void *variant_void, QVector3D *out,
                                  std::uint32_t base_count,
                                  std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        if (max - count < Render::GL::k_arm_guards_role_count) {
          return count;
        }
        colors[count] = variant.palette.leather_dark;
        colors[count + 1U] = variant.palette.leather_dark * 0.78F +
                             variant.palette.metal * 0.22F;
        return count + Render::GL::k_arm_guards_role_count;
      },
      out, base_count, max_count);
}

auto carthage_arm_guards_role_colors(const void *variant_void, QVector3D *out,
                                     std::uint32_t base_count,
                                     std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant &variant, QVector3D *colors, std::uint32_t count,
         std::size_t max) {
        if (max - count < Render::GL::k_arm_guards_role_count) {
          return count;
        }
        colors[count] = variant.palette.leather_dark;
        colors[count + 1U] = variant.palette.leather_dark * 0.80F +
                             variant.palette.metal * 0.20F;
        return count + Render::GL::k_arm_guards_role_count;
      },
      out, base_count, max_count);
}

void register_humanoid_descriptor(EquipmentCategory category, const char *id,
                                  HumanoidEquipmentContribution contribution) {
  const auto handle =
      EquipmentRegistry::instance().resolve_handle(category, id);
  register_humanoid_equipment_contribution(handle, std::move(contribution));
}

void register_horse_descriptor(EquipmentCategory category, const char *id,
                               HorseEquipmentContribution contribution) {
  const auto handle =
      EquipmentRegistry::instance().resolve_handle(category, id);
  register_horse_equipment_contribution(handle, std::move(contribution));
}

} // namespace

void register_built_in_equipment() {
  auto &registry = EquipmentRegistry::instance();

  BowRenderConfig carthage_config;
  carthage_config.bow_depth = 0.28F;
  carthage_config.bow_curve_factor = 1.2F;
  carthage_config.bow_height_scale = 0.95F;
  auto carthage_bow = std::make_shared<BowRenderer>(carthage_config);
  registry.register_equipment(EquipmentCategory::Weapon, "bow_carthage",
                              carthage_bow);

  BowRenderConfig roman_config;
  roman_config.bow_depth = 0.22F;
  roman_config.bow_curve_factor = 1.0F;
  roman_config.bow_height_scale = 1.0F;
  auto roman_bow = std::make_shared<BowRenderer>(roman_config);
  registry.register_equipment(EquipmentCategory::Weapon, "bow_roman",
                              roman_bow);

  auto bow = std::make_shared<BowRenderer>();
  registry.register_equipment(EquipmentCategory::Weapon, "bow", bow);

  auto quiver = std::make_shared<QuiverRenderer>();
  registry.register_equipment(EquipmentCategory::Weapon, "quiver", quiver);
  auto roman_quiver = std::make_shared<QuiverRenderer>(roman_quiver_config());
  registry.register_equipment(EquipmentCategory::Weapon, "quiver_roman",
                              roman_quiver);
  auto carthage_quiver =
      std::make_shared<QuiverRenderer>(carthage_quiver_config());
  registry.register_equipment(EquipmentCategory::Weapon, "quiver_carthage",
                              carthage_quiver);

  auto roman_scutum = std::make_shared<RomanScutumRenderer>();
  registry.register_equipment(EquipmentCategory::Weapon, "roman_scutum",
                              roman_scutum);

  auto carthage_heavy_helmet = std::make_shared<CarthageHeavyHelmetRenderer>();
  registry.register_equipment(EquipmentCategory::Helmet, "carthage_heavy",
                              carthage_heavy_helmet);
  auto roman_heavy_helmet = std::make_shared<RomanHeavyHelmetRenderer>();
  registry.register_equipment(EquipmentCategory::Helmet, "roman_heavy",
                              roman_heavy_helmet);
  auto roman_light_helmet = std::make_shared<RomanLightHelmetRenderer>();
  registry.register_equipment(EquipmentCategory::Helmet, "roman_light",
                              roman_light_helmet);

  auto carthage_light_helmet = std::make_shared<CarthageLightHelmetRenderer>();
  registry.register_equipment(EquipmentCategory::Helmet, "carthage_light",
                              carthage_light_helmet);

  auto headwrap = std::make_shared<HeadwrapRenderer>();
  registry.register_equipment(EquipmentCategory::Helmet, "headwrap", headwrap);

  auto roman_heavy_armor = std::make_shared<RomanHeavyArmorRenderer>();
  registry.register_equipment(EquipmentCategory::Armor, "roman_heavy_armor",
                              roman_heavy_armor);

  auto roman_light_armor = std::make_shared<RomanLightArmorRenderer>();
  registry.register_equipment(EquipmentCategory::Armor, "roman_light_armor",
                              roman_light_armor);

  auto armor_light_carthage = std::make_shared<ArmorLightCarthageRenderer>();
  registry.register_equipment(EquipmentCategory::Armor, "armor_light_carthage",
                              armor_light_carthage);

  auto armor_heavy_carthage = std::make_shared<ArmorHeavyCarthageRenderer>();
  registry.register_equipment(EquipmentCategory::Armor, "armor_heavy_carthage",
                              armor_heavy_carthage);

  auto roman_shoulder_cover = std::make_shared<RomanShoulderCoverRenderer>();
  registry.register_equipment(EquipmentCategory::Armor, "roman_shoulder_cover",
                              roman_shoulder_cover);
  auto roman_shoulder_cover_cavalry =
      std::make_shared<RomanShoulderCoverRenderer>(1.8F);
  registry.register_equipment(EquipmentCategory::Armor,
                              "roman_shoulder_cover_cavalry",
                              roman_shoulder_cover_cavalry);

  auto roman_greaves = std::make_shared<RomanGreavesRenderer>();
  registry.register_equipment(EquipmentCategory::Armor, "roman_greaves",
                              roman_greaves);

  auto carthage_shoulder_cover =
      std::make_shared<CarthageShoulderCoverRenderer>();
  registry.register_equipment(EquipmentCategory::Armor,
                              "carthage_shoulder_cover",
                              carthage_shoulder_cover);
  auto carthage_shoulder_cover_cavalry =
      std::make_shared<CarthageShoulderCoverRenderer>(1.8F);
  registry.register_equipment(EquipmentCategory::Armor,
                              "carthage_shoulder_cover_cavalry",
                              carthage_shoulder_cover_cavalry);
  CloakConfig carthage_cloak_config;
  carthage_cloak_config.primary_color = QVector3D(0.14F, 0.38F, 0.54F);
  carthage_cloak_config.trim_color = QVector3D(0.75F, 0.66F, 0.42F);
  auto cloak_carthage = std::make_shared<CloakRenderer>(carthage_cloak_config);
  registry.register_equipment(EquipmentCategory::Armor, "cloak_carthage",
                              cloak_carthage);
  CloakConfig roman_cloak_config;
  roman_cloak_config.primary_color = QVector3D(0.70F, 0.15F, 0.18F);
  roman_cloak_config.trim_color = QVector3D(0.78F, 0.72F, 0.58F);
  roman_cloak_config.back_material_id = 12;
  roman_cloak_config.shoulder_material_id = 13;
  auto cloak_roman = std::make_shared<CloakRenderer>(roman_cloak_config);
  registry.register_equipment(EquipmentCategory::Armor, "cloak_roman",
                              cloak_roman);

  auto sword = std::make_shared<SwordRenderer>();
  registry.register_equipment(EquipmentCategory::Weapon, "sword", sword);

  auto sword_carthage = std::make_shared<CarthageSwordRenderer>();
  registry.register_equipment(EquipmentCategory::Weapon, "sword_carthage",
                              sword_carthage);

  auto sword_roman = std::make_shared<RomanSwordRenderer>();
  registry.register_equipment(EquipmentCategory::Weapon, "sword_roman",
                              sword_roman);

  auto spear = std::make_shared<SpearRenderer>();
  registry.register_equipment(EquipmentCategory::Weapon, "spear", spear);

  auto shield = std::make_shared<ShieldRenderer>();
  registry.register_equipment(EquipmentCategory::Weapon, "shield", shield);

  auto shield_carthage = std::make_shared<CarthageShieldRenderer>();
  registry.register_equipment(EquipmentCategory::Weapon, "shield_carthage",
                              shield_carthage);

  auto shield_carthage_cavalry =
      std::make_shared<CarthageShieldRenderer>(0.84F);
  registry.register_equipment(EquipmentCategory::Weapon,
                              "shield_carthage_cavalry",
                              shield_carthage_cavalry);

  WorkApronConfig roman_apron_config;
  roman_apron_config.leather_color = QVector3D(0.48F, 0.35F, 0.22F);
  auto work_apron_roman =
      std::make_shared<WorkApronRenderer>(roman_apron_config);
  registry.register_equipment(EquipmentCategory::Armor, "work_apron_roman",
                              work_apron_roman);

  WorkApronConfig carthage_apron_config;
  carthage_apron_config.leather_color = QVector3D(0.44F, 0.30F, 0.18F);
  auto work_apron_carthage =
      std::make_shared<WorkApronRenderer>(carthage_apron_config);
  registry.register_equipment(EquipmentCategory::Armor, "work_apron_carthage",
                              work_apron_carthage);

  ToolBeltConfig roman_tool_belt_config;
  roman_tool_belt_config.leather_color = QVector3D(0.52F, 0.40F, 0.28F);
  roman_tool_belt_config.include_saw = true;
  auto tool_belt_roman =
      std::make_shared<ToolBeltRenderer>(roman_tool_belt_config);
  registry.register_equipment(EquipmentCategory::Armor, "tool_belt_roman",
                              tool_belt_roman);

  ToolBeltConfig carthage_tool_belt_config;
  carthage_tool_belt_config.leather_color = QVector3D(0.46F, 0.34F, 0.22F);
  carthage_tool_belt_config.include_saw = true;
  auto tool_belt_carthage =
      std::make_shared<ToolBeltRenderer>(carthage_tool_belt_config);
  registry.register_equipment(EquipmentCategory::Armor, "tool_belt_carthage",
                              tool_belt_carthage);

  ArmGuardsConfig arm_guards_config;
  arm_guards_config.leather_color = QVector3D(0.50F, 0.38F, 0.26F);
  auto arm_guards = std::make_shared<ArmGuardsRenderer>(arm_guards_config);
  registry.register_equipment(EquipmentCategory::Armor, "arm_guards",
                              arm_guards);
  registry.register_equipment(EquipmentCategory::Armor, "arm_guards_roman",
                              arm_guards);
  registry.register_equipment(EquipmentCategory::Armor, "arm_guards_carthage",
                              arm_guards);
  registry.register_placeholder_equipment(EquipmentCategory::Armor,
                                          "builder_tunic_roman");
  registry.register_placeholder_equipment(EquipmentCategory::Armor,
                                          "roman_civilian_mantle");
  registry.register_placeholder_equipment(EquipmentCategory::Helmet,
                                          "headwrap_carthage_civilian");
  registry.register_placeholder_equipment(EquipmentCategory::Armor,
                                          "carthage_robes");
  registry.register_placeholder_equipment(EquipmentCategory::Armor,
                                          "carthage_civilian_sash");
  registry.register_placeholder_equipment(EquipmentCategory::HorseTack,
                                          "roman_horse_saddle");
  registry.register_placeholder_equipment(EquipmentCategory::HorseTack,
                                          "carthage_horse_saddle");
  registry.register_placeholder_equipment(EquipmentCategory::HorseTack,
                                          "light_cavalry_saddle");

  registry.register_horse_equipment(EquipmentCategory::HorseTack,
                                    "horse_bridle",
                                    std::make_shared<BridleRenderer>());
  registry.register_horse_equipment(EquipmentCategory::HorseTack, "horse_reins",
                                    std::make_shared<ReinsRenderer>());
  registry.register_horse_equipment(EquipmentCategory::HorseTack,
                                    "horse_blanket",
                                    std::make_shared<BlanketRenderer>());
  registry.register_horse_equipment(EquipmentCategory::HorseArmor,
                                    "horse_leather_barding",
                                    std::make_shared<LeatherBardingRenderer>());
  registry.register_horse_equipment(EquipmentCategory::HorseArmor,
                                    "horse_scale_barding",
                                    std::make_shared<ScaleBardingRenderer>());
  registry.register_horse_equipment(EquipmentCategory::HorseArmor,
                                    "horse_champion_barding",
                                    std::make_shared<ChampionRenderer>());
  registry.register_horse_equipment(EquipmentCategory::HorseArmor,
                                    "horse_crupper",
                                    std::make_shared<CrupperRenderer>());
  registry.register_horse_equipment(EquipmentCategory::HorseDecoration,
                                    "horse_saddle_bag",
                                    std::make_shared<SaddleBagRenderer>());

  register_horse_descriptor(
      EquipmentCategory::HorseTack, "roman_horse_saddle",
      {.build_attachments = &build_roman_horse_saddle_attachment,
       .append_role_colors = &roman_horse_saddle_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_roman_saddle_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseTack, "carthage_horse_saddle",
      {.build_attachments = &build_carthage_horse_saddle_attachment,
       .append_role_colors = &carthage_horse_saddle_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_carthage_saddle_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseTack, "light_cavalry_saddle",
      {.build_attachments = &build_light_cavalry_saddle_attachment,
       .append_role_colors = &light_cavalry_saddle_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_light_cavalry_saddle_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseTack, "horse_bridle",
      {.build_attachments = &build_horse_bridle_attachment,
       .append_role_colors = &horse_bridle_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_bridle_role_count)});
  register_horse_descriptor(EquipmentCategory::HorseTack, "horse_reins",
                            {.build_attachments = &build_horse_reins_attachment,
                             .append_role_colors = &horse_reins_role_colors,
                             .role_count = static_cast<std::uint8_t>(
                                 Render::GL::k_reins_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseTack, "horse_blanket",
      {.build_attachments = &build_horse_blanket_attachment,
       .append_role_colors = &horse_blanket_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_blanket_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseArmor, "horse_leather_barding",
      {.build_attachments = &build_leather_barding_attachments,
       .append_role_colors = &leather_barding_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_leather_barding_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseArmor, "horse_scale_barding",
      {.build_attachments = &build_scale_barding_attachments,
       .append_role_colors = &scale_barding_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_scale_barding_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseArmor, "horse_champion_barding",
      {.build_attachments = &build_champion_barding_attachment,
       .append_role_colors = &champion_barding_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_champion_role_count)});
  register_horse_descriptor(EquipmentCategory::HorseArmor, "horse_crupper",
                            {.build_attachments = &build_crupper_attachment,
                             .append_role_colors = &horse_crupper_role_colors,
                             .role_count = static_cast<std::uint8_t>(
                                 Render::GL::k_crupper_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseDecoration, "horse_saddle_bag",
      {.build_attachments = &build_saddle_bag_attachment,
       .append_role_colors = &saddle_bag_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_saddle_bag_role_count)});

  register_humanoid_descriptor(
      EquipmentCategory::Helmet, "carthage_light",
      {.build_attachments = &build_carthage_light_helmet_attachments,
       .append_role_colors = &carthage_light_helmet_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_carthage_light_helmet_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Helmet, "carthage_heavy",
      {.build_attachments = &build_carthage_heavy_helmet_attachments,
       .append_role_colors = &carthage_heavy_helmet_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_carthage_heavy_helmet_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Helmet, "roman_heavy",
      {.build_attachments = &build_roman_heavy_helmet_attachment,
       .append_role_colors = &roman_heavy_helmet_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_roman_heavy_helmet_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Helmet, "roman_light",
      {.build_attachments = &build_roman_light_helmet_attachment,
       .append_role_colors = &roman_light_helmet_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_roman_light_helmet_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor, "roman_light_armor",
      {.build_attachments = &build_roman_light_armor_attachment,
       .append_role_colors = &roman_light_armor_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_roman_light_armor_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor, "roman_heavy_armor",
      {.build_attachments = &build_roman_heavy_armor_attachment,
       .append_role_colors = &roman_heavy_armor_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_roman_heavy_armor_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor, "armor_light_carthage",
      {.build_attachments = &build_carthage_light_armor_attachment,
       .append_role_colors = &carthage_light_armor_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_armor_light_carthage_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor, "armor_heavy_carthage",
      {.build_attachments = &build_carthage_heavy_armor_attachment,
       .append_role_colors = &carthage_heavy_armor_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_armor_heavy_carthage_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor, "roman_greaves",
      {.build_attachments = &build_roman_greaves_attachments,
       .append_role_colors = &roman_greaves_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_roman_greaves_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor, "roman_shoulder_cover",
      {.build_attachments = &build_roman_shoulder_attachments,
       .append_role_colors = &roman_shoulder_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_roman_shoulder_cover_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor, "roman_shoulder_cover_cavalry",
      {.build_attachments = &build_roman_shoulder_attachments,
       .append_role_colors = &roman_shoulder_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_roman_shoulder_cover_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor, "carthage_shoulder_cover",
      {.build_attachments = &build_carthage_shoulder_attachments,
       .append_role_colors = &carthage_shoulder_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_carthage_shoulder_cover_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor, "carthage_shoulder_cover_cavalry",
      {.build_attachments = &build_carthage_shoulder_attachments,
       .append_role_colors = &carthage_shoulder_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_carthage_shoulder_cover_role_count)});
  register_humanoid_descriptor(EquipmentCategory::Weapon, "spear",
                               {.build_attachments = &build_spear_attachments,
                                .append_role_colors = &spear_role_colors,
                                .role_count = static_cast<std::uint8_t>(
                                    Render::GL::k_spear_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon, "roman_scutum",
      {.build_attachments = &build_roman_scutum_attachment,
       .append_role_colors = &roman_scutum_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_roman_scutum_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon, "shield_carthage",
      {.build_attachments = &build_carthage_shield_attachment,
       .append_role_colors = &carthage_shield_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_carthage_shield_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon, "sword_roman",
      {.build_attachments = &build_roman_sword_attachments,
       .append_role_colors = &roman_sword_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_sword_role_count +
                                     Render::GL::k_scabbard_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon, "sword_carthage",
      {.build_attachments = &build_carthage_sword_attachments,
       .append_role_colors = &carthage_sword_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_sword_role_count +
                                     Render::GL::k_scabbard_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon, "bow_roman",
      {.build_attachments = &build_roman_bow_attachments,
       .append_role_colors = &bow_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_bow_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon, "bow_carthage",
      {.build_attachments = &build_carthage_bow_attachments,
       .append_role_colors = &bow_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_bow_role_count)});
  register_humanoid_descriptor(EquipmentCategory::Weapon, "quiver",
                               {.build_attachments = &build_quiver_attachments,
                                .append_role_colors = &quiver_role_colors,
                                .role_count = static_cast<std::uint8_t>(
                                    Render::GL::k_quiver_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon, "quiver_roman",
      {.build_attachments = &build_roman_quiver_attachments,
       .append_role_colors = &roman_quiver_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_quiver_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon, "quiver_carthage",
      {.build_attachments = &build_carthage_quiver_attachments,
       .append_role_colors = &carthage_quiver_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_quiver_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor, "cloak_carthage",
      {.build_attachments = &build_carthage_cloak_attachment,
       .append_role_colors = &carthage_cloak_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_cloak_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor, "cloak_roman",
      {.build_attachments = &build_roman_cloak_attachment,
       .append_role_colors = &roman_cloak_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_cloak_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor, "tool_belt_roman",
      {.build_attachments = &build_tool_belt_attachments,
       .append_role_colors = &roman_tool_belt_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_tool_belt_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor, "tool_belt_carthage",
      {.build_attachments = &build_tool_belt_attachments,
       .append_role_colors = &carthage_tool_belt_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_tool_belt_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor, "work_apron_roman",
      {.build_attachments = &build_work_apron_attachments,
       .append_role_colors = &roman_work_apron_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_work_apron_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor, "work_apron_carthage",
      {.build_attachments = &build_work_apron_attachments,
       .append_role_colors = &carthage_work_apron_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_work_apron_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor, "arm_guards_roman",
      {.build_attachments = &build_arm_guards_attachments,
       .append_role_colors = &roman_arm_guards_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_arm_guards_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor, "arm_guards_carthage",
      {.build_attachments = &build_arm_guards_attachments,
       .append_role_colors = &carthage_arm_guards_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_arm_guards_role_count)});

  auto &ar = RenderArchetypeRegistry::instance();

  ar.register_archetype("roman_light_helmet",
                        [] { (void)roman_light_helmet_archetype(); });
  ar.register_archetype("roman_heavy_helmet",
                        [] { (void)roman_heavy_helmet_archetype(); });
  ar.register_archetype("headwrap_helmet",
                        [] { (void)headwrap_helmet_archetype(); });
  ar.register_archetype("carthage_light_helmet_shell",
                        [] { (void)carthage_light_helmet_shell_archetype(); });
  ar.register_archetype("carthage_light_helmet_neck_guard", [] {
    (void)carthage_light_helmet_neck_guard_archetype();
  });
  ar.register_archetype("carthage_light_helmet_cheek_guards", [] {
    (void)carthage_light_helmet_cheek_guards_archetype();
  });
  ar.register_archetype("carthage_light_helmet_nasal_guard", [] {
    (void)carthage_light_helmet_nasal_guard_archetype();
  });
  ar.register_archetype("carthage_light_helmet_crest_low", [] {
    (void)carthage_light_helmet_crest_archetype(false);
  });
  ar.register_archetype("carthage_light_helmet_crest_high", [] {
    (void)carthage_light_helmet_crest_archetype(true);
  });
  ar.register_archetype("carthage_light_helmet_rivets",
                        [] { (void)carthage_light_helmet_rivets_archetype(); });
  ar.register_archetype("carthage_heavy_helmet_shell",
                        [] { (void)carthage_heavy_helmet_shell_archetype(); });
  ar.register_archetype("carthage_heavy_helmet_neck_guard", [] {
    (void)carthage_heavy_helmet_neck_guard_archetype();
  });
  ar.register_archetype("carthage_heavy_helmet_cheek_guards", [] {
    (void)carthage_heavy_helmet_cheek_guards_archetype();
  });
  ar.register_archetype("carthage_heavy_helmet_face_plate", [] {
    (void)carthage_heavy_helmet_face_plate_archetype();
  });
  ar.register_archetype("carthage_heavy_helmet_crest",
                        [] { (void)carthage_heavy_helmet_crest_archetype(); });
  ar.register_archetype("carthage_heavy_helmet_rivets",
                        [] { (void)carthage_heavy_helmet_rivets_archetype(); });

  ar.register_archetype("roman_shoulder_cover",
                        [] { (void)roman_shoulder_cover_archetype(); });
  ar.register_archetype("carthage_shoulder_cover",
                        [] { (void)carthage_shoulder_cover_archetype(); });
  ar.register_archetype("roman_greaves",
                        [] { (void)roman_greaves_archetype(); });

  ar.register_archetype("roman_scutum", [] { (void)roman_scutum_archetype(); });

  ar.register_archetype("tool_belt_ring",
                        [] { (void)tool_belt_ring_archetype(); });
  ar.register_archetype("tool_belt_buckle",
                        [] { (void)tool_belt_buckle_archetype(); });
  ar.register_archetype("tool_belt_hammer",
                        [] { (void)tool_belt_hammer_archetype(); });
  ar.register_archetype("tool_belt_chisel",
                        [] { (void)tool_belt_chisel_archetype(); });
  ar.register_archetype("tool_belt_saw",
                        [] { (void)tool_belt_saw_archetype(); });
  ar.register_archetype("tool_belt_pouches",
                        [] { (void)tool_belt_pouches_archetype(); });

  ar.register_archetype("reins", [] { (void)reins_archetype(); });
  ar.register_archetype("blanket", [] { (void)blanket_archetype(); });
  ar.register_archetype("bridle", [] { (void)bridle_archetype(); });

  ar.register_archetype("roman_saddle", [] { (void)roman_saddle_archetype(); });
  ar.register_archetype("carthage_saddle",
                        [] { (void)carthage_saddle_archetype(); });
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
