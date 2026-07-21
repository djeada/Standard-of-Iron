#include "command_executor_common.h"

namespace Render::GL {

using namespace Render::GL::ColorIndex;
using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

void Backend::execute_scatter_commands(const PreparedBatch& prepared,
                                       CommandExecutionContext& context) {
  const auto& queue = context.queue;
  const Camera& cam = context.cam;
  const QMatrix4x4& view = context.view;
  const QMatrix4x4& projection = context.projection;
  const QMatrix4x4& view_proj = context.view_proj;
  const float banner_wind_strength = context.banner_wind_strength;
  bool const& polygon_offset_enabled = context.polygon_offset_enabled;
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
  case TerrainScatterCmdIndex: {
    const auto& deco_cmd_ = std::get<TerrainScatterCmdIndex>(cmd);
    switch (deco_cmd_.species) {
    case TerrainScatterCmd::Species::Grass: {
      struct GrassView {
        Buffer* instance_buffer;
        std::size_t instance_count;
        const GrassBatchParams& params;
      };
      const GrassView grass{
          deco_cmd_.instance_buffer, deco_cmd_.instance_count, deco_cmd_.grass};
      if ((grass.instance_buffer == nullptr) || grass.instance_count == 0 ||
          (m_terrain_pipeline->m_grass_shader == nullptr) ||
          (m_terrain_pipeline->m_grass_vao == 0U) ||
          m_terrain_pipeline->m_grass_vertex_count == 0) {
        break;
      }

      DepthMaskScope const depth_mask(false);
      BlendScope const blend(true);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
      if (prev_cull != 0U) {
        glDisable(GL_CULL_FACE);
      }

      if (m_last_bound_shader != m_terrain_pipeline->m_grass_shader) {
        m_terrain_pipeline->m_grass_shader->use();
        m_last_bound_shader = m_terrain_pipeline->m_grass_shader;
        m_last_bound_texture = nullptr;
      }

      if (m_terrain_pipeline->m_grass_uniforms.view_proj != Shader::InvalidUniform) {
        m_terrain_pipeline->m_grass_shader->set_uniform(
            m_terrain_pipeline->m_grass_uniforms.view_proj, view_proj);
      }
      if (m_terrain_pipeline->m_grass_uniforms.time != Shader::InvalidUniform) {
        m_terrain_pipeline->m_grass_shader->set_uniform(
            m_terrain_pipeline->m_grass_uniforms.time, grass.params.time);
      }
      if (m_terrain_pipeline->m_grass_uniforms.wind_strength !=
          Shader::InvalidUniform) {
        m_terrain_pipeline->m_grass_shader->set_uniform(
            m_terrain_pipeline->m_grass_uniforms.wind_strength,
            grass.params.wind_strength);
      }
      if (m_terrain_pipeline->m_grass_uniforms.wind_speed != Shader::InvalidUniform) {
        m_terrain_pipeline->m_grass_shader->set_uniform(
            m_terrain_pipeline->m_grass_uniforms.wind_speed, grass.params.wind_speed);
      }
      if (m_terrain_pipeline->m_grass_uniforms.soil_color != Shader::InvalidUniform) {
        m_terrain_pipeline->m_grass_shader->set_uniform(
            m_terrain_pipeline->m_grass_uniforms.soil_color, grass.params.soil_color);
      }
      if (m_terrain_pipeline->m_grass_uniforms.light_dir != Shader::InvalidUniform) {
        QVector3D light_dir = grass.params.light_direction;
        if (!light_dir.isNull()) {
          light_dir.normalize();
        }
        m_terrain_pipeline->m_grass_shader->set_uniform(
            m_terrain_pipeline->m_grass_uniforms.light_dir, light_dir);
      }

      glBindVertexArray(m_terrain_pipeline->m_grass_vao);
      grass.instance_buffer->bind();
      const auto stride = static_cast<GLsizei>(sizeof(GrassInstanceGpu));
      apply_vertex_attrib_layout({{tex_coord,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride,
                                   offsetof(GrassInstanceGpu, pos_height)},
                                  {instance_position,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride,
                                   offsetof(GrassInstanceGpu, color_width)},
                                  {instance_scale,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride,
                                   offsetof(GrassInstanceGpu, sway_params)}});
      grass.instance_buffer->unbind();

      glDrawArraysInstanced(GL_TRIANGLES,
                            0,
                            m_terrain_pipeline->m_grass_vertex_count,
                            static_cast<GLsizei>(grass.instance_count));
      glBindVertexArray(0);

      if (prev_cull != 0U) {
        glEnable(GL_CULL_FACE);
      }

      break;
    }
    case TerrainScatterCmd::Species::Stone: {
      if (!m_vegetation_pipeline) {
        break;
      }
      struct StoneView {
        Buffer* instance_buffer;
        std::size_t instance_count;
        const StoneBatchParams& params;
      };
      const StoneView stone{
          deco_cmd_.instance_buffer, deco_cmd_.instance_count, deco_cmd_.stone};
      if ((stone.instance_buffer == nullptr) || stone.instance_count == 0 ||
          (m_vegetation_pipeline->stone_shader() == nullptr) ||
          (m_vegetation_pipeline->m_stone_vao == 0U) ||
          m_vegetation_pipeline->m_stone_index_count == 0) {
        break;
      }

      DepthMaskScope const depth_mask(true);
      BlendScope const blend(false);

      Shader* stone_shader = m_vegetation_pipeline->stone_shader();
      if (m_last_bound_shader != stone_shader) {
        stone_shader->use();
        m_last_bound_shader = stone_shader;
        m_last_bound_texture = nullptr;
      }

      if (m_vegetation_pipeline->m_stone_uniforms.view_proj != Shader::InvalidUniform) {
        stone_shader->set_uniform(m_vegetation_pipeline->m_stone_uniforms.view_proj,
                                  view_proj);
      }
      if (m_vegetation_pipeline->m_stone_uniforms.light_direction !=
          Shader::InvalidUniform) {
        QVector3D light_dir = stone.params.light_direction;
        if (!light_dir.isNull()) {
          light_dir.normalize();
        }
        stone_shader->set_uniform(
            m_vegetation_pipeline->m_stone_uniforms.light_direction, light_dir);
      }

      glBindVertexArray(m_vegetation_pipeline->m_stone_vao);
      stone.instance_buffer->bind();
      const auto stride = static_cast<GLsizei>(sizeof(StoneInstanceGpu));
      apply_vertex_attrib_layout({{tex_coord,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride,
                                   offsetof(StoneInstanceGpu, pos_scale)},
                                  {instance_position,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride,
                                   offsetof(StoneInstanceGpu, color_rot)}});
      stone.instance_buffer->unbind();

      glDrawElementsInstanced(GL_TRIANGLES,
                              m_vegetation_pipeline->m_stone_index_count,
                              GL_UNSIGNED_SHORT,
                              nullptr,
                              static_cast<GLsizei>(stone.instance_count));
      glBindVertexArray(0);

      break;
    }
    case TerrainScatterCmd::Species::Plant: {
      if (!m_vegetation_pipeline) {
        break;
      }
      struct PlantView {
        Buffer* instance_buffer;
        std::size_t instance_count;
        const PlantBatchParams& params;
      };
      const PlantView plant{
          deco_cmd_.instance_buffer, deco_cmd_.instance_count, deco_cmd_.plant};

      if ((plant.instance_buffer == nullptr) || plant.instance_count == 0 ||
          (m_vegetation_pipeline->plant_shader() == nullptr) ||
          (m_vegetation_pipeline->m_plant_vao == 0U) ||
          m_vegetation_pipeline->m_plant_index_count == 0) {
        break;
      }

      DepthMaskScope const depth_mask(true);

      glEnable(GL_DEPTH_TEST);
      BlendScope const blend(false);
      GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
      if (prev_cull != 0U) {
        glDisable(GL_CULL_FACE);
      }

      Shader* plant_shader = m_vegetation_pipeline->plant_shader();
      if (m_last_bound_shader != plant_shader) {
        plant_shader->use();
        m_last_bound_shader = plant_shader;
        m_last_bound_texture = nullptr;
      }

      if (m_vegetation_pipeline->m_plant_uniforms.view_proj != Shader::InvalidUniform) {
        plant_shader->set_uniform(m_vegetation_pipeline->m_plant_uniforms.view_proj,
                                  view_proj);
      }
      if (m_vegetation_pipeline->m_plant_uniforms.time != Shader::InvalidUniform) {
        plant_shader->set_uniform(m_vegetation_pipeline->m_plant_uniforms.time,
                                  plant.params.time);
      }
      if (m_vegetation_pipeline->m_plant_uniforms.wind_strength !=
          Shader::InvalidUniform) {
        plant_shader->set_uniform(m_vegetation_pipeline->m_plant_uniforms.wind_strength,
                                  plant.params.wind_strength);
      }
      if (m_vegetation_pipeline->m_plant_uniforms.wind_speed !=
          Shader::InvalidUniform) {
        plant_shader->set_uniform(m_vegetation_pipeline->m_plant_uniforms.wind_speed,
                                  plant.params.wind_speed);
      }
      if (m_vegetation_pipeline->m_plant_uniforms.light_direction !=
          Shader::InvalidUniform) {
        QVector3D light_dir = plant.params.light_direction;
        if (!light_dir.isNull()) {
          light_dir.normalize();
        }
        plant_shader->set_uniform(
            m_vegetation_pipeline->m_plant_uniforms.light_direction, light_dir);
      }

      glBindVertexArray(m_vegetation_pipeline->m_plant_vao);
      plant.instance_buffer->bind();
      const auto stride = static_cast<GLsizei>(sizeof(PlantInstanceGpu));
      apply_vertex_attrib_layout({{instance_position,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride,
                                   offsetof(PlantInstanceGpu, pos_scale)},
                                  {instance_scale,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride,
                                   offsetof(PlantInstanceGpu, color_sway)},
                                  {instance_color,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride,
                                   offsetof(PlantInstanceGpu, type_params)}});
      plant.instance_buffer->unbind();

      glDrawElementsInstanced(GL_TRIANGLES,
                              m_vegetation_pipeline->m_plant_index_count,
                              GL_UNSIGNED_SHORT,
                              nullptr,
                              static_cast<GLsizei>(plant.instance_count));
      glBindVertexArray(0);

      if (prev_cull != 0U) {
        glEnable(GL_CULL_FACE);
      }

      break;
    }
    case TerrainScatterCmd::Species::Pine: {
      if (!m_vegetation_pipeline) {
        break;
      }
      struct PineView {
        Buffer* instance_buffer;
        std::size_t instance_count;
        const PineBatchParams& params;
      };
      const PineView pine{
          deco_cmd_.instance_buffer, deco_cmd_.instance_count, deco_cmd_.pine};

      if ((pine.instance_buffer == nullptr) || pine.instance_count == 0 ||
          (m_vegetation_pipeline->pine_shader() == nullptr) ||
          (m_vegetation_pipeline->m_pine_vao == 0U) ||
          m_vegetation_pipeline->m_pine_index_count == 0) {
        break;
      }

      DepthMaskScope const depth_mask(true);
      glEnable(GL_DEPTH_TEST);
      BlendScope const blend(false);
      GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
      if (prev_cull != 0U) {
        glDisable(GL_CULL_FACE);
      }

      Shader* pine_shader = m_vegetation_pipeline->pine_shader();
      if (m_last_bound_shader != pine_shader) {
        pine_shader->use();
        m_last_bound_shader = pine_shader;
        m_last_bound_texture = nullptr;
      }

      if (m_vegetation_pipeline->m_pine_uniforms.view_proj != Shader::InvalidUniform) {
        pine_shader->set_uniform(m_vegetation_pipeline->m_pine_uniforms.view_proj,
                                 view_proj);
      }
      if (m_vegetation_pipeline->m_pine_uniforms.time != Shader::InvalidUniform) {
        pine_shader->set_uniform(m_vegetation_pipeline->m_pine_uniforms.time,
                                 pine.params.time);
      }
      if (m_vegetation_pipeline->m_pine_uniforms.wind_strength !=
          Shader::InvalidUniform) {
        pine_shader->set_uniform(m_vegetation_pipeline->m_pine_uniforms.wind_strength,
                                 pine.params.wind_strength);
      }
      if (m_vegetation_pipeline->m_pine_uniforms.wind_speed != Shader::InvalidUniform) {
        pine_shader->set_uniform(m_vegetation_pipeline->m_pine_uniforms.wind_speed,
                                 pine.params.wind_speed);
      }
      if (m_vegetation_pipeline->m_pine_uniforms.light_direction !=
          Shader::InvalidUniform) {
        QVector3D light_dir = pine.params.light_direction;
        if (!light_dir.isNull()) {
          light_dir.normalize();
        }
        pine_shader->set_uniform(m_vegetation_pipeline->m_pine_uniforms.light_direction,
                                 light_dir);
      }

      glBindVertexArray(m_vegetation_pipeline->m_pine_vao);
      pine.instance_buffer->bind();
      const auto stride = static_cast<GLsizei>(sizeof(PineInstanceGpu));
      apply_vertex_attrib_layout({{instance_position,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride,
                                   offsetof(PineInstanceGpu, pos_scale)},
                                  {instance_scale,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride,
                                   offsetof(PineInstanceGpu, color_sway)},
                                  {instance_color,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride,
                                   offsetof(PineInstanceGpu, rotation)}});
      pine.instance_buffer->unbind();

      glDrawElementsInstanced(GL_TRIANGLES,
                              m_vegetation_pipeline->m_pine_index_count,
                              GL_UNSIGNED_SHORT,
                              nullptr,
                              static_cast<GLsizei>(pine.instance_count));
      glBindVertexArray(0);

      if (prev_cull != 0U) {
        glEnable(GL_CULL_FACE);
      }

      break;
    }
    case TerrainScatterCmd::Species::Olive: {
      if (!m_vegetation_pipeline) {
        break;
      }
      struct OliveView {
        Buffer* instance_buffer;
        std::size_t instance_count;
        const OliveBatchParams& params;
      };
      const OliveView olive{
          deco_cmd_.instance_buffer, deco_cmd_.instance_count, deco_cmd_.olive};

      if ((olive.instance_buffer == nullptr) || olive.instance_count == 0 ||
          (m_vegetation_pipeline->olive_shader() == nullptr) ||
          (m_vegetation_pipeline->m_olive_vao == 0U) ||
          m_vegetation_pipeline->m_olive_index_count == 0) {
        break;
      }

      DepthMaskScope const depth_mask(true);
      glEnable(GL_DEPTH_TEST);
      BlendScope const blend(false);
      GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
      if (prev_cull != 0U) {
        glDisable(GL_CULL_FACE);
      }

      Shader* olive_shader = m_vegetation_pipeline->olive_shader();
      if (m_last_bound_shader != olive_shader) {
        olive_shader->use();
        m_last_bound_shader = olive_shader;
        m_last_bound_texture = nullptr;
      }

      if (m_vegetation_pipeline->m_olive_uniforms.view_proj != Shader::InvalidUniform) {
        olive_shader->set_uniform(m_vegetation_pipeline->m_olive_uniforms.view_proj,
                                  view_proj);
      }
      if (m_vegetation_pipeline->m_olive_uniforms.time != Shader::InvalidUniform) {
        olive_shader->set_uniform(m_vegetation_pipeline->m_olive_uniforms.time,
                                  olive.params.time);
      }
      if (m_vegetation_pipeline->m_olive_uniforms.wind_strength !=
          Shader::InvalidUniform) {
        olive_shader->set_uniform(m_vegetation_pipeline->m_olive_uniforms.wind_strength,
                                  olive.params.wind_strength);
      }
      if (m_vegetation_pipeline->m_olive_uniforms.wind_speed !=
          Shader::InvalidUniform) {
        olive_shader->set_uniform(m_vegetation_pipeline->m_olive_uniforms.wind_speed,
                                  olive.params.wind_speed);
      }
      if (m_vegetation_pipeline->m_olive_uniforms.light_direction !=
          Shader::InvalidUniform) {
        QVector3D light_dir = olive.params.light_direction;
        if (!light_dir.isNull()) {
          light_dir.normalize();
        }
        olive_shader->set_uniform(
            m_vegetation_pipeline->m_olive_uniforms.light_direction, light_dir);
      }

      glBindVertexArray(m_vegetation_pipeline->m_olive_vao);
      olive.instance_buffer->bind();
      const auto stride = static_cast<GLsizei>(sizeof(OliveInstanceGpu));
      apply_vertex_attrib_layout({{instance_position,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride,
                                   offsetof(OliveInstanceGpu, pos_scale)},
                                  {instance_scale,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride,
                                   offsetof(OliveInstanceGpu, color_sway)},
                                  {instance_color,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride,
                                   offsetof(OliveInstanceGpu, rotation)}});
      olive.instance_buffer->unbind();

      glDrawElementsInstanced(GL_TRIANGLES,
                              m_vegetation_pipeline->m_olive_index_count,
                              GL_UNSIGNED_SHORT,
                              nullptr,
                              static_cast<GLsizei>(olive.instance_count));
      glBindVertexArray(0);

      if (prev_cull != 0U) {
        glEnable(GL_CULL_FACE);
      }

      break;
    }
    case TerrainScatterCmd::Species::FireCamp: {
      if (!m_vegetation_pipeline) {
        break;
      }
      struct FireCampView {
        Buffer* instance_buffer;
        std::size_t instance_count;
        const FireCampBatchParams& params;
      };
      const FireCampView firecamp{
          deco_cmd_.instance_buffer, deco_cmd_.instance_count, deco_cmd_.firecamp};

      if ((firecamp.instance_buffer == nullptr) || firecamp.instance_count == 0 ||
          (m_vegetation_pipeline->firecamp_shader() == nullptr) ||
          (m_vegetation_pipeline->m_firecamp_vao == 0U) ||
          m_vegetation_pipeline->m_firecamp_index_count == 0) {
        break;
      }

      DepthMaskScope const depth_mask(true);
      glEnable(GL_DEPTH_TEST);
      BlendScope const blend(true);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
      if (prev_cull != 0U) {
        glDisable(GL_CULL_FACE);
      }

      Shader* firecamp_shader = m_vegetation_pipeline->firecamp_shader();
      if (m_last_bound_shader != firecamp_shader) {
        firecamp_shader->use();
        m_last_bound_shader = firecamp_shader;
        m_last_bound_texture = nullptr;
      }

      if (m_vegetation_pipeline->m_firecamp_uniforms.view_proj !=
          Shader::InvalidUniform) {
        firecamp_shader->set_uniform(
            m_vegetation_pipeline->m_firecamp_uniforms.view_proj, view_proj);
      }
      if (m_vegetation_pipeline->m_firecamp_uniforms.time != Shader::InvalidUniform) {
        firecamp_shader->set_uniform(m_vegetation_pipeline->m_firecamp_uniforms.time,
                                     firecamp.params.time);
      }
      if (m_vegetation_pipeline->m_firecamp_uniforms.flicker_speed !=
          Shader::InvalidUniform) {
        firecamp_shader->set_uniform(
            m_vegetation_pipeline->m_firecamp_uniforms.flicker_speed,
            firecamp.params.flicker_speed);
      }
      if (m_vegetation_pipeline->m_firecamp_uniforms.flicker_amount !=
          Shader::InvalidUniform) {
        firecamp_shader->set_uniform(
            m_vegetation_pipeline->m_firecamp_uniforms.flicker_amount,
            firecamp.params.flicker_amount);
      }
      if (m_vegetation_pipeline->m_firecamp_uniforms.glow_strength !=
          Shader::InvalidUniform) {
        firecamp_shader->set_uniform(
            m_vegetation_pipeline->m_firecamp_uniforms.glow_strength,
            firecamp.params.glow_strength);
      }
      if (m_vegetation_pipeline->m_firecamp_uniforms.camera_right !=
          Shader::InvalidUniform) {
        QVector3D camera_right = cam.get_right_vector();
        if (camera_right.lengthSquared() < 1e-6F) {
          camera_right = QVector3D(1.0F, 0.0F, 0.0F);
        } else {
          camera_right.normalize();
        }
        firecamp_shader->set_uniform(
            m_vegetation_pipeline->m_firecamp_uniforms.camera_right, camera_right);
      }
      if (m_vegetation_pipeline->m_firecamp_uniforms.camera_forward !=
          Shader::InvalidUniform) {
        QVector3D camera_forward = cam.get_forward_vector();
        if (camera_forward.lengthSquared() < 1e-6F) {
          camera_forward = QVector3D(0.0F, 0.0F, -1.0F);
        } else {
          camera_forward.normalize();
        }
        firecamp_shader->set_uniform(
            m_vegetation_pipeline->m_firecamp_uniforms.camera_forward, camera_forward);
      }

      if (m_vegetation_pipeline->m_firecamp_uniforms.fire_texture !=
          Shader::InvalidUniform) {
        if (m_resources && (m_resources->white() != nullptr)) {
          m_resources->white()->bind(0);
          firecamp_shader->set_uniform(
              m_vegetation_pipeline->m_firecamp_uniforms.fire_texture, 0);
        }
      }

      glBindVertexArray(m_vegetation_pipeline->m_firecamp_vao);
      firecamp.instance_buffer->bind();
      const auto stride = static_cast<GLsizei>(sizeof(FireCampInstanceGpu));
      apply_vertex_attrib_layout({{instance_position,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride,
                                   offsetof(FireCampInstanceGpu, pos_intensity)},
                                  {instance_scale,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride,
                                   offsetof(FireCampInstanceGpu, radius_phase)}});
      firecamp.instance_buffer->unbind();

      glDrawElementsInstanced(GL_TRIANGLES,
                              m_vegetation_pipeline->m_firecamp_index_count,
                              GL_UNSIGNED_SHORT,
                              nullptr,
                              static_cast<GLsizei>(firecamp.instance_count));
      glBindVertexArray(0);

      if (prev_cull != 0U) {
        glEnable(GL_CULL_FACE);
      }

      break;
    }

    case TerrainScatterCmd::Species::Tent:
    case TerrainScatterCmd::Species::SupplyCart:
    case TerrainScatterCmd::Species::WeaponRack:
    case TerrainScatterCmd::Species::Ruins:
    case TerrainScatterCmd::Species::DeadTree:
    case TerrainScatterCmd::Species::IronOre:
    case TerrainScatterCmd::Species::MagicShrine: {
      if (!m_vegetation_pipeline) {
        break;
      }

      Shader* prop_shader = nullptr;
      GLuint prop_vao = 0U;
      GLsizei prop_idx_count = 0;
      const BackendPipelines::VegetationPipeline::PropUniforms* prop_uniforms = nullptr;
      QVector3D prop_light_dir;

      switch (deco_cmd_.species) {
      case TerrainScatterCmd::Species::Tent:
        prop_shader = m_vegetation_pipeline->tent_shader();
        prop_vao = m_vegetation_pipeline->m_tent_vao;
        prop_idx_count = m_vegetation_pipeline->m_tent_index_count;
        prop_uniforms = &m_vegetation_pipeline->m_tent_uniforms;
        prop_light_dir = deco_cmd_.tent.light_direction;
        break;
      case TerrainScatterCmd::Species::SupplyCart:
        prop_shader = m_vegetation_pipeline->supply_cart_shader();
        prop_vao = m_vegetation_pipeline->m_supply_cart_vao;
        prop_idx_count = m_vegetation_pipeline->m_supply_cart_index_count;
        prop_uniforms = &m_vegetation_pipeline->m_supply_cart_uniforms;
        prop_light_dir = deco_cmd_.supply_cart.light_direction;
        break;
      case TerrainScatterCmd::Species::WeaponRack:
        prop_shader = m_vegetation_pipeline->weapon_rack_shader();
        prop_vao = m_vegetation_pipeline->m_weapon_rack_vao;
        prop_idx_count = m_vegetation_pipeline->m_weapon_rack_index_count;
        prop_uniforms = &m_vegetation_pipeline->m_weapon_rack_uniforms;
        prop_light_dir = deco_cmd_.weapon_rack.light_direction;
        break;
      case TerrainScatterCmd::Species::Ruins:
        prop_shader = m_vegetation_pipeline->ruins_shader();
        prop_vao = m_vegetation_pipeline->m_ruins_vao;
        prop_idx_count = m_vegetation_pipeline->m_ruins_index_count;
        prop_uniforms = &m_vegetation_pipeline->m_ruins_uniforms;
        prop_light_dir = deco_cmd_.ruins.light_direction;
        break;
      case TerrainScatterCmd::Species::DeadTree:
        prop_shader = m_vegetation_pipeline->dead_tree_shader();
        prop_vao = m_vegetation_pipeline->m_dead_tree_vao;
        prop_idx_count = m_vegetation_pipeline->m_dead_tree_index_count;
        prop_uniforms = &m_vegetation_pipeline->m_dead_tree_uniforms;
        prop_light_dir = deco_cmd_.dead_tree.light_direction;
        break;
      case TerrainScatterCmd::Species::IronOre:
        prop_shader = m_vegetation_pipeline->iron_ore_shader();
        prop_vao = m_vegetation_pipeline->m_iron_ore_vao;
        prop_idx_count = m_vegetation_pipeline->m_iron_ore_index_count;
        prop_uniforms = &m_vegetation_pipeline->m_iron_ore_uniforms;
        prop_light_dir = deco_cmd_.iron_ore.light_direction;
        break;
      case TerrainScatterCmd::Species::MagicShrine:
        prop_shader = m_vegetation_pipeline->magic_shrine_shader();
        prop_vao = m_vegetation_pipeline->m_magic_shrine_vao;
        prop_idx_count = m_vegetation_pipeline->m_magic_shrine_index_count;
        prop_uniforms = &m_vegetation_pipeline->m_magic_shrine_uniforms;
        prop_light_dir = deco_cmd_.magic_shrine.light_direction;
        break;
      default:
        break;
      }

      if (prop_shader == nullptr || prop_vao == 0U || prop_idx_count == 0 ||
          deco_cmd_.instance_buffer == nullptr || deco_cmd_.instance_count == 0 ||
          prop_uniforms == nullptr) {
        break;
      }

      DepthMaskScope const depth_mask(true);
      BlendScope const blend(false);
      GLboolean const prev_cull2 = glIsEnabled(GL_CULL_FACE);
      if (prev_cull2 != 0U) {
        glDisable(GL_CULL_FACE);
      }

      if (m_last_bound_shader != prop_shader) {
        prop_shader->use();
        m_last_bound_shader = prop_shader;
        m_last_bound_texture = nullptr;
      }

      if (prop_uniforms->view_proj != Shader::InvalidUniform) {
        prop_shader->set_uniform(prop_uniforms->view_proj, view_proj);
      }
      if (prop_uniforms->light_direction != Shader::InvalidUniform) {
        QVector3D ld = prop_light_dir;
        if (!ld.isNull()) {
          ld.normalize();
        }
        prop_shader->set_uniform(prop_uniforms->light_direction, ld);
      }
      if (prop_uniforms->camera_pos != Shader::InvalidUniform) {
        prop_shader->set_uniform(prop_uniforms->camera_pos, cam.get_position());
      }
      if (prop_uniforms->time != Shader::InvalidUniform) {
        prop_shader->set_uniform(prop_uniforms->time, m_animation_time);
      }
      if (prop_uniforms->magic_strength != Shader::InvalidUniform) {
        float const magic_strength =
            (deco_cmd_.species == TerrainScatterCmd::Species::IronOre ||
             deco_cmd_.species == TerrainScatterCmd::Species::MagicShrine)
                ? (deco_cmd_.species == TerrainScatterCmd::Species::MagicShrine ? 1.18F
                                                                                : 1.15F)
                : 0.0F;
        prop_shader->set_uniform(prop_uniforms->magic_strength, magic_strength);
      }

      glBindVertexArray(prop_vao);
      deco_cmd_.instance_buffer->bind();
      const auto stride2 = static_cast<GLsizei>(sizeof(TentInstanceGpu));
      apply_vertex_attrib_layout({{tex_coord,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride2,
                                   offsetof(TentInstanceGpu, pos_scale)},
                                  {instance_position,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride2,
                                   offsetof(TentInstanceGpu, color_rot)}});
      deco_cmd_.instance_buffer->unbind();

      glDrawElementsInstanced(GL_TRIANGLES,
                              prop_idx_count,
                              GL_UNSIGNED_SHORT,
                              nullptr,
                              static_cast<GLsizei>(deco_cmd_.instance_count));
      glBindVertexArray(0);

      if (prev_cull2 != 0U) {
        glEnable(GL_CULL_FACE);
      }

      break;
    }
    }
    break;
  }
  default:
    break;
  }
}

} // namespace Render::GL
