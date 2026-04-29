#include "horse_archer_renderer.h"

#include "../../../equipment/armor/armor_light_carthage.h"
#include "../../../equipment/armor/cloak_renderer.h"
#include "../../../equipment/equipment_registry.h"
#include "../../../equipment/helmets/carthage_light_helmet.h"
#include "../../../equipment/horse/horse_attachment_archetype.h"
#include "../../../equipment/horse/saddles/carthage_saddle_renderer.h"
#include "../../../equipment/horse/saddles/horse_mount_archetype.h"
#include "../../../humanoid/humanoid_renderer_base.h"
#include "../../../humanoid/humanoid_spec.h"
#include "../../../humanoid/skeleton.h"
#include "../../../submitter.h"
#include "../../horse_archer_renderer_base.h"

#include <memory>

namespace Render::GL::Carthage {
namespace {

auto register_horse_archer_rider_archetype() -> Render::Creature::ArchetypeId {
  static const auto k_head_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head);
  static const auto k_chest_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest);
  static const auto k_helmet_base_role_byte =
      static_cast<std::uint8_t>(Render::Humanoid::k_humanoid_role_count + 1U);
  static const auto k_armor_base_role_byte = static_cast<std::uint8_t>(
      k_helmet_base_role_byte + Render::GL::kCarthageLightHelmetRoleCount);
  static const auto k_cloak_base_role_byte = static_cast<std::uint8_t>(
      k_armor_base_role_byte + Render::GL::kArmorLightCarthageRoleCount);
  static const auto k_head_bind_matrix =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
          Render::Humanoid::HumanoidBone::Head)];
  static const CloakConfig k_cloak_cfg = []() {
    CloakConfig cfg;
    cfg.primary_color = QVector3D(0.14F, 0.38F, 0.54F);
    return cfg;
  }();
  static const CloakMeshes k_cloak_meshes = []() -> CloakMeshes {
    auto &reg = Render::GL::EquipmentRegistry::instance();
    auto cloak_inst =
        reg.get(Render::GL::EquipmentCategory::Armor, "cloak_carthage");
    if (cloak_inst) {
      if (auto *cr = dynamic_cast<CloakRenderer *>(cloak_inst.get())) {
        return cr->meshes();
      }
    }
    return CloakMeshes{};
  }();
  static const std::array<Render::Creature::StaticAttachmentSpec, 8>
      k_attachments{
          Render::GL::carthage_light_helmet_make_static_attachment(
              Render::GL::carthage_light_helmet_shell_archetype(), k_head_bone,
              k_helmet_base_role_byte, k_head_bind_matrix),
          Render::GL::carthage_light_helmet_make_static_attachment(
              Render::GL::carthage_light_helmet_neck_guard_archetype(),
              k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix),
          Render::GL::carthage_light_helmet_make_static_attachment(
              Render::GL::carthage_light_helmet_cheek_guards_archetype(),
              k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix),
          Render::GL::carthage_light_helmet_make_static_attachment(
              Render::GL::carthage_light_helmet_nasal_guard_archetype(),
              k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix),
          Render::GL::carthage_light_helmet_make_static_attachment(
              Render::GL::carthage_light_helmet_crest_archetype(true),
              k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix),
          Render::GL::carthage_light_helmet_make_static_attachment(
              Render::GL::carthage_light_helmet_rivets_archetype(), k_head_bone,
              k_helmet_base_role_byte, k_head_bind_matrix),
          Render::GL::armor_light_carthage_make_static_attachment(
              k_chest_bone, k_armor_base_role_byte),
          Render::GL::cloak_make_static_attachment(k_cloak_cfg, k_cloak_meshes,
                                                   k_chest_bone,
                                                   k_cloak_base_role_byte),
      };
  static const auto k_id =
      Render::Creature::ArchetypeRegistry::instance().register_unit_archetype(
          "troops/carthage/horse_archer/rider",
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
            count += Render::GL::carthage_light_helmet_fill_role_colors(
                v.palette, out + count, max_count - count);
            if (max_count <= count) {
              return count;
            }
            count += Render::GL::armor_light_carthage_fill_role_colors(
                v.palette, out + count, max_count - count);
            if (max_count <= count) {
              return count;
            }
            count += Render::GL::cloak_fill_role_colors_with_primary(
                QVector3D(0.14F, 0.38F, 0.54F), v.palette, out + count,
                max_count - count);
            return count;
          });
  return k_id;
}

auto make_horse_archer_config() -> HorseArcherRendererConfig {
  HorseArcherRendererConfig config;
  config.bow_equipment_id = "bow_carthage";
  config.quiver_equipment_id = "quiver";
  config.helmet_equipment_id = "carthage_light";
  config.armor_equipment_id = "armor_light_carthage";
  config.cloak_equipment_id = "cloak_carthage";
  config.has_cloak = true;
  config.cloak_color = {0.14F, 0.38F, 0.54F};
  config.cloak_trim_color = {0.75F, 0.66F, 0.42F};
  config.cloak_back_material_id = 12;
  config.cloak_shoulder_material_id = 13;
  config.helmet_offset_moving = 0.035F;
  config.fletching_color = {0.85F, 0.40F, 0.40F};
  config.rider_archetype_id = register_horse_archer_rider_archetype();
  static const auto k_mount_archetype =
      register_mount_saddle_archetype("troops/carthage/horse_archer/mount",
                                      &carthage_saddle_make_static_attachment,
                                      &carthage_saddle_fill_role_colors);
  config.mount_archetype_id = k_mount_archetype;
  return config;
}

} // namespace

void register_horse_archer_renderer(EntityRendererRegistry &registry) {
  registry.register_renderer(
      "troops/carthage/horse_archer",
      [](const DrawContext &ctx, ISubmitter &out) {
        static HorseArcherRendererBase const static_renderer(
            make_horse_archer_config());
        static_renderer.render(ctx, out);
      });
}

} // namespace Render::GL::Carthage
