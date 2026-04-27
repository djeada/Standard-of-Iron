#include "horse_swordsman_renderer.h"

#include "../../../creature/pipeline/creature_asset.h"
#include "../../../equipment/armor/armor_heavy_carthage.h"
#include "../../../equipment/armor/carthage_shoulder_cover.h"
#include "../../../equipment/armor/shoulder_cover_archetype.h"
#include "../../../equipment/helmets/carthage_heavy_helmet.h"
#include "../../../equipment/horse/horse_attachment_archetype.h"
#include "../../../equipment/horse/saddles/carthage_saddle_renderer.h"
#include "../../../equipment/horse/saddles/horse_mount_archetype.h"
#include "../../../equipment/weapons/shield_carthage.h"
#include "../../../equipment/weapons/sword_renderer.h"
#include "../../../humanoid/humanoid_renderer_base.h"
#include "../../../humanoid/humanoid_spec.h"
#include "../../../humanoid/skeleton.h"
#include "../../../humanoid/style_palette.h"
#include "../../../submitter.h"
#include "../../mounted_knight_renderer_base.h"
#include "swordsman_style.h"

#include <memory>
#include <optional>

namespace Render::GL::Carthage {
namespace {

constexpr float k_team_mix_weight = 0.6F;
constexpr float k_style_mix_weight = 0.4F;

auto carthage_style() -> KnightStyleConfig {
  KnightStyleConfig style;
  style.cloth_color = QVector3D(0.15F, 0.36F, 0.55F);
  style.leather_color = QVector3D(0.32F, 0.22F, 0.12F);
  style.leather_dark_color = QVector3D(0.20F, 0.14F, 0.09F);
  style.metal_color = QVector3D(0.70F, 0.68F, 0.52F);
  return style;
}

class CarthageMountedKnightRenderer : public MountedKnightRendererBase {
public:
  using MountedKnightRendererBase::MountedKnightRendererBase;

  void get_variant(const DrawContext &ctx, uint32_t seed,
                   HumanoidVariant &v) const override {
    MountedKnightRendererBase::get_variant(ctx, seed, v);
    const KnightStyleConfig style = carthage_style();
    QVector3D const team_tint = resolve_team_tint(ctx);

    auto apply_color = [&](const std::optional<QVector3D> &override_color,
                           QVector3D &target) {
      target = Render::GL::Humanoid::mix_palette_color(
          target, override_color, team_tint, k_team_mix_weight,
          k_style_mix_weight);
    };

    apply_color(style.cloth_color, v.palette.cloth);
    apply_color(style.leather_color, v.palette.leather);
    apply_color(style.leather_dark_color, v.palette.leather_dark);
    apply_color(style.metal_color, v.palette.metal);
  }
};

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
  static const auto k_shield_base_role_byte = static_cast<std::uint8_t>(
      k_helmet_base_role_byte + Render::GL::kCarthageHeavyHelmetRoleCount);
  static const auto k_shoulder_base_role_byte = static_cast<std::uint8_t>(
      k_shield_base_role_byte + Render::GL::kCarthageShieldRoleCount);
  static const auto k_armor_base_role_byte = static_cast<std::uint8_t>(
      k_shoulder_base_role_byte + Render::GL::kCarthageShoulderCoverRoleCount);
  static const auto k_sword_base_role_byte = static_cast<std::uint8_t>(
      k_armor_base_role_byte + Render::GL::kArmorHeavyCarthageRoleCount);
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
  constexpr float k_canonical_sheath_r = 0.060F * 0.85F;
  static const std::array<Render::Creature::StaticAttachmentSpec, 12>
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
          Render::GL::carthage_shield_make_static_attachment(
              Render::GL::CarthageShieldConfig{}, k_shield_base_role_byte),
          Render::GL::carthage_shoulder_cover_make_static_attachment(
              k_shoulder_l_bone, k_shoulder_base_role_byte,
              k_shoulder_l_bind_matrix),
          Render::GL::carthage_shoulder_cover_make_static_attachment(
              k_shoulder_r_bone, k_shoulder_base_role_byte,
              k_shoulder_r_bind_matrix),
          Render::GL::armor_heavy_carthage_make_static_attachment(
              k_chest_bone, k_armor_base_role_byte),
          Render::GL::sword_make_static_attachment(k_canonical_sword_cfg,
                                                   k_sword_base_role_byte),
          Render::GL::scabbard_make_static_attachment(
              k_canonical_sheath_r, k_hip_l_bone, k_scabbard_base_role_byte),
      };
  static const auto k_id =
      Render::Creature::ArchetypeRegistry::instance().register_unit_archetype(
          "troops/carthage/horse_swordsman/rider",
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
            count += Render::GL::carthage_shield_fill_role_colors(
                v.palette, out + count, max_count - count);
            if (max_count <= count) {
              return count;
            }
            count += Render::GL::carthage_shoulder_cover_fill_role_colors(
                v.palette, out + count, max_count - count);
            if (max_count <= count) {
              return count;
            }
            count += Render::GL::armor_heavy_carthage_fill_role_colors(
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
  config.sword_equipment_id = "sword_carthage";
  config.shield_equipment_id = "shield_carthage";
  config.helmet_equipment_id = "carthage_heavy";
  config.armor_equipment_id = "armor_heavy_carthage";
  config.shoulder_equipment_id = "carthage_shoulder_cover_cavalry";
  config.metal_color = QVector3D(0.70F, 0.68F, 0.52F);
  config.has_shoulder = true;
  config.helmet_offset_moving = 0.03F;
  config.rider_creature_asset_id =
      Render::Creature::Pipeline::kHumanoidSwordAsset;
  config.rider_archetype_id = register_horse_swordsman_rider_archetype();
  static const auto k_mount_archetype =
      register_mount_saddle_archetype("troops/carthage/horse_swordsman/mount",
                                      &carthage_saddle_make_static_attachment,
                                      &carthage_saddle_fill_role_colors);
  config.mount_archetype_id = k_mount_archetype;
  return config;
}

} // namespace

void register_mounted_knight_renderer(EntityRendererRegistry &registry) {
  registry.register_renderer(
      "troops/carthage/horse_swordsman",
      [](const DrawContext &ctx, ISubmitter &out) {
        static CarthageMountedKnightRenderer const static_renderer(
            make_mounted_knight_config());
        static_renderer.render(ctx, out);
      });
}

} // namespace Render::GL::Carthage
