#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../../static_attachment_spec.h"
#include "../i_equipment_renderer.h"

#include <QVector3D>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

struct ArmorHeavyCarthageConfig {};

inline constexpr std::uint32_t kArmorHeavyCarthageRoleCount = 3;

auto armor_heavy_carthage_fill_role_colors(const HumanoidPalette &palette,
                                           QVector3D *out,
                                           std::size_t max) -> std::uint32_t;

auto armor_heavy_carthage_make_static_attachment(
    std::uint16_t torso_socket_bone_index,
    std::uint8_t base_role_byte) -> Render::Creature::StaticAttachmentSpec;

class ArmorHeavyCarthageRenderer : public IEquipmentRenderer {
public:
  ArmorHeavyCarthageRenderer() = default;

  static void submit(const ArmorHeavyCarthageConfig &config,
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
