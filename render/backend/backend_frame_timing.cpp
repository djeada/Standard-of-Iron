#include <QDebug>
#include <QOpenGLContext>
#include <qglobal.h>
#include <qmatrix4x4.h>
#include <qopenglcontext.h>
#include <qstringliteral.h>
#include <qvectornd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

#include "render/backend/backend.h"
#include "render/backend/banner_pipeline.h"
#include "render/backend/combat_dust_pipeline.h"
#include "render/backend/cylinder_pipeline.h"
#include "render/backend/directional_shadow_block.h"
#include "render/backend/effects_pipeline.h"
#include "render/backend/fog_range.h"
#include "render/backend/ground_marker_pipeline.h"
#include "render/backend/healer_aura_pipeline.h"
#include "render/backend/healing_beam_pipeline.h"
#include "render/backend/mesh_instancing_pipeline.h"
#include "render/backend/mode_indicator_pipeline.h"
#include "render/backend/post_process_pipeline.h"
#include "render/backend/primitive_batch_pipeline.h"
#include "render/backend/rain_pipeline.h"
#include "render/backend/rigged_character_pipeline.h"
#include "render/backend/rigged_cull_pipeline.h"
#include "render/backend/shader_uniform_cache.h"
#include "render/backend/shadow_cascade_fit.h"
#include "render/backend/sky_box_pipeline.h"
#include "render/backend/state_scopes.h"
#include "render/backend/terrain_pipeline.h"
#include "render/backend/vegetation_pipeline.h"
#include "render/backend/water_pipeline.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/draw_queue.h"
#include "render/frame/graphics_settings.h"
#include "render/frame/local_lighting.h"
#include "render/frame/material.h"
#include "render/frame/primitive_batch.h"
#include "render/frame/rain_gpu.h"
#include "render/geom/mesh.h"
#include "render/geom/mode_indicator.h"
#include "render/geom/rigged_mesh.h"
#include "render/geom/selection_disc.h"
#include "render/geom/shared_geometry_cache.h"
#include "render/gl/buffer.h"
#include "render/gl/draw_tally.h"
#include "render/gl/gl_lifetime.h"
#include "render/gl/gl_resource_tracking.h"
#include "render/gl/platform_gl.h"
#include "render/gl/render_constants.h"
#include "render/gl/resources.h"
#include "render/gl/shader.h"
#include "render/gl/texture.h"
#include "render/gl/ubo_bindings.h"
#include "render/profiling/frame_profile.h"
#include "scene/camera.h"

namespace Render::GL {

using namespace Render::GL::ColorIndex;
using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

auto Backend::try_collect_frame_slot(FrameGpuTiming& slot) -> bool {
  if (!slot.pending) {
    return true;
  }
  auto const wait_start = std::chrono::steady_clock::now();
  const GLenum polled = glClientWaitSync(
      static_cast<GLsync>(slot.fence.get()), GL_SYNC_FLUSH_COMMANDS_BIT, 0);
  m_last_playback_stats.gpu_wait_ms = std::chrono::duration<double, std::milli>(
                                          std::chrono::steady_clock::now() - wait_start)
                                          .count();
  if (polled == GL_TIMEOUT_EXPIRED) {
    ++m_last_playback_stats.gpu_samples_deferred;
    return false;
  }
  if (polled == GL_WAIT_FAILED) {
    qWarning() << "Backend: glClientWaitSync failed while collecting GPU timing; "
                  "dropping this frame's queries";
    slot.fence.reset();
    slot.pending = false;
    slot.mark_count = 0;
    m_last_playback_stats.gpu_shadow_ms = 0.0;
    m_last_playback_stats.gpu_color_ms = 0.0;
    return true;
  }
  collect_frame_slot(slot);
  return true;
}

void Backend::wait_for_frame_slot(FrameGpuTiming& slot) {
  if (!slot.pending) {
    return;
  }
  auto const wait_start = std::chrono::steady_clock::now();
  constexpr GLuint64 k_wait_timeout_ns = 1'000'000'000ULL;
  constexpr int k_max_wait_attempts = 4;
  bool signaled = false;
  for (int attempt = 0; attempt < k_max_wait_attempts && !signaled; ++attempt) {
    const GLenum waited = glClientWaitSync(static_cast<GLsync>(slot.fence.get()),
                                           GL_SYNC_FLUSH_COMMANDS_BIT,
                                           k_wait_timeout_ns);
    if (waited == GL_ALREADY_SIGNALED || waited == GL_CONDITION_SATISFIED) {
      signaled = true;
    } else if (waited == GL_WAIT_FAILED) {
      qWarning() << "Backend: glClientWaitSync failed while collecting GPU timing; "
                    "dropping this frame's queries";
      break;
    }
  }
  m_last_playback_stats.gpu_wait_ms = std::chrono::duration<double, std::milli>(
                                          std::chrono::steady_clock::now() - wait_start)
                                          .count();
  if (!signaled) {
    qWarning() << "Backend: GPU timing fence never signaled after"
               << k_max_wait_attempts << "waits; discarding the frame's timing slot";
    slot.fence.reset();
    slot.pending = false;
    slot.mark_count = 0;
    m_last_playback_stats.gpu_shadow_ms = 0.0;
    m_last_playback_stats.gpu_color_ms = 0.0;
    return;
  }
  collect_frame_slot(slot);
}

void Backend::collect_frame_slot(FrameGpuTiming& slot) {
  slot.fence.reset();
  m_last_playback_stats.gpu_sample_frame = slot.frame_index;
  m_last_playback_stats.gpu_sample_age_frames =
      m_frame_timing_index > slot.frame_index ? m_frame_timing_index - slot.frame_index
                                              : 0;
  std::array<GLuint64, 4> ticks{};
  for (std::size_t i = 0; i < ticks.size(); ++i) {
    if (slot.timestamps[i]) {
      glGetQueryObjectui64v(slot.timestamps[i].id(), GL_QUERY_RESULT, &ticks[i]);
    }
  }
  m_last_playback_stats.gpu_shadow_ms =
      static_cast<double>(ticks[1] - ticks[0]) / 1.0e6;
  m_last_playback_stats.gpu_color_ms = static_cast<double>(ticks[2] - ticks[1]) / 1.0e6;
  if (ticks[3] >= ticks[2]) {
    m_gpu_breakdown_post_ms += static_cast<double>(ticks[3] - ticks[2]) / 1.0e6;
  }

  if (m_gpu_breakdown_enabled && slot.mark_count > 0) {
    GLuint64 previous = ticks[1];
    for (std::size_t i = 0; i < slot.mark_count; ++i) {
      GLuint64 tick = 0;
      glGetQueryObjectui64v(slot.marks[i].id(), GL_QUERY_RESULT, &tick);
      const std::uint8_t type = slot.mark_types[i];
      if (type < m_gpu_breakdown_ms.size() && tick >= previous) {
        m_gpu_breakdown_ms[type] += static_cast<double>(tick - previous) / 1.0e6;
      }
      previous = tick;
    }
    ++m_gpu_breakdown_frames;
    if (m_gpu_breakdown_frames >= 120) {
      QString report = QStringLiteral("SOI_GPU_BREAKDOWN per frame:");
      for (std::size_t type = 0; type < m_gpu_breakdown_ms.size(); ++type) {
        if (m_gpu_breakdown_ms[type] <= 0.0) {
          continue;
        }
        report += QStringLiteral(" %1=%2ms")
                      .arg(type)
                      .arg(m_gpu_breakdown_ms[type] /
                               static_cast<double>(m_gpu_breakdown_frames),
                           0,
                           'f',
                           3);
        m_gpu_breakdown_ms[type] = 0.0;
      }
      report += QStringLiteral(" post=%1ms")
                    .arg(m_gpu_breakdown_post_ms /
                             static_cast<double>(m_gpu_breakdown_frames),
                         0,
                         'f',
                         3);
      m_gpu_breakdown_post_ms = 0.0;
      qInfo().noquote() << report;
      m_gpu_breakdown_frames = 0;
    }
  }
  slot.mark_count = 0;
  slot.pending = false;
}

void Backend::gpu_breakdown_mark(FrameGpuTiming& slot, std::uint8_t type) {
  if (!m_gpu_breakdown_enabled || slot.mark_count >= k_gpu_breakdown_marks) {
    return;
  }
  QueryHandle& mark = slot.marks[slot.mark_count];
  if (!mark) {
    mark = QueryHandle::create();
  }
  slot.mark_types[slot.mark_count] = type;
  glQueryCounter(mark.id(), GL_TIMESTAMP);
  ++slot.mark_count;
}

} // namespace Render::GL
