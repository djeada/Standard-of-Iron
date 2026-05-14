#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstddef>
#include <cstdint>

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../../render_archetype.h"
#include "../../static_attachment_spec.h"
#include "../i_equipment_renderer.h"

namespace Render::GL {

struct RomanHeavyHelmetConfig {};

auto roman_heavy_helmet_archetype() -> const RenderArchetype&;

inline constexpr std::uint32_t k_roman_heavy_helmet_role_count = 4;

auto roman_heavy_helmet_fill_role_colors(const HumanoidPalette& palette,
                                         QVector3D* out,
                                         std::size_t max) -> std::uint32_t;

auto roman_heavy_helmet_make_static_attachment(
    std::uint16_t socket_bone_index,
    std::uint8_t base_role_byte,
    const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec;

class RomanHeavyHelmetRenderer : public IEquipmentRenderer {
public:
  explicit RomanHeavyHelmetRenderer(const RomanHeavyHelmetConfig& cfg = {})
      : m_config(cfg) {}

  static void submit(const RomanHeavyHelmetConfig& config,
                     const DrawContext& ctx,
                     const BodyFrames& frames,
                     const HumanoidPalette& palette,
                     const HumanoidAnimationContext& anim,
                     EquipmentBatch& batch);

  void render(const DrawContext& ctx,
              const BodyFrames& frames,
              const HumanoidPalette& palette,
              const HumanoidAnimationContext& anim,
              EquipmentBatch& batch) override;

private:
  RomanHeavyHelmetConfig m_config;
};

} // namespace Render::GL
