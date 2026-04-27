#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../../render_archetype.h"
#include "../../static_attachment_spec.h"
#include "../i_equipment_renderer.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

struct CarthageLightHelmetConfig {
  QVector3D bronze_color = QVector3D(0.72F, 0.45F, 0.20F);
  QVector3D leather_color = QVector3D(0.35F, 0.25F, 0.18F);
  float helmet_height = 0.18F;
  float brim_width = 0.05F;
  float cheek_guard_length = 0.12F;
  bool has_crest = true;
  bool has_nasal_guard = true;
  int detail_level = 2;
};

auto carthage_light_helmet_shell_archetype() -> const RenderArchetype &;
auto carthage_light_helmet_neck_guard_archetype() -> const RenderArchetype &;
auto carthage_light_helmet_cheek_guards_archetype() -> const RenderArchetype &;
auto carthage_light_helmet_nasal_guard_archetype() -> const RenderArchetype &;
auto carthage_light_helmet_crest_archetype(bool high_detail)
    -> const RenderArchetype &;
auto carthage_light_helmet_rivets_archetype() -> const RenderArchetype &;

inline constexpr std::uint32_t kCarthageLightHelmetRoleCount = 4;

auto carthage_light_helmet_fill_role_colors(const HumanoidPalette &palette,
                                            QVector3D *out,
                                            std::size_t max) -> std::uint32_t;

auto carthage_light_helmet_make_static_attachment(
    const RenderArchetype &sub_archetype, std::uint16_t socket_bone_index,
    std::uint8_t base_role_byte, const QMatrix4x4 &bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec;

class CarthageLightHelmetRenderer : public IEquipmentRenderer {
public:
  CarthageLightHelmetRenderer() = default;

  static void submit(const CarthageLightHelmetConfig &config,
                     const DrawContext &ctx, const BodyFrames &frames,
                     const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  [[nodiscard]] auto
  base_config() const noexcept -> const CarthageLightHelmetConfig & {
    return m_config;
  }

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

private:
  CarthageLightHelmetConfig m_config;
};

} // namespace Render::GL
