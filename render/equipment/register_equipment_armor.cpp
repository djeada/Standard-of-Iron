#include "armor/arm_guards_renderer.h"
#include "armor/armor_heavy_carthage.h"
#include "armor/armor_light_carthage.h"
#include "armor/carthage_shoulder_cover.h"
#include "armor/cloak_renderer.h"
#include "armor/roman_armor.h"
#include "armor/roman_greaves.h"
#include "armor/roman_shoulder_cover.h"
#include "armor/tool_belt_renderer.h"
#include "armor/work_apron_renderer.h"
#include "register_equipment_internal.h"

namespace Render::GL::EquipmentRegistration {

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

} // namespace Render::GL::EquipmentRegistration
