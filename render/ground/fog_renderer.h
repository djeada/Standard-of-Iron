#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <memory>
#include <vector>

#include "render/draw_commands.h"
#include "render/gl/buffer.h"
#include "render/gl/texture.h"
#include "render/i_render_pass.h"
#include "visibility_mask_encoder.h"

namespace Render::GL {
class Renderer;
class ResourceManager;

class FogRenderer : public IRenderPass {
public:
  FogRenderer() = default;
  ~FogRenderer() override = default;

  void set_enabled(bool enabled) { m_enabled = enabled; }
  [[nodiscard]] auto is_enabled() const -> bool { return m_enabled; }
  void set_soft_reveal_enabled(bool enabled);
  [[nodiscard]] auto soft_reveal_enabled() const -> bool {
    return m_soft_reveal_enabled;
  }

  void update_mask(int width,
                   int height,
                   float tile_size,
                   const std::vector<std::uint8_t>& cells);

  void submit(Renderer& renderer, ResourceManager* resources) override;

  auto prepare_mask(Renderer& renderer) -> FogMaskResources;

  void advance_reveal(float dt_seconds);

  [[nodiscard]] auto patch_count() const -> std::size_t { return m_instances.size(); }
  [[nodiscard]] auto fog_amount_at(int grid_x, int grid_z) const -> float;
  [[nodiscard]] auto is_settled() const -> bool { return m_settled; }

private:
  void rebuild_patches();
  void upload_mask(Renderer& renderer);
  void upload_instances();
  void clear_state();

  using FogInstance = FogInstanceData;

  bool m_enabled = true;
  bool m_soft_reveal_enabled = false;
  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;

  std::vector<float> m_fog_amount;
  std::vector<float> m_target_fog;

  std::vector<float> m_seen_amount;
  bool m_settled = true;
  bool m_patches_dirty = false;

  Ground::MaskRegion m_mask_dirty;
  Ground::MaskRegion m_fade_region;
  float m_last_time = -1.0F;

  std::vector<FogInstance> m_instances;
  std::unique_ptr<Buffer> m_instance_buffer;
  bool m_instances_dirty = false;

  std::unique_ptr<Texture> m_mask_texture;
  std::vector<unsigned char> m_mask_texels;
  int m_mask_texture_width = 0;
  int m_mask_texture_height = 0;
};

} // namespace Render::GL
