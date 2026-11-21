

#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "render/cache/static_attachment_spec.h"
#include "render/creature/humanoid/runtime/humanoid_renderer.h"
#include "render/creature/humanoid/runtime/sword_pose_utils.h"
#include "render/equipment/i_equipment_renderer.h"
#include "render/frame/palette.h"

namespace Render::GL {

class SwordRenderer : public IEquipmentRenderer {
public:
  explicit SwordRenderer(SwordRenderConfig config = {});

  static void submit(const SwordRenderConfig& config,
                     const DrawContext& ctx,
                     const BodyFrames& frames,
                     const HumanoidPalette& palette,
                     const HumanoidAnimationContext& anim,
                     EquipmentBatch& batch);

  [[nodiscard]] auto base_config() const noexcept -> const SwordRenderConfig& {
    return m_base;
  }

  void render(const DrawContext& ctx,
              const BodyFrames& frames,
              const HumanoidPalette& palette,
              const HumanoidAnimationContext& anim,
              EquipmentBatch& batch) override;

private:
  SwordRenderConfig m_base;
};

inline constexpr std::uint32_t k_sword_role_count = 4U;
inline constexpr std::uint32_t k_scabbard_role_count = 2U;

auto sword_fill_role_colors(const HumanoidPalette& palette,
                            const SwordRenderConfig& config,
                            QVector3D* out,
                            std::size_t max) -> std::uint32_t;

auto sword_make_static_attachment(const SwordRenderConfig& config,
                                  std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec;

auto sword_make_static_attachment(const SwordRenderConfig& config,
                                  std::uint8_t base_role_byte,
                                  const QVector3D& blade_dir_local)
    -> Render::Creature::StaticAttachmentSpec;

auto scabbard_fill_role_colors(const HumanoidPalette& palette,
                               QVector3D* out,
                               std::size_t max) -> std::uint32_t;

auto scabbard_make_static_attachment(float sheath_r,
                                     std::uint16_t socket_bone_index,
                                     std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec;

auto held_scroll_make_static_attachment(std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec;
auto held_wax_tablet_make_static_attachment(std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec;
auto manacles_make_static_attachments(std::uint8_t base_role_byte)
    -> std::vector<Render::Creature::StaticAttachmentSpec>;
auto held_prop_fill_role_colors(bool tablet,
                                QVector3D* out,
                                std::size_t max) -> std::uint32_t;
auto manacles_fill_role_colors(QVector3D* out, std::size_t max) -> std::uint32_t;

} // namespace Render::GL
