#include <QVector2D>

#include "command_executor_common.h"
#include "rigged_cull_pipeline.h"

namespace Render::GL {

using namespace Render::GL::ColorIndex;
using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

void Backend::execute_rigged_commands(const PreparedBatch& prepared,
                                      CommandExecutionContext& context) {
  const auto& queue = context.queue;
  const Camera& cam = context.cam;
  const QMatrix4x4& view = context.view;
  const QMatrix4x4& projection = context.projection;
  const QMatrix4x4& view_proj = context.view_proj;
  const float banner_wind_strength = context.banner_wind_strength;
  bool& polygon_offset_enabled = context.polygon_offset_enabled;
  (void)cam;
  (void)view;
  (void)projection;
  (void)view_proj;
  (void)banner_wind_strength;
  (void)polygon_offset_enabled;

  const std::size_t i = prepared.start;
  const std::size_t batch_end = prepared.end();
  const auto& cmd = queue.get_sorted(i);
  switch (cmd.index()) {
  case RiggedCreatureCmdIndex: {
    ++m_last_playback_stats.rigged_prepared_batches;
    m_last_playback_stats.rigged_commands += prepared.count;
    if (polygon_offset_enabled) {
      glDisable(GL_POLYGON_OFFSET_FILL);
      polygon_offset_enabled = false;
    }
    glDepthMask(GL_TRUE);
    if (!m_rigged_character_pipeline ||
        !m_rigged_character_pipeline->is_initialized()) {
      break;
    }

    std::size_t rig_fallback_start = i;

    if (prepared.kind == PreparedBatchKind::RiggedCreatureInstanced &&
        m_rigged_cull_pipeline != nullptr && m_rigged_cull_pipeline->is_available() &&
        prepared.count >= BackendPipelines::RiggedCullPipeline::minimum_instances()) {
      thread_local std::vector<const RiggedCreatureCmd*> cull_refs;
      cull_refs.clear();
      cull_refs.reserve(prepared.count);
      for (std::size_t k = i; k < batch_end; ++k) {
        cull_refs.push_back(&std::get<RiggedCreatureCmdIndex>(queue.get_sorted(k)));
      }
      QVector2D const viewport(static_cast<float>(m_viewport_width),
                               static_cast<float>(m_viewport_height));
      if (viewport.x() > 0.0F && viewport.y() > 0.0F &&
          m_rigged_cull_pipeline->draw(cull_refs.data(),
                                       cull_refs.size(),
                                       view_proj,
                                       cam.get_position(),
                                       viewport)) {
        m_rigged_drawn_this_frame += cull_refs.size();
        ++m_last_playback_stats.rigged_instanced_draws;
        m_last_playback_stats.rigged_instanced_instances += cull_refs.size();
        m_last_bound_shader = m_rigged_cull_pipeline->shader();
        m_last_bound_texture = nullptr;
        break;
      }
    }

    if (prepared.kind == PreparedBatchKind::RiggedCreatureInstanced &&
        m_rigged_character_pipeline->instanced_shader() != nullptr) {
      thread_local std::vector<const RiggedCreatureCmd*> rig_batch_refs;
      const std::size_t cap = std::max<std::size_t>(
          1U, m_rigged_character_pipeline->max_instances_per_batch());
      if (rig_batch_refs.capacity() < cap) {
        rig_batch_refs.reserve(cap);
      }
      std::size_t j = i;
      while (j < batch_end) {
        const std::size_t chunk_end = std::min(batch_end, j + cap);
        const std::size_t chunk_count = chunk_end - j;
        if (chunk_count < 2U) {
          const auto& single = std::get<RiggedCreatureCmdIndex>(queue.get_sorted(j));
          m_rigged_character_pipeline->draw(single, view_proj, cam.get_position());
          ++m_rigged_drawn_this_frame;
          ++m_last_playback_stats.rigged_single_draws;
          m_last_bound_shader = m_rigged_character_pipeline->shader();
          m_last_bound_texture = single.texture;
          ++j;
          rig_fallback_start = j;
          continue;
        }

        rig_batch_refs.clear();
        for (std::size_t k = j; k < chunk_end; ++k) {
          rig_batch_refs.push_back(
              &std::get<RiggedCreatureCmdIndex>(queue.get_sorted(k)));
        }
        if (m_rigged_character_pipeline->draw_instanced(rig_batch_refs.data(),
                                                        rig_batch_refs.size(),
                                                        view_proj,
                                                        cam.get_position())) {
          m_rigged_drawn_this_frame += rig_batch_refs.size();
          ++m_last_playback_stats.rigged_instanced_draws;
          m_last_playback_stats.rigged_instanced_instances += rig_batch_refs.size();
          m_last_bound_shader = m_rigged_character_pipeline->instanced_shader();
          m_last_bound_texture = nullptr;
          j = chunk_end;
          rig_fallback_start = j;
          continue;
        }

        break;
      }
    }

    if (rig_fallback_start < batch_end) {
      for (std::size_t j = rig_fallback_start; j < batch_end; ++j) {
        const auto& single = std::get<RiggedCreatureCmdIndex>(queue.get_sorted(j));
        m_rigged_character_pipeline->draw(single, view_proj, cam.get_position());
        ++m_rigged_drawn_this_frame;
        ++m_last_playback_stats.rigged_single_draws;
        m_last_bound_shader = m_rigged_character_pipeline->shader();
        m_last_bound_texture = single.texture;
      }
    }
    break;
  }
  default:
    break;
  }
}

} // namespace Render::GL
