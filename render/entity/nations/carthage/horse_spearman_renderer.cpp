#include "horse_spearman_renderer.h"

#include "../../../equipment/armor/armor_heavy_carthage.h"
#include "../../../equipment/armor/carthage_shoulder_cover.h"
#include "../../../equipment/armor/shoulder_cover_archetype.h"
#include "../../../equipment/helmets/carthage_heavy_helmet.h"
#include "../../../equipment/horse/horse_attachment_archetype.h"
#include "../../../equipment/horse/saddles/carthage_saddle_renderer.h"
#include "../../../equipment/horse/saddles/horse_mount_archetype.h"
#include "../../../equipment/weapons/spear_renderer.h"
#include "../../../humanoid/humanoid_renderer_base.h"
#include "../../../humanoid/humanoid_spec.h"
#include "../../../humanoid/skeleton.h"
#include "../../../submitter.h"
#include "../../horse_spearman_renderer_base.h"

#include <memory>

namespace Render::GL::Carthage {
namespace {

auto register_horse_spearman_rider_archetype()
    -> Render::Creature::ArchetypeId {
  static const auto k_head_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head);
  static const auto k_shoulder_l_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ShoulderL);
  static const auto k_shoulder_r_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ShoulderR);
  static const auto k_hand_r_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::HandR);
  static const auto k_chest_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest);
  static const auto k_helmet_base_role_byte =
      static_cast<std::uint8_t>(Render::Humanoid::k_humanoid_role_count + 1U);
  static const auto k_shoulder_base_role_byte = static_cast<std::uint8_t>(
      k_helmet_base_role_byte + Render::GL::kCarthageHeavyHelmetRoleCount);
  static const auto k_spear_base_role_byte = static_cast<std::uint8_t>(
      k_shoulder_base_role_byte + Render::GL::kCarthageShoulderCoverRoleCount);
  static const auto k_armor_base_role_byte = static_cast<std::uint8_t>(
      k_spear_base_role_byte + Render::GL::kSpearRoleCount);
  static const auto k_head_bind_matrix =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
          Render::Humanoid::HumanoidBone::Head)];
  const auto &bind_frames = Render::Humanoid::humanoid_bind_body_frames();
  static const auto k_shoulder_l_bind_matrix =
      Render::GL::make_shoulder_cover_transform(
          QMatrix4x4{}, bind_frames.shoulder_l.origin, -bind_frames.torso.right,
          bind_frames.torso.up);
  static const auto k_shoulder_r_bind_matrix =
      Render::GL::make_shoulder_cover_transform(
          QMatrix4x4{}, bind_frames.shoulder_r.origin, bind_frames.torso.right,
          bind_frames.torso.up);
  static const auto k_hand_r_bind_matrix =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
          Render::Humanoid::HumanoidBone::HandR)];
  static const QVector3D k_hand_r_bind_right = bind_frames.hand_r.right;
  static const SpearRenderConfig k_canonical_spear_cfg = []() {
    SpearRenderConfig cfg;
    cfg.shaft_color =
        QVector3D(0.5F, 0.3F, 0.2F) * QVector3D(0.85F, 0.75F, 0.65F);
    cfg.spearhead_color = QVector3D(0.75F, 0.76F, 0.80F);
    cfg.spear_length = 1.15F;
    cfg.shaft_radius = 0.018F;
    cfg.spearhead_length = 0.16F;
    return cfg;
  }();
  static const std::array<Render::Creature::StaticAttachmentSpec, 13>
      k_attachments{
          Render::GL::carthage_heavy_helmet_make_static_attachment(
              Render::GL::carthage_heavy_helmet_shell_archetype(), k_head_bone,
              k_helmet_base_role_byte, k_head_bind_matrix),
          Render::GL::carthage_heavy_helmet_make_static_attachment(
              Render::GL::carthage_heavy_helmet_neck_guard_archetype(),
              k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix),
          Render::GL::carthage_heavy_helmet_make_static_attachment(
              Render::GL::carthage_heavy_helmet_cheek_guards_archetype(),
              k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix),
          Render::GL::carthage_heavy_helmet_make_static_attachment(
              Render::GL::carthage_heavy_helmet_face_plate_archetype(),
              k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix),
          Render::GL::carthage_heavy_helmet_make_static_attachment(
              Render::GL::carthage_heavy_helmet_crest_archetype(), k_head_bone,
              k_helmet_base_role_byte, k_head_bind_matrix),
          Render::GL::carthage_heavy_helmet_make_static_attachment(
              Render::GL::carthage_heavy_helmet_rivets_archetype(), k_head_bone,
              k_helmet_base_role_byte, k_head_bind_matrix),
          Render::GL::carthage_shoulder_cover_make_static_attachment(
              k_shoulder_l_bone, k_shoulder_base_role_byte,
              k_shoulder_l_bind_matrix),
          Render::GL::carthage_shoulder_cover_make_static_attachment(
              k_shoulder_r_bone, k_shoulder_base_role_byte,
              k_shoulder_r_bind_matrix),
          Render::GL::spear_make_static_attachments(k_canonical_spear_cfg,
                                                    k_spear_base_role_byte)[0],
          Render::GL::spear_make_static_attachments(k_canonical_spear_cfg,
                                                    k_spear_base_role_byte)[1],
          Render::GL::spear_make_static_attachments(k_canonical_spear_cfg,
                                                    k_spear_base_role_byte)[2],
          Render::GL::spear_make_static_attachments(k_canonical_spear_cfg,
                                                    k_spear_base_role_byte)[3],
          Render::GL::armor_heavy_carthage_make_static_attachment(
              k_chest_bone, k_armor_base_role_byte),
      };
  static const auto k_id =
      Render::Creature::ArchetypeRegistry::instance().register_unit_archetype(
          "troops/carthage/horse_spearman/rider",
          Render::Creature::Pipeline::CreatureKind::Humanoid,
          std::span<const Render::Creature::StaticAttachmentSpec>(
              k_attachments.data(), k_attachments.size()),
          +[](const void *variant_void, QVector3D *out,
              std::uint32_t base_count,
              std::size_t max_count) -> std::uint32_t {
            if (variant_void == nullptr || max_count <= base_count) {
              return base_count;
            }
            const auto &v = *static_cast<const HumanoidVariant *>(variant_void);
            auto count = base_count;
            count += Render::GL::carthage_heavy_helmet_fill_role_colors(
                v.palette, out + count, max_count - count);
            if (max_count <= count) {
              return count;
            }
            count += Render::GL::carthage_shoulder_cover_fill_role_colors(
                v.palette, out + count, max_count - count);
            if (max_count <= count) {
              return count;
            }
            count += Render::GL::spear_fill_role_colors(
                v.palette, k_canonical_spear_cfg, out + count,
                max_count - count);
            if (max_count <= count) {
              return count;
            }
            count += Render::GL::armor_heavy_carthage_fill_role_colors(
                v.palette, out + count, max_count - count);
            return count;
          });
  return k_id;
}

auto make_horse_spearman_config() -> HorseSpearmanRendererConfig {
  HorseSpearmanRendererConfig config;
  config.spear_equipment_id = "spear";
  config.helmet_equipment_id = "carthage_heavy";
  config.armor_equipment_id = "armor_heavy_carthage";
  config.shoulder_equipment_id = "carthage_shoulder_cover_cavalry";
  config.has_shoulder = true;
  config.helmet_offset_moving = 0.04F;
  config.rider_archetype_id = register_horse_spearman_rider_archetype();
  static const auto k_mount_archetype =
      register_mount_saddle_archetype("troops/carthage/horse_spearman/mount",
                                      &carthage_saddle_make_static_attachment,
                                      &carthage_saddle_fill_role_colors);
  config.mount_archetype_id = k_mount_archetype;
  return config;
}

} // namespace

void register_horse_spearman_renderer(EntityRendererRegistry &registry) {
  registry.register_renderer(
      "troops/carthage/horse_spearman",
      [](const DrawContext &ctx, ISubmitter &out) {
        static HorseSpearmanRendererBase const static_renderer(
            make_horse_spearman_config());
        static_renderer.render(ctx, out);
      });
}

} // namespace Render::GL::Carthage
