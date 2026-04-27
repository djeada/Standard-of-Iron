#include "horse_spearman_renderer.h"

#include "../../../../render/creature/archetype_registry.h"
#include "../../../../render/creature/pipeline/unit_visual_spec.h"
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
#include "../../../equipment/weapons/spear_renderer.h"
#include "../../../submitter.h"
#include "../../horse_spearman_renderer_base.h"

#include <QVector3D>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace Render::GL::Roman {
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
      static_cast<std::uint8_t>(Render::Humanoid::kHumanoidRoleCount + 1U);
  static const auto k_shoulder_base_role_byte = static_cast<std::uint8_t>(
      k_helmet_base_role_byte + Render::GL::kRomanHeavyHelmetRoleCount);
  static const auto k_spear_base_role_byte = static_cast<std::uint8_t>(
      k_shoulder_base_role_byte + Render::GL::kRomanShoulderCoverRoleCount);
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
  static const std::array<Render::Creature::StaticAttachmentSpec, 4>
      k_spear_specs = Render::GL::spear_make_static_attachments(
          k_canonical_spear_cfg, k_spear_base_role_byte);
  static const Render::Creature::StaticAttachmentSpec k_armor_spec =
      Render::GL::roman_heavy_armor_make_static_attachment(
          k_chest_bone, k_armor_base_role_byte);
  static const std::array<Render::Creature::StaticAttachmentSpec, 8>
      k_attachments{k_helmet_spec,    k_shoulder_l_spec, k_shoulder_r_spec,
                    k_spear_specs[0], k_spear_specs[1],  k_spear_specs[2],
                    k_spear_specs[3], k_armor_spec};
  static const auto k_id =
      Render::Creature::ArchetypeRegistry::instance().register_unit_archetype(
          "troops/roman/horse_spearman/rider",
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
            count += Render::GL::spear_fill_role_colors(
                v.palette, k_canonical_spear_cfg, out + count,
                max_count - count);
            if (max_count <= count) {
              return count;
            }
            count += Render::GL::roman_heavy_armor_fill_role_colors(
                v.palette, out + count, max_count - count);
            return count;
          });
  return k_id;
}

auto make_horse_spearman_config() -> HorseSpearmanRendererConfig {
  HorseSpearmanRendererConfig config;
  config.spear_equipment_id = "spear";
  config.armor_equipment_id = "roman_heavy_armor";
  config.shoulder_equipment_id = "roman_shoulder_cover_cavalry";
  config.has_shoulder = true;
  config.rider_archetype_id = register_horse_spearman_rider_archetype();
  static const auto k_mount_archetype = register_mount_saddle_archetype(
      "troops/roman/horse_spearman/mount", &roman_saddle_make_static_attachment,
      &roman_saddle_fill_role_colors);
  config.mount_archetype_id = k_mount_archetype;
  return config;
}

} // namespace

void register_horse_spearman_renderer(EntityRendererRegistry &registry) {
  registry.register_renderer(
      "troops/roman/horse_spearman",
      [](const DrawContext &ctx, ISubmitter &out) {
        static HorseSpearmanRendererBase const static_renderer(
            make_horse_spearman_config());
        static_renderer.render(ctx, out);
      });
}

} // namespace Render::GL::Roman
