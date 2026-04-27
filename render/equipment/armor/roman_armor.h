#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../../static_attachment_spec.h"
#include "../i_equipment_renderer.h"

#include <QVector3D>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

struct RomanHeavyArmorConfig {};
struct RomanLightArmorConfig {};

inline constexpr std::uint32_t kRomanHeavyArmorRoleCount = 4;

auto roman_heavy_armor_fill_role_colors(const HumanoidPalette &palette,
                                        QVector3D *out,
                                        std::size_t max) -> std::uint32_t;

auto roman_heavy_armor_make_static_attachment(
    std::uint16_t torso_socket_bone_index,
    std::uint8_t base_role_byte) -> Render::Creature::StaticAttachmentSpec;

inline constexpr std::uint32_t kRomanLightArmorRoleCount = 3;

auto roman_light_armor_fill_role_colors(const HumanoidPalette &palette,
                                        QVector3D *out,
                                        std::size_t max) -> std::uint32_t;

auto roman_light_armor_make_static_attachment(
    std::uint16_t torso_socket_bone_index,
    std::uint8_t base_role_byte) -> Render::Creature::StaticAttachmentSpec;

class RomanHeavyArmorRenderer : public IEquipmentRenderer {
public:
  RomanHeavyArmorRenderer() = default;

  static void submit(const RomanHeavyArmorConfig &config,
                     const DrawContext &ctx, const BodyFrames &frames,
                     const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;
};

class RomanLightArmorRenderer : public IEquipmentRenderer {
public:
  RomanLightArmorRenderer() = default;

  static void submit(const RomanLightArmorConfig &config,
                     const DrawContext &ctx, const BodyFrames &frames,
                     const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;
};

} // namespace Render::GL
