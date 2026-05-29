#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "../horse/dimensions.h"
#include "../horse/horse_spec.h"
#include "../humanoid/humanoid_spec.h"
#include "../humanoid/skeleton.h"
#include "armor/shoulder_cover_archetype.h"
#include "equipment_registry.h"
#include "horse_equipment_archetype.h"
#include "humanoid_equipment_archetype.h"

namespace Render::GL::EquipmentRegistration {

using StaticAttachmentSpec = Render::Creature::StaticAttachmentSpec;

template <std::size_t N>
auto to_vector(const std::array<StaticAttachmentSpec, N>& attachments)
    -> std::vector<StaticAttachmentSpec> {
  return {attachments.begin(), attachments.end()};
}

auto humanoid_head_bone() -> std::uint16_t;
auto humanoid_chest_bone() -> std::uint16_t;
auto humanoid_pelvis_bone() -> std::uint16_t;
auto humanoid_foot_l_bone() -> std::uint16_t;
auto humanoid_foot_r_bone() -> std::uint16_t;
auto humanoid_hip_l_bone() -> std::uint16_t;
auto humanoid_shoulder_l_bone() -> std::uint16_t;
auto humanoid_shoulder_r_bone() -> std::uint16_t;
auto humanoid_forearm_l_bone() -> std::uint16_t;
auto humanoid_forearm_r_bone() -> std::uint16_t;
auto humanoid_head_bind_matrix() -> const QMatrix4x4&;
auto humanoid_shoulder_bind_matrix(bool left) -> const QMatrix4x4&;
auto humanoid_shin_bind_matrix(bool left) -> const QMatrix4x4&;
auto horse_root_bone() -> std::uint16_t;
auto horse_head_bone() -> std::uint16_t;
auto horse_neck_top_bone() -> std::uint16_t;
auto horse_root_bind_matrix() -> const QMatrix4x4&;
auto horse_head_bind_matrix() -> const QMatrix4x4&;
auto horse_neck_top_bind_matrix() -> const QMatrix4x4&;

template <typename Fn>
auto with_variant_palette(const void* variant_void,
                          Fn&& fn,
                          QVector3D* out,
                          std::uint32_t base_count,
                          std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  auto clamp_color = [](const QVector3D& color) -> QVector3D {
    return {std::clamp(color.x(), 0.0F, 1.0F),
            std::clamp(color.y(), 0.0F, 1.0F),
            std::clamp(color.z(), 0.0F, 1.0F)};
  };
  auto hash01 = [](float value) -> float {
    return value - std::floor(value);
  };
  auto worn_equipment_color = [&](const HumanoidVariant& variant,
                                  const QVector3D& base,
                                  std::uint32_t slot_index) -> QVector3D {
    float const max_component = std::max({base.x(), base.y(), base.z()});
    float const min_component = std::min({base.x(), base.y(), base.z()});
    float const brightness = (base.x() + base.y() + base.z()) / 3.0F;
    float const saturation = max_component - min_component;
    float const grayscale = brightness;
    float const metallicness =
        std::clamp((max_component - 0.22F) * 1.6F, 0.0F, 1.0F) *
        (1.0F - std::clamp((saturation - 0.08F) * 2.8F, 0.0F, 1.0F));
    float const leatherness =
        (1.0F - metallicness) *
        std::clamp((base.x() - base.z()) * 1.6F + 0.15F, 0.0F, 1.0F);
    float const clothness = std::clamp(1.0F - metallicness * 0.75F, 0.0F, 1.0F);

    float const seed =
        variant.pattern_seed * 131.0F + static_cast<float>(slot_index) * 17.0F;
    float const wear_noise = hash01(std::sin(seed + 0.37F) * 43758.5453F);
    float const grime_noise = hash01(std::sin(seed * 1.73F + 1.91F) * 24634.6345F);
    float const blood_noise = hash01(std::sin(seed * 2.17F + 5.13F) * 15384.1837F);
    float const fade_noise = hash01(std::sin(seed * 2.81F + 0.83F) * 31415.9265F);
    float const contrast_noise = hash01(std::sin(seed * 3.71F + 2.63F) * 27182.8183F);

    float const wear =
        std::clamp(variant.weathering * (0.75F + 0.55F * wear_noise), 0.0F, 1.0F);
    float const grime =
        std::clamp(variant.grime * (0.65F + 0.65F * grime_noise), 0.0F, 1.0F);
    float const blood = std::clamp(
        variant.bloodiness * std::max(0.0F, blood_noise - 0.58F) / 0.42F, 0.0F, 1.0F);
    float const fading = wear * (0.45F + 0.55F * fade_noise);
    float const slot_variation = 0.82F + 0.36F * contrast_noise;

    QVector3D color = base;
    QVector3D const grayscale_vec(grayscale, grayscale, grayscale);
    color *= QVector3D(slot_variation, slot_variation, slot_variation);
    color = color * (1.0F - wear * (0.18F + 0.18F * clothness + 0.10F * leatherness));
    color += (grayscale_vec - color) * (wear * (0.28F + 0.28F * metallicness));
    color += (color * QVector3D(0.78F, 0.72F, 0.62F) - color) *
             (fading * (0.30F * clothness + 0.12F * leatherness));
    color *= 1.0F - grime * (0.24F + 0.18F * leatherness + 0.16F * clothness);
    color += (color * QVector3D(0.56F, 0.72F, 0.52F) - color) *
             (wear * metallicness * 0.34F);
    color += (color * QVector3D(0.70F, 0.54F, 0.40F) - color) *
             (grime * leatherness * 0.28F);
    color += (QVector3D(0.45F, 0.08F, 0.06F) - color) *
             (blood * (0.26F + 0.50F * clothness + 0.22F * leatherness));
    return clamp_color(color);
  };

  auto const& variant = *static_cast<const HumanoidVariant*>(variant_void);
  std::uint32_t const count = fn(variant, out, base_count, max_count);
  for (std::uint32_t i = base_count; i < count; ++i) {
    out[i] = worn_equipment_color(variant, out[i], i - base_count);
  }
  return count;
}

template <typename Fn>
auto with_horse_variant(const void* variant_void,
                        Fn&& fn,
                        QVector3D* out,
                        std::uint32_t base_count,
                        std::size_t max_count) -> std::uint32_t {
  if (variant_void == nullptr || max_count <= base_count) {
    return base_count;
  }
  return fn(
      *static_cast<const HorseVariant*>(variant_void), out, base_count, max_count);
}

void register_equipment_ids(EquipmentRegistry& registry);
void register_equipment_descriptors();
void register_equipment_archetypes();

auto build_carthage_light_helmet_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_carthage_heavy_helmet_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_roman_heavy_helmet_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_roman_light_helmet_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_headwrap_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_roman_light_armor_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_roman_heavy_armor_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_carthage_light_armor_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_carthage_heavy_armor_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_roman_greaves_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_roman_shoulder_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_carthage_shoulder_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_tool_belt_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_work_apron_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_arm_guards_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_spear_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_fabius_spear_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_hanno_spear_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_roman_scutum_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_carthage_shield_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_roman_sword_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_scipio_sword_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_carthage_sword_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_hannibal_sword_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_sepulcher_sword_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_roman_bow_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_marcellus_bow_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_carthage_bow_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_hasdrubal_bow_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_quiver_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_roman_quiver_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_marcellus_quiver_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_carthage_quiver_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_hasdrubal_quiver_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_carthage_cloak_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_carthage_mounted_cloak_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_sepulcher_cloak_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_roman_cloak_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_roman_mounted_cloak_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_roman_horse_saddle_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_carthage_horse_saddle_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_light_cavalry_saddle_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_horse_bridle_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_horse_reins_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_horse_blanket_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_leather_barding_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_scale_barding_attachments(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_champion_barding_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_crupper_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto build_saddle_bag_attachment(std::uint8_t base_role_byte)
    -> std::vector<StaticAttachmentSpec>;
auto roman_horse_saddle_role_colors(const void* variant_void,
                                    QVector3D* out,
                                    std::uint32_t base_count,
                                    std::size_t max_count) -> std::uint32_t;
auto carthage_horse_saddle_role_colors(const void* variant_void,
                                       QVector3D* out,
                                       std::uint32_t base_count,
                                       std::size_t max_count) -> std::uint32_t;
auto light_cavalry_saddle_role_colors(const void* variant_void,
                                      QVector3D* out,
                                      std::uint32_t base_count,
                                      std::size_t max_count) -> std::uint32_t;
auto horse_bridle_role_colors(const void* variant_void,
                              QVector3D* out,
                              std::uint32_t base_count,
                              std::size_t max_count) -> std::uint32_t;
auto horse_reins_role_colors(const void* variant_void,
                             QVector3D* out,
                             std::uint32_t base_count,
                             std::size_t max_count) -> std::uint32_t;
auto horse_blanket_role_colors(const void* variant_void,
                               QVector3D* out,
                               std::uint32_t base_count,
                               std::size_t max_count) -> std::uint32_t;
auto leather_barding_role_colors(const void* variant_void,
                                 QVector3D* out,
                                 std::uint32_t base_count,
                                 std::size_t max_count) -> std::uint32_t;
auto scale_barding_role_colors(const void* variant_void,
                               QVector3D* out,
                               std::uint32_t base_count,
                               std::size_t max_count) -> std::uint32_t;
auto champion_barding_role_colors(const void* variant_void,
                                  QVector3D* out,
                                  std::uint32_t base_count,
                                  std::size_t max_count) -> std::uint32_t;
auto horse_crupper_role_colors(const void* variant_void,
                               QVector3D* out,
                               std::uint32_t base_count,
                               std::size_t max_count) -> std::uint32_t;
auto saddle_bag_role_colors(const void* variant_void,
                            QVector3D* out,
                            std::uint32_t base_count,
                            std::size_t max_count) -> std::uint32_t;
auto carthage_light_helmet_role_colors(const void* variant_void,
                                       QVector3D* out,
                                       std::uint32_t base_count,
                                       std::size_t max_count) -> std::uint32_t;
auto carthage_heavy_helmet_role_colors(const void* variant_void,
                                       QVector3D* out,
                                       std::uint32_t base_count,
                                       std::size_t max_count) -> std::uint32_t;
auto roman_heavy_helmet_role_colors(const void* variant_void,
                                    QVector3D* out,
                                    std::uint32_t base_count,
                                    std::size_t max_count) -> std::uint32_t;
auto roman_light_helmet_role_colors(const void* variant_void,
                                    QVector3D* out,
                                    std::uint32_t base_count,
                                    std::size_t max_count) -> std::uint32_t;
auto roman_light_armor_role_colors(const void* variant_void,
                                   QVector3D* out,
                                   std::uint32_t base_count,
                                   std::size_t max_count) -> std::uint32_t;
auto roman_heavy_armor_role_colors(const void* variant_void,
                                   QVector3D* out,
                                   std::uint32_t base_count,
                                   std::size_t max_count) -> std::uint32_t;
auto carthage_light_armor_role_colors(const void* variant_void,
                                      QVector3D* out,
                                      std::uint32_t base_count,
                                      std::size_t max_count) -> std::uint32_t;
auto carthage_heavy_armor_role_colors(const void* variant_void,
                                      QVector3D* out,
                                      std::uint32_t base_count,
                                      std::size_t max_count) -> std::uint32_t;
auto roman_greaves_role_colors(const void* variant_void,
                               QVector3D* out,
                               std::uint32_t base_count,
                               std::size_t max_count) -> std::uint32_t;
auto roman_shoulder_role_colors(const void* variant_void,
                                QVector3D* out,
                                std::uint32_t base_count,
                                std::size_t max_count) -> std::uint32_t;
auto carthage_shoulder_role_colors(const void* variant_void,
                                   QVector3D* out,
                                   std::uint32_t base_count,
                                   std::size_t max_count) -> std::uint32_t;
auto spear_role_colors(const void* variant_void,
                       QVector3D* out,
                       std::uint32_t base_count,
                       std::size_t max_count) -> std::uint32_t;
auto fabius_spear_role_colors(const void* variant_void,
                              QVector3D* out,
                              std::uint32_t base_count,
                              std::size_t max_count) -> std::uint32_t;
auto hanno_spear_role_colors(const void* variant_void,
                             QVector3D* out,
                             std::uint32_t base_count,
                             std::size_t max_count) -> std::uint32_t;
auto roman_scutum_role_colors(const void* variant_void,
                              QVector3D* out,
                              std::uint32_t base_count,
                              std::size_t max_count) -> std::uint32_t;
auto carthage_shield_role_colors(const void* variant_void,
                                 QVector3D* out,
                                 std::uint32_t base_count,
                                 std::size_t max_count) -> std::uint32_t;
auto roman_sword_role_colors(const void* variant_void,
                             QVector3D* out,
                             std::uint32_t base_count,
                             std::size_t max_count) -> std::uint32_t;
auto scipio_sword_role_colors(const void* variant_void,
                              QVector3D* out,
                              std::uint32_t base_count,
                              std::size_t max_count) -> std::uint32_t;
auto carthage_sword_role_colors(const void* variant_void,
                                QVector3D* out,
                                std::uint32_t base_count,
                                std::size_t max_count) -> std::uint32_t;
auto hannibal_sword_role_colors(const void* variant_void,
                                QVector3D* out,
                                std::uint32_t base_count,
                                std::size_t max_count) -> std::uint32_t;
auto sepulcher_sword_role_colors(const void* variant_void,
                                 QVector3D* out,
                                 std::uint32_t base_count,
                                 std::size_t max_count) -> std::uint32_t;
auto bow_role_colors(const void* variant_void,
                     QVector3D* out,
                     std::uint32_t base_count,
                     std::size_t max_count) -> std::uint32_t;
auto marcellus_bow_role_colors(const void* variant_void,
                               QVector3D* out,
                               std::uint32_t base_count,
                               std::size_t max_count) -> std::uint32_t;
auto hasdrubal_bow_role_colors(const void* variant_void,
                               QVector3D* out,
                               std::uint32_t base_count,
                               std::size_t max_count) -> std::uint32_t;
auto quiver_role_colors(const void* variant_void,
                        QVector3D* out,
                        std::uint32_t base_count,
                        std::size_t max_count) -> std::uint32_t;
auto marcellus_quiver_role_colors(const void* variant_void,
                                  QVector3D* out,
                                  std::uint32_t base_count,
                                  std::size_t max_count) -> std::uint32_t;
auto roman_quiver_role_colors(const void* variant_void,
                              QVector3D* out,
                              std::uint32_t base_count,
                              std::size_t max_count) -> std::uint32_t;
auto hasdrubal_quiver_role_colors(const void* variant_void,
                                  QVector3D* out,
                                  std::uint32_t base_count,
                                  std::size_t max_count) -> std::uint32_t;
auto carthage_quiver_role_colors(const void* variant_void,
                                 QVector3D* out,
                                 std::uint32_t base_count,
                                 std::size_t max_count) -> std::uint32_t;
auto carthage_cloak_role_colors(const void* variant_void,
                                QVector3D* out,
                                std::uint32_t base_count,
                                std::size_t max_count) -> std::uint32_t;
auto sepulcher_cloak_role_colors(const void* variant_void,
                                 QVector3D* out,
                                 std::uint32_t base_count,
                                 std::size_t max_count) -> std::uint32_t;
auto roman_cloak_role_colors(const void* variant_void,
                             QVector3D* out,
                             std::uint32_t base_count,
                             std::size_t max_count) -> std::uint32_t;
auto roman_tool_belt_role_colors(const void* variant_void,
                                 QVector3D* out,
                                 std::uint32_t base_count,
                                 std::size_t max_count) -> std::uint32_t;
auto carthage_tool_belt_role_colors(const void* variant_void,
                                    QVector3D* out,
                                    std::uint32_t base_count,
                                    std::size_t max_count) -> std::uint32_t;
auto roman_work_apron_role_colors(const void* variant_void,
                                  QVector3D* out,
                                  std::uint32_t base_count,
                                  std::size_t max_count) -> std::uint32_t;
auto carthage_work_apron_role_colors(const void* variant_void,
                                     QVector3D* out,
                                     std::uint32_t base_count,
                                     std::size_t max_count) -> std::uint32_t;
auto roman_arm_guards_role_colors(const void* variant_void,
                                  QVector3D* out,
                                  std::uint32_t base_count,
                                  std::size_t max_count) -> std::uint32_t;
auto carthage_arm_guards_role_colors(const void* variant_void,
                                     QVector3D* out,
                                     std::uint32_t base_count,
                                     std::size_t max_count) -> std::uint32_t;

} // namespace Render::GL::EquipmentRegistration
