#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <memory>
#include <vector>

#include "../draw_queue.h"
#include "../gl/buffer.h"
#include "../i_render_pass.h"

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

private:
  void build_chunks();
  void build_transition_chunks(const std::vector<std::uint8_t>& previous_cells,
                               const std::vector<std::uint8_t>& next_cells,
                               float now);
  void upload_instances();

  using FogInstance = FogInstanceData;

  struct FogTransition {
    FogInstance instance;
    float start_time = 0.0F;
    float duration = 0.28F;
  };

  bool m_enabled = true;
  bool m_soft_reveal_enabled = false;
  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;
  float m_half_width = 0.0F;
  float m_half_height = 0.0F;
  std::vector<std::uint8_t> m_cells;
  std::vector<FogInstance> m_instances;
  std::vector<FogTransition> m_transitions;
  std::vector<FogInstance> m_submit_instances;
  std::unique_ptr<Buffer> m_instance_buffer;
  bool m_instances_dirty = false;
};

} // namespace Render::GL
