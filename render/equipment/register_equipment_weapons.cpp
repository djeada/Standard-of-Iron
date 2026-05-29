#include "register_equipment_internal.h"
#include "weapons/bow_renderer.h"
#include "weapons/quiver_renderer.h"
#include "weapons/roman_scutum.h"
#include "weapons/shield_carthage.h"
#include "weapons/spear_renderer.h"
#include "weapons/sword_carthage.h"
#include "weapons/sword_renderer.h"
#include "weapons/sword_roman.h"

namespace Render::GL::EquipmentRegistration {

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

} // namespace Render::GL::EquipmentRegistration
