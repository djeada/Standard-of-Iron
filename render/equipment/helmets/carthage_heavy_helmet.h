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

struct CarthageHeavyHelmetConfig {
  QVector3D bronze_color = QVector3D(0.72F, 0.45F, 0.20F);
  QVector3D crest_color = QVector3D(0.95F, 0.95F, 0.90F);
  QVector3D glow_color = QVector3D(1.0F, 0.98F, 0.92F);
  bool has_cheek_guards = true;
  bool has_face_plate = true;
  bool has_neck_guard = true;
  bool has_hair_crest = true;
  int detail_level = 2;
};

auto carthage_heavy_helmet_shell_archetype() -> const RenderArchetype &;
auto carthage_heavy_helmet_neck_guard_archetype() -> const RenderArchetype &;
auto carthage_heavy_helmet_cheek_guards_archetype() -> const RenderArchetype &;
auto carthage_heavy_helmet_face_plate_archetype() -> const RenderArchetype &;
auto carthage_heavy_helmet_crest_archetype() -> const RenderArchetype &;
auto carthage_heavy_helmet_rivets_archetype() -> const RenderArchetype &;

inline constexpr std::uint32_t kCarthageHeavyHelmetRoleCount = 4;

auto carthage_heavy_helmet_fill_role_colors(const HumanoidPalette &palette,
                                            QVector3D *out,
                                            std::size_t max) -> std::uint32_t;

auto carthage_heavy_helmet_make_static_attachment(
    const RenderArchetype &sub_archetype, std::uint16_t socket_bone_index,
    std::uint8_t base_role_byte, const QMatrix4x4 &bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec;

class CarthageHeavyHelmetRenderer : public IEquipmentRenderer {
public:
  explicit CarthageHeavyHelmetRenderer(
      const CarthageHeavyHelmetConfig &cfg = {})
      : m_config(cfg) {}

  static void submit(const CarthageHeavyHelmetConfig &config,
                     const DrawContext &ctx, const BodyFrames &frames,
                     const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

  [[nodiscard]] auto base_config() const -> const CarthageHeavyHelmetConfig & {
    return m_config;
  }

private:
  CarthageHeavyHelmetConfig m_config;
};

} // namespace Render::GL
