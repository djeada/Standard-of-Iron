#include "horse_archer_renderer.h"

#include "../../../../render/creature/archetype_registry.h"
#include "../../../../render/creature/pipeline/unit_visual_spec.h"
#include "../../../../render/humanoid/humanoid_renderer_base.h"
#include "../../../../render/humanoid/humanoid_spec.h"
#include "../../../../render/humanoid/skeleton.h"
#include "../../../equipment/armor/cloak_renderer.h"
#include "../../../equipment/armor/roman_armor.h"
#include "../../../equipment/equipment_registry.h"
#include "../../../equipment/helmets/roman_light_helmet.h"
#include "../../../equipment/horse/horse_attachment_archetype.h"
#include "../../../equipment/horse/saddles/horse_mount_archetype.h"
#include "../../../equipment/horse/saddles/roman_saddle_renderer.h"
#include "../../../submitter.h"
#include "../../horse_archer_renderer_base.h"

#include <QVector3D>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace Render::GL::Roman {
namespace {

auto register_horse_archer_rider_archetype() -> Render::Creature::ArchetypeId {
  static const auto k_head_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head);
  static const auto k_chest_bone =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Chest);
  static const auto k_helmet_base_role_byte =
      static_cast<std::uint8_t>(Render::Humanoid::kHumanoidRoleCount + 1U);
  static const auto k_armor_base_role_byte = static_cast<std::uint8_t>(
      k_helmet_base_role_byte + Render::GL::kRomanLightHelmetRoleCount);
  static const auto k_cloak_base_role_byte = static_cast<std::uint8_t>(
      k_armor_base_role_byte + Render::GL::kRomanLightArmorRoleCount);
  static const auto k_head_bind_matrix =
      Render::Humanoid::humanoid_bind_palette()[static_cast<std::size_t>(
          Render::Humanoid::HumanoidBone::Head)];
  static const CloakConfig k_cloak_cfg = []() {
    CloakConfig cfg;
    cfg.primary_color = QVector3D(0.70F, 0.15F, 0.18F);
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
  static const Render::Creature::StaticAttachmentSpec k_helmet_spec =
      Render::GL::roman_light_helmet_make_static_attachment(
          k_head_bone, k_helmet_base_role_byte, k_head_bind_matrix);
  static const Render::Creature::StaticAttachmentSpec k_armor_spec =
      Render::GL::roman_light_armor_make_static_attachment(
          k_chest_bone, k_armor_base_role_byte);
  static const Render::Creature::StaticAttachmentSpec k_cloak_spec =
      Render::GL::cloak_make_static_attachment(
          k_cloak_cfg, k_cloak_meshes, k_chest_bone, k_cloak_base_role_byte);
  static const std::array<Render::Creature::StaticAttachmentSpec, 3>
      k_attachments{k_helmet_spec, k_armor_spec, k_cloak_spec};
  static const auto k_id =
      Render::Creature::ArchetypeRegistry::instance().register_unit_archetype(
          "troops/roman/horse_archer/rider",
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
            count += Render::GL::roman_light_helmet_fill_role_colors(
                v.palette, out + count, max_count - count);
            if (max_count <= count) {
              return count;
            }
            count += Render::GL::roman_light_armor_fill_role_colors(
                v.palette, out + count, max_count - count);
            if (max_count <= count) {
              return count;
            }
            count += Render::GL::cloak_fill_role_colors_with_primary(
                QVector3D(0.70F, 0.15F, 0.18F), v.palette, out + count,
                max_count - count);
            return count;
          });
  return k_id;
}

auto make_horse_archer_config() -> HorseArcherRendererConfig {
  HorseArcherRendererConfig config;
  config.bow_equipment_id = "bow_roman";
  config.quiver_equipment_id = "quiver";
  config.armor_equipment_id = "roman_light_armor";
  config.cloak_equipment_id = "cloak_carthage";
  config.has_cloak = true;
  config.cloak_color = {0.70F, 0.15F, 0.18F};
  config.cloak_trim_color = {0.78F, 0.72F, 0.58F};
  config.cloak_back_material_id = 12;
  config.cloak_shoulder_material_id = 13;
  config.fletching_color = {0.85F, 0.40F, 0.40F};
  config.rider_archetype_id = register_horse_archer_rider_archetype();
  static const auto k_mount_archetype = register_mount_saddle_archetype(
      "troops/roman/horse_archer/mount", &roman_saddle_make_static_attachment,
      &roman_saddle_fill_role_colors);
  config.mount_archetype_id = k_mount_archetype;
  return config;
}

} // namespace

void register_horse_archer_renderer(EntityRendererRegistry &registry) {
  registry.register_renderer(
      "troops/roman/horse_archer", [](const DrawContext &ctx, ISubmitter &out) {
        static HorseArcherRendererBase const static_renderer(
            make_horse_archer_config());
        static_renderer.render(ctx, out);
      });
}

} // namespace Render::GL::Roman
