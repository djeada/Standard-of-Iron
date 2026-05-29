#include "command_executor_common.h"

namespace Render::GL {

using namespace Render::GL::ColorIndex;
using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

void Backend::execute_cylinder_commands(const PreparedBatch& prepared,
                                        CommandExecutionContext& context) {
  const auto& queue = context.queue;
  const Camera& cam = context.cam;
  const QMatrix4x4& view = context.view;
  const QMatrix4x4& projection = context.projection;
  const QMatrix4x4& view_proj = context.view_proj;
  const float banner_wind_strength = context.banner_wind_strength;
  bool& polygon_offset_enabled = context.polygon_offset_enabled;
  const bool rigged_instancing_enabled = context.rigged_instancing_enabled;
  std::size_t const& debug_rigged_batches = context.debug_rigged_batches;
  std::size_t const& debug_rigged_cmds = context.debug_rigged_cmds;
  std::size_t const& debug_rigged_instanced_attempts =
      context.debug_rigged_instanced_attempts;
  std::size_t const& debug_rigged_instanced_successes =
      context.debug_rigged_instanced_successes;
  std::size_t const& debug_rigged_instanced_failures =
      context.debug_rigged_instanced_failures;
  std::size_t const& debug_rigged_single_draws = context.debug_rigged_single_draws;
  (void)cam;
  (void)view;
  (void)projection;
  (void)view_proj;
  (void)banner_wind_strength;
  (void)polygon_offset_enabled;
  (void)rigged_instancing_enabled;
  (void)debug_rigged_batches;
  (void)debug_rigged_cmds;
  (void)debug_rigged_instanced_attempts;
  (void)debug_rigged_instanced_successes;
  (void)debug_rigged_instanced_failures;
  (void)debug_rigged_single_draws;

  const std::size_t i = prepared.start;
  const std::size_t batch_end = prepared.end();
  const auto& cmd = queue.get_sorted(i);
  switch (cmd.index()) {
  case CylinderCmdIndex: {
    if (!m_cylinder_pipeline) {
      break;
    }
    m_cylinder_pipeline->m_cylinder_scratch.clear();
    for (std::size_t j = i; j < batch_end; ++j) {
      const auto& cy = std::get<CylinderCmdIndex>(queue.get_sorted(j));
      BackendPipelines::CylinderPipeline::CylinderInstanceGpu gpu{};
      gpu.start = cy.start;
      gpu.end = cy.end;
      gpu.radius = cy.radius;
      gpu.alpha = cy.alpha;
      gpu.color = cy.color;
      m_cylinder_pipeline->m_cylinder_scratch.emplace_back(gpu);
    }

    const std::size_t instance_count = m_cylinder_pipeline->m_cylinder_scratch.size();
    if (instance_count > 0 && (m_cylinder_pipeline->cylinder_shader() != nullptr)) {
      glDepthMask(GL_TRUE);
      if (polygon_offset_enabled) {
        glDisable(GL_POLYGON_OFFSET_FILL);
        polygon_offset_enabled = false;
      }
      Shader* cylinder_shader = m_cylinder_pipeline->cylinder_shader();
      if (m_last_bound_shader != cylinder_shader) {
        cylinder_shader->use();
        m_last_bound_shader = cylinder_shader;
        m_last_bound_texture = nullptr;
      }
      set_uniform_if_valid(*cylinder_shader,
                           m_cylinder_pipeline->m_cylinder_uniforms.view_proj,
                           view_proj);
      m_cylinder_pipeline->upload_cylinder_instances(instance_count);
      m_cylinder_pipeline->draw_cylinders(instance_count);
    }
    break;
  }
  case FogBatchCmdIndex: {
    if (!m_cylinder_pipeline) {
      break;
    }
    const auto& batch = std::get<FogBatchCmdIndex>(cmd);
    const FogInstanceData* instances = batch.instances;
    const std::size_t instance_count = batch.count;
    if (((instances != nullptr) || (batch.instance_buffer != nullptr)) &&
        instance_count > 0 && (m_cylinder_pipeline->fog_shader() != nullptr)) {
      DepthMaskScope const depth_mask(false);
      BlendScope const blend(true);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      if (polygon_offset_enabled) {
        glDisable(GL_POLYGON_OFFSET_FILL);
        polygon_offset_enabled = false;
      }
      Shader* fog_shader = m_cylinder_pipeline->fog_shader();
      if (m_last_bound_shader != fog_shader) {
        fog_shader->use();
        m_last_bound_shader = fog_shader;
        m_last_bound_texture = nullptr;
      }
      if (m_cylinder_pipeline->m_fog_uniforms.view_proj != Shader::InvalidUniform) {
        fog_shader->set_uniform(m_cylinder_pipeline->m_fog_uniforms.view_proj,
                                view_proj);
      }
      if (m_cylinder_pipeline->m_fog_uniforms.time != Shader::InvalidUniform) {
        fog_shader->set_uniform(m_cylinder_pipeline->m_fog_uniforms.time,
                                m_animation_time);
      }
      if (batch.instance_buffer != nullptr) {
        m_cylinder_pipeline->bind_fog_instance_buffer(batch.instance_buffer);
      } else {
        m_cylinder_pipeline->m_fog_scratch.resize(instance_count);
        for (std::size_t idx = 0; idx < instance_count; ++idx) {
          BackendPipelines::CylinderPipeline::FogInstanceGpu gpu{};
          gpu.center = instances[idx].center;
          gpu.size = instances[idx].size;
          gpu.color = instances[idx].color;
          gpu.alpha = instances[idx].alpha;
          m_cylinder_pipeline->m_fog_scratch[idx] = gpu;
        }
        m_cylinder_pipeline->upload_fog_instances(instance_count);
      }
      m_cylinder_pipeline->draw_fog(instance_count);
    }
    break;
  }
  default:
    break;
  }
}

} // namespace Render::GL
