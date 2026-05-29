#include "helmets/carthage_heavy_helmet.h"
#include "helmets/carthage_light_helmet.h"
#include "helmets/headwrap.h"
#include "helmets/roman_heavy_helmet.h"
#include "helmets/roman_light_helmet.h"
#include "register_equipment_internal.h"

namespace Render::GL::EquipmentRegistration {

auto build_carthage_light_helmet_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_shell_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_neck_guard_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_cheek_guards_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_nasal_guard_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_crest_archetype(true),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_light_helmet_make_static_attachment(
          Render::GL::carthage_light_helmet_rivets_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
  };
}

auto build_carthage_heavy_helmet_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {
      Render::GL::carthage_heavy_helmet_make_static_attachment(
          Render::GL::carthage_heavy_helmet_shell_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_heavy_helmet_make_static_attachment(
          Render::GL::carthage_heavy_helmet_neck_guard_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_heavy_helmet_make_static_attachment(
          Render::GL::carthage_heavy_helmet_cheek_guards_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_heavy_helmet_make_static_attachment(
          Render::GL::carthage_heavy_helmet_face_plate_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_heavy_helmet_make_static_attachment(
          Render::GL::carthage_heavy_helmet_crest_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
      Render::GL::carthage_heavy_helmet_make_static_attachment(
          Render::GL::carthage_heavy_helmet_rivets_archetype(),
          humanoid_head_bone(),
          base_role_byte,
          humanoid_head_bind_matrix()),
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

auto build_headwrap_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec> {
  return {Render::GL::headwrap_make_static_attachment(
      humanoid_head_bone(), base_role_byte, humanoid_head_bind_matrix())};
}
auto carthage_light_helmet_role_colors(const void* variant_void,
                                       QVector3D* out,
                                       std::uint32_t base_count,
                                       std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::carthage_light_helmet_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto carthage_heavy_helmet_role_colors(const void* variant_void,
                                       QVector3D* out,
                                       std::uint32_t base_count,
                                       std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::carthage_heavy_helmet_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto roman_heavy_helmet_role_colors(const void* variant_void,
                                    QVector3D* out,
                                    std::uint32_t base_count,
                                    std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::roman_heavy_helmet_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

auto roman_light_helmet_role_colors(const void* variant_void,
                                    QVector3D* out,
                                    std::uint32_t base_count,
                                    std::size_t max_count) -> std::uint32_t {
  return with_variant_palette(
      variant_void,
      [](const HumanoidVariant& variant,
         QVector3D* colors,
         std::uint32_t count,
         std::size_t max) {
        return count + Render::GL::roman_light_helmet_fill_role_colors(
                           variant.palette, colors + count, max - count);
      },
      out,
      base_count,
      max_count);
}

} // namespace Render::GL::EquipmentRegistration
