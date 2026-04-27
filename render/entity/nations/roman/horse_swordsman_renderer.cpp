#include "horse_swordsman_renderer.h"

#include "../../../../render/creature/archetype_registry.h"
#include "../../../../render/creature/pipeline/creature_asset.h"
#include "../../../../render/creature/pipeline/unit_visual_spec.h"
#include "../../../../render/horse/horse_spec.h"
#include "../../../../render/humanoid/humanoid_renderer_base.h"
#include "../../../../render/humanoid/humanoid_spec.h"
#include "../../../../render/humanoid/skeleton.h"
#include "../../../equipment/armor/roman_armor.h"
#include "../../../equipment/armor/roman_shoulder_cover.h"
#include "../../../equipment/armor/shoulder_cover_archetype.h"
#include "../../../equipment/helmets/roman_heavy_helmet.h"
#include "../../../equipment/horse/horse_attachment_archetype.h"
#include "../../../equipment/horse/saddles/horse_mount_archetype.h"
#include "../../../equipment/horse/saddles/roman_saddle_renderer.h"
#include "../../../equipment/weapons/roman_scutum.h"
#include "../../../equipment/weapons/sword_renderer.h"
#include "../../../submitter.h"
#include "../../mounted_knight_renderer_base.h"

#include <QVector3D>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace Render::GL::Roman {
namespace {

auto register_horse_swordsman_rider_archetype()
    -> Render::Creature::ArchetypeId {
  static const auto k_head_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head);
  static const auto k_shoulder_l_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ShoulderL);
  static const auto k_shoulder_r_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::ShoulderR);
  static const auto k_hand_l_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::HandL);
  static const auto k_hand_r_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::HandR);
  static const auto k_hip_l_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::HipL);
  static const auto k_chest_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest);
  static const auto k_helmet_base_role_byte =
      static_cast<std::uint8_t>(Render::Humanoid::kHumanoidRoleCount + 1U);
  static const auto k_shoulder_base_role_byte = static_cast<std::uint8_t>(
      k_helmet_base_role_byte + Render::GL::kRomanHeavyHelmetRoleCount);
  static const auto k_shield_base_role_byte = static_cast<std::uint8_t>(
      k_shoulder_base_role_byte + Render::GL::kRomanShoulderCoverRoleCount);
  static const auto k_armor_base_role_byte = static_cast<std::uint8_t>(
      k_shield_base_role_byte + Render::GL::kRomanScutumRoleCount);
  static const auto k_sword_base_role_byte = static_cast<std::uint8_t>(
      k_armor_base_role_byte + Render::GL::kRomanHeavyArmorRoleCount);
  static const auto k_scabbard_base_role_byte = static_cast<std::uint8_t>(
      k_sword_base_role_byte + Render::GL::kSwordRoleCount);
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
  static const auto k_hand_l_bind_matrix =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
          Render::Humanoid::HumanoidBone::HandL)];
  static const auto k_hand_r_bind_matrix =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
          Render::Humanoid::HumanoidBone::HandR)];
  static const SwordRenderConfig k_canonical_sword_cfg = []() {
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
  constexpr float k_canonical_sheath_r = 0.060F * 0.85F;
  static const Render::Creature::StaticAttachmentSpec k_helmet_spec =
      Render::GL::roman_heavy_helmet_make_static_attachment(
          k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix);
  static const Render::Creature::StaticAttachmentSpec k_shoulder_l_spec =
      Render::GL::roman_shoulder_cover_make_static_attachment(
          k_shoulder_l_bone, k_shoulder_base_role_byte,
          k_shoulder_l_bind_matrix);
  static const Render::Creature::StaticAttachmentSpec k_shoulder_r_spec =
      Render::GL::roman_shoulder_cover_make_static_attachment(
          k_shoulder_r_bone, k_shoulder_base_role_byte,
          k_shoulder_r_bind_matrix);
  static const Render::Creature::StaticAttachmentSpec k_shield_spec =
      Render::GL::roman_scutum_make_static_attachment(k_shield_base_role_byte);
  static const Render::Creature::StaticAttachmentSpec k_armor_spec =
      Render::GL::roman_heavy_armor_make_static_attachment(
          k_chest_bone, k_armor_base_role_byte);
  static const Render::Creature::StaticAttachmentSpec k_sword_spec =
      Render::GL::sword_make_static_attachment(k_canonical_sword_cfg,
                                               k_sword_base_role_byte);
  static const Render::Creature::StaticAttachmentSpec k_scabbard_spec =
      Render::GL::scabbard_make_static_attachment(
          k_canonical_sheath_r, k_hip_l_bone, k_scabbard_base_role_byte);
  static const std::array<Render::Creature::StaticAttachmentSpec, 7>
      k_attachments{k_helmet_spec,  k_shoulder_l_spec, k_shoulder_r_spec,
                    k_shield_spec,  k_armor_spec,      k_sword_spec,
                    k_scabbard_spec};
  static const auto k_id =
      Render::Creature::ArchetypeRegistry::instance().register_unit_archetype(
          "troops/roman/horse_swordsman/rider",
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
            count += Render::GL::roman_heavy_helmet_fill_role_colors(
                v.palette, out + count, max_count - count);
            if (max_count <= count) {
              return count;
            }
            count += Render::GL::roman_shoulder_cover_fill_role_colors(
                v.palette, out + count, max_count - count);
            if (max_count <= count) {
              return count;
            }
            count += Render::GL::roman_scutum_fill_role_colors(
                v.palette, out + count, max_count - count);
            if (max_count <= count) {
              return count;
            }
            count += Render::GL::roman_heavy_armor_fill_role_colors(
                v.palette, out + count, max_count - count);
            if (max_count <= count) {
              return count;
            }
            count += Render::GL::sword_fill_role_colors(
                v.palette, k_canonical_sword_cfg, out + count,
                max_count - count);
            if (max_count <= count) {
              return count;
            }
            count += Render::GL::scabbard_fill_role_colors(
                v.palette, out + count, max_count - count);
            return count;
          });
  return k_id;
}

auto make_mounted_knight_config() -> MountedKnightRendererConfig {
  MountedKnightRendererConfig config;
  config.sword_equipment_id = "sword_roman";
  config.shield_equipment_id = "roman_scutum";
  config.armor_equipment_id = "roman_heavy_armor";
  config.shoulder_equipment_id = "roman_shoulder_cover_cavalry";
  config.has_shoulder = true;
  config.rider_creature_asset_id =
      Render::Creature::Pipeline::kHumanoidSwordAsset;
  config.rider_archetype_id = register_horse_swordsman_rider_archetype();

  static const auto k_mount_archetype = register_mount_saddle_archetype(
      "troops/roman/horse_swordsman/mount",
      &roman_saddle_make_static_attachment, &roman_saddle_fill_role_colors);
  config.mount_archetype_id = k_mount_archetype;
  return config;
}

} // namespace

void register_mounted_knight_renderer(EntityRendererRegistry &registry) {
  registry.register_renderer(
      "troops/roman/horse_swordsman",
      [](const DrawContext &ctx, ISubmitter &out) {
        static MountedKnightRendererBase const static_renderer(
            make_mounted_knight_config());
        static_renderer.render(ctx, out);
      });
}

} // namespace Render::GL::Roman
