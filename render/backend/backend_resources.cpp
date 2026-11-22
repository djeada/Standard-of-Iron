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

void Backend::sync_frame_resources(const DrawQueue& queue) {
  auto& instances = m_context.instances();
  auto& textures = m_context.textures();
  const auto sync_height =
      [&textures](const TerrainSurfaceCmd::HeightResources& height) {
        (void)textures.resolve(height.texture);
        (void)textures.resolve(height.field_texture);
        (void)textures.resolve(height.cover_texture);
      };
  for (const DrawCmd& cmd : queue.items()) {
    switch (cmd.index()) {
    case TerrainScatterCmdIndex: {
      const auto& scatter = std::get<TerrainScatterCmdIndex>(cmd);
      (void)instances.resolve(scatter.instances);
      (void)textures.resolve(scatter.visibility.texture);
      break;
    }
    case FogBatchCmdIndex: {
      const auto& fog = std::get<FogBatchCmdIndex>(cmd);
      (void)instances.resolve(fog.instance_block);
      (void)textures.resolve(fog.mask.texture);
      break;
    }
    case TerrainSurfaceCmdIndex: {
      const auto& surface = std::get<TerrainSurfaceCmdIndex>(cmd);
      sync_height(surface.height);
      if (m_terrain_pipeline != nullptr) {
        m_terrain_pipeline->sync_noise_atlas(surface.height.noise);
      }
      (void)textures.resolve(surface.visibility.texture);
      break;
    }
    case TerrainFeatureCmdIndex: {
      const auto& feature = std::get<TerrainFeatureCmdIndex>(cmd);
      (void)textures.resolve(feature.visibility.texture);
      (void)textures.resolve(feature.fog_mask.texture);
      break;
    }
    case RiggedCreatureCmdIndex: {
      const auto& rigged = std::get<RiggedCreatureCmdIndex>(cmd);
      (void)instances.resolve(rigged.palette_block);
      if (rigged.mesh != nullptr) {
        (void)m_context.rigged_meshes().ensure(*rigged.mesh);
      }
      if (rigged.shadow_mesh != nullptr) {
        (void)m_context.rigged_meshes().ensure(*rigged.shadow_mesh);
      }
      break;
    }
    case GroundMarkerCmdIndex:
      sync_height(std::get<GroundMarkerCmdIndex>(cmd).height);
      break;
    default:
      break;
    }
  }
  // Uploads bind buffers and texture unit 0 behind the state cache's back.
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
  glBindVertexArray(0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);
  m_gl_state.forget_bindings();
}

void Backend::refresh_draw_resources() {
  DrawResources::Parts parts;
  if (const auto* stock = m_context.resources(); stock != nullptr) {
    parts.quad = stock->quad();
    parts.ground = stock->ground();
    parts.unit = stock->unit();
    parts.white = stock->white();
    parts.wear_volume = stock->wear_volume();
  }
  parts.banner_mesh = banner_mesh();
  parts.banner_shader = banner_shader();
  parts.troop_shadow_shader = m_shadow_shader;
  m_draw_resources = DrawResources(parts);
}

auto Backend::banner_mesh() const -> Mesh* {
  if (m_banner_pipeline != nullptr) {
    return m_banner_pipeline->get_banner_mesh();
  }
  return nullptr;
}

auto Backend::banner_shader() const -> Shader* {
  if (m_banner_pipeline != nullptr) {
    return m_banner_pipeline->shader();
  }
  return nullptr;
}

} // namespace Render::GL
