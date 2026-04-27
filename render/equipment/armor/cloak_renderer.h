#pragma once

#include "../../gl/mesh.h"
#include "../../humanoid/humanoid_renderer_base.h"
#include "../../palette.h"
#include "../../static_attachment_spec.h"
#include "../i_equipment_renderer.h"
#include <QVector3D>
#include <cstddef>
#include <cstdint>

namespace Render::GL {

struct CloakConfig {
  QVector3D primary_color{0.14F, 0.38F, 0.54F};
  QVector3D trim_color{0.75F, 0.66F, 0.42F};
  float length_scale = 1.0F;
  float width_scale = 1.0F;
  bool show_clasp = true;
  int back_material_id = 5;
  int shoulder_material_id = 6;
};

struct CloakMeshes {
  Mesh *back = nullptr;
  Mesh *shoulder = nullptr;
};

inline constexpr std::uint32_t kCloakRoleCount = 2;

auto cloak_fill_role_colors_with_primary(const QVector3D &primary_color,
                                         const HumanoidPalette &palette,
                                         QVector3D *out,
                                         std::size_t max) -> std::uint32_t;

auto cloak_make_static_attachment(
    const CloakConfig &config, const CloakMeshes &meshes,
    std::uint16_t torso_socket_bone_index,
    std::uint8_t base_role_byte) -> Render::Creature::StaticAttachmentSpec;

class CloakRenderer : public IEquipmentRenderer {
public:
  explicit CloakRenderer(const CloakConfig &config = CloakConfig{});

  static void submit(const CloakConfig &config, const CloakMeshes &meshes,
                     const DrawContext &ctx, const BodyFrames &frames,
                     const HumanoidPalette &palette,
                     const HumanoidAnimationContext &anim,
                     EquipmentBatch &batch);

  [[nodiscard]] auto base_config() const noexcept -> const CloakConfig & {
    return m_config;
  }

  [[nodiscard]] auto meshes() const noexcept -> CloakMeshes;

  void set_config(const CloakConfig &config);

  void render(const DrawContext &ctx, const BodyFrames &frames,
              const HumanoidPalette &palette,
              const HumanoidAnimationContext &anim,
              EquipmentBatch &batch) override;

private:
  CloakConfig m_config;
};

} // namespace Render::GL
