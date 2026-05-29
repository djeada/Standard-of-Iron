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
#include "register_equipment_internal.h"

namespace Render::GL::EquipmentRegistration {

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

} // namespace Render::GL::EquipmentRegistration
