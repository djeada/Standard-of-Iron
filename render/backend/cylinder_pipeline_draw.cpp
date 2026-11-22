#include "render/backend/cylinder_pipeline.h"
#include "render/backend/pipeline_draw_context.h"
#include "render/backend/state_scopes.h"
#include "render/backend/uniform_helpers.h"
#include "render/frame/draw_commands.h"
#include "render/gl/render_constants.h"
#include "render/gl/shader.h"
#include "render/gl/texture.h"

namespace Render::GL::BackendPipelines {

using namespace Render::GL::ColorIndex;

void CylinderPipeline::draw(const PreparedBatch& prepared,
                            PipelineDrawContext& context) {
  const auto& queue = context.queue;
  const QMatrix4x4& view_proj = context.frame.view_proj;
  bool& polygon_offset_enabled = context.state.polygon_offset_enabled;

  const std::size_t i = prepared.start;
  const std::size_t batch_end = prepared.end();
  const auto& cmd = queue.get_sorted(i);
  switch (cmd.index()) {
  case CylinderCmdIndex: {
    m_cylinder_scratch.clear();
    for (std::size_t j = i; j < batch_end; ++j) {
      const auto& cy = std::get<CylinderCmdIndex>(queue.get_sorted(j));
      CylinderInstanceGpu gpu{};
      gpu.start = cy.start;
      gpu.end = cy.end;
      gpu.radius = cy.radius;
      gpu.alpha = cy.alpha;
      gpu.color = cy.color;
      m_cylinder_scratch.emplace_back(gpu);
    }

    const std::size_t instance_count = m_cylinder_scratch.size();
    if (instance_count > 0 && (m_cylinder_shader != nullptr)) {
      glDepthMask(GL_TRUE);
      if (polygon_offset_enabled) {
        glDisable(GL_POLYGON_OFFSET_FILL);
        polygon_offset_enabled = false;
      }
      Shader* cylinder_shader = m_cylinder_shader;
      if (context.state.bound_shader != cylinder_shader) {
        cylinder_shader->use();
        context.state.bound_shader = cylinder_shader;
        context.state.bound_texture = nullptr;
      }
      set_uniform_if_valid(*cylinder_shader, m_cylinder_uniforms.view_proj, view_proj);
      upload_cylinder_instances(instance_count);
      draw_cylinders(instance_count);
    }
    break;
  }
  case FogBatchCmdIndex: {
    const auto& batch = std::get<FogBatchCmdIndex>(cmd);
    const FogInstanceData* instances = batch.instances;
    const std::size_t instance_count = batch.count;
    if (((instances != nullptr) || (batch.instance_buffer != nullptr)) &&
        instance_count > 0 && (m_fog_shader != nullptr)) {
      DepthMaskScope const depth_mask(false);
      BlendScope const blend(true);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      if (polygon_offset_enabled) {
        glDisable(GL_POLYGON_OFFSET_FILL);
        polygon_offset_enabled = false;
      }
      Shader* fog_shader = m_fog_shader;
      if (context.state.bound_shader != fog_shader) {
        fog_shader->use();
        context.state.bound_shader = fog_shader;
        context.state.bound_texture = nullptr;
      }
      if (m_fog_uniforms.view_proj != Shader::InvalidUniform) {
        fog_shader->set_uniform(m_fog_uniforms.view_proj, view_proj);
      }
      if (m_fog_uniforms.time != Shader::InvalidUniform) {
        fog_shader->set_uniform(m_fog_uniforms.time, context.frame.animation_time);
      }
      const auto& mask = batch.mask;
      const bool has_mask = mask.enabled && (mask.texture != nullptr);
      if (m_fog_uniforms.has_mask != Shader::InvalidUniform) {
        fog_shader->set_uniform(m_fog_uniforms.has_mask, has_mask ? 1 : 0);
      }
      if (has_mask) {
        if (m_fog_uniforms.mask_size != Shader::InvalidUniform) {
          fog_shader->set_uniform(m_fog_uniforms.mask_size, mask.size);
        }
        if (m_fog_uniforms.mask_tile_size != Shader::InvalidUniform) {
          fog_shader->set_uniform(m_fog_uniforms.mask_tile_size, mask.tile_size);
        }
        mask.texture->bind(TextureUnit::terrain_visibility);
        context.state.bound_texture = mask.texture;
        if (m_fog_uniforms.mask_tex != Shader::InvalidUniform) {
          fog_shader->set_uniform(m_fog_uniforms.mask_tex,
                                  TextureUnit::terrain_visibility);
        }
      }
      if (batch.instance_buffer != nullptr) {
        bind_fog_instance_buffer(batch.instance_buffer);
      } else {
        m_fog_scratch.resize(instance_count);
        for (std::size_t idx = 0; idx < instance_count; ++idx) {
          FogInstanceGpu gpu{};
          gpu.center = instances[idx].center;
          gpu.size = instances[idx].size;
          gpu.color = instances[idx].color;
          gpu.alpha = instances[idx].alpha;
          m_fog_scratch[idx] = gpu;
        }
        upload_fog_instances(instance_count);
      }
      draw_fog(instance_count);
    }
    break;
  }
  default:
    break;
  }
}

} // namespace Render::GL::BackendPipelines
