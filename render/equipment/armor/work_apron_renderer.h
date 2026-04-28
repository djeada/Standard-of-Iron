#pragma once

#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../../static_attachment_spec.h"
#include "../i_equipment_renderer.h"

#include <QVector3D>
#include <array>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

struct WorkApronConfig {
  QVector3D leather_color = QVector3D(0.48F, 0.35F, 0.22F);
  QVector3D strap_color = QVector3D(0.35F, 0.28F, 0.20F);
  float apron_length = 0.45F;
  float apron_width = 0.65F;
  bool include_straps = true;
  bool include_pockets = true;
};

inline constexpr std::uint32_t kWorkApronRoleCount = 9;

auto work_apron_fill_role_colors(const HumanoidPalette &palette, QVector3D *out,
                                 std::size_t max) -> std::uint32_t;

auto work_apron_make_static_attachments(std::uint16_t waist_socket_bone_index,
                                        std::uint16_t chest_socket_bone_index,
                                        std::uint8_t base_role_byte)
    -> std::array<Render::Creature::StaticAttachmentSpec, 3>;

class WorkApronRenderer : public IEquipmentRenderer {
public:
  explicit WorkApronRenderer(const WorkApronConfig &config = WorkApronConfig{});

  static void submit(const WorkApronConfig &config, const DrawContext &ctx,
                     const BodyFrames &frames, const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  [[nodiscard]] auto base_config() const noexcept -> const WorkApronConfig & {
    return m_config;
  }

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

private:
  WorkApronConfig m_config;

  static void render_apron_body(const WorkApronConfig &config,
                              const DrawContext &ctx,
                              const AttachmentFrame &torso,
                              const AttachmentFrame &waist,
                              EquipmentBatch &batch);

  static void render_straps(const WorkApronConfig &config,
                           const DrawContext &ctx, const AttachmentFrame &torso,
                           const BodyFrames &frames, EquipmentBatch &batch);

  static void render_pockets(const WorkApronConfig &config,
                            const DrawContext &ctx,
                            const AttachmentFrame &waist,
                            EquipmentBatch &batch);
};

} // namespace Render::GL
