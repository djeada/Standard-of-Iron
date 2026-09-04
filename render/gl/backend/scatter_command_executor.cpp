#include "command_executor_common.h"

namespace Render::GL {

using namespace Render::GL::ColorIndex;
using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

namespace {

struct FoliageDraw {
  Shader* shader{nullptr};
  const BackendPipelines::StaticMeshBuffers* mesh{nullptr};
  const BackendPipelines::VegetationPipeline::FoliageUniforms* uniforms{nullptr};
};

auto resolve_foliage_draw(const BackendPipelines::VegetationPipeline& veg,
                          TerrainScatterCmd::Species species) -> FoliageDraw {
  using S = TerrainScatterCmd::Species;
  switch (species) {
  case S::Plant:
    return {veg.plant_shader(), &veg.m_plant_mesh, &veg.m_plant_uniforms};
  case S::Pine:
    return {veg.pine_shader(), &veg.m_pine_mesh, &veg.m_pine_uniforms};
  case S::Olive:
    return {veg.olive_shader(), &veg.m_olive_mesh, &veg.m_olive_uniforms};
  case S::Cypress:
    return {veg.pine_shader(), &veg.m_cypress_mesh, &veg.m_pine_uniforms};
  case S::Palm:
    return {veg.olive_shader(), &veg.m_palm_mesh, &veg.m_olive_uniforms};
  default:
    return {};
  }
}

struct PropDraw {
  Shader* shader{nullptr};
  const BackendPipelines::StaticMeshBuffers* mesh{nullptr};
  const BackendPipelines::VegetationPipeline::PropUniforms* uniforms{nullptr};
  float magic_strength{0.0F};
};

auto resolve_prop_draw(const BackendPipelines::VegetationPipeline& veg,
                       TerrainScatterCmd::Species species) -> PropDraw {
  using S = TerrainScatterCmd::Species;
  switch (species) {
  case S::Tent:
    return {veg.tent_shader(), &veg.m_tent_mesh, &veg.m_tent_uniforms};
  case S::SupplyCart:
    return {
        veg.supply_cart_shader(), &veg.m_supply_cart_mesh, &veg.m_supply_cart_uniforms};
  case S::WeaponRack:
    return {
        veg.weapon_rack_shader(), &veg.m_weapon_rack_mesh, &veg.m_weapon_rack_uniforms};
  case S::Ruins:
    return {veg.ruins_shader(), &veg.m_ruins_mesh, &veg.m_ruins_uniforms};
  case S::AbandonedHome:
    return {veg.ruins_shader(), &veg.m_abandoned_home_mesh, &veg.m_ruins_uniforms};
  case S::DeadTree:
    return {veg.dead_tree_shader(), &veg.m_dead_tree_mesh, &veg.m_dead_tree_uniforms};
  case S::Statue:
    return {veg.statue_shader(), &veg.m_statue_mesh, &veg.m_statue_uniforms};
  case S::IronOre:
    return {
        veg.iron_ore_shader(), &veg.m_iron_ore_mesh, &veg.m_iron_ore_uniforms, 1.15F};
  case S::MagicShrine:
    return {veg.magic_shrine_shader(),
            &veg.m_magic_shrine_mesh,
            &veg.m_magic_shrine_uniforms,
            1.18F};
  case S::CursedGoldVein:
    return {veg.cursed_gold_vein_shader(),
            &veg.m_cursed_gold_vein_mesh,
            &veg.m_cursed_gold_vein_uniforms,
            1.10F};
  default:
    return {};
  }
}

} // namespace

void Backend::execute_scatter_commands(const PreparedBatch& prepared,
                                       CommandExecutionContext& context) {
  const auto& queue = context.queue;
  const Camera& cam = context.cam;
  const QMatrix4x4& view = context.view;
  const QMatrix4x4& projection = context.projection;
  const QMatrix4x4& view_proj = context.view_proj;
  const float banner_wind_strength = context.banner_wind_strength;
  bool const& polygon_offset_enabled = context.polygon_offset_enabled;

  (void)view;
  (void)projection;
  (void)view_proj;
  (void)banner_wind_strength;
  (void)polygon_offset_enabled;

  const std::size_t i = prepared.start;
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

      DepthMaskScope const depth_mask(true);
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
      bind_visibility_mask(*m_terrain_pipeline->m_grass_shader, deco_cmd_.visibility);

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

      if (m_terrain_pipeline->m_grass_uniforms.viewport_size !=
          Shader::InvalidUniform) {
        m_terrain_pipeline->m_grass_shader->set_uniform(
            m_terrain_pipeline->m_grass_uniforms.viewport_size,
            QVector2D(static_cast<float>(std::max(m_viewport_width, 1)),
                      static_cast<float>(std::max(m_viewport_height, 1))));
      }
      if (m_terrain_pipeline->m_grass_uniforms.camera_pos != Shader::InvalidUniform) {
        m_terrain_pipeline->m_grass_shader->set_uniform(
            m_terrain_pipeline->m_grass_uniforms.camera_pos, cam.get_position());
      }
      if (m_terrain_pipeline->m_grass_uniforms.ambient_boost !=
          Shader::InvalidUniform) {
        m_terrain_pipeline->m_grass_shader->set_uniform(
            m_terrain_pipeline->m_grass_uniforms.ambient_boost,
            grass.params.ambient_boost);
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
          (m_vegetation_pipeline->m_stone_mesh.vao == 0U) ||
          m_vegetation_pipeline->m_stone_mesh.index_count == 0) {
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
      bind_visibility_mask(*stone_shader, deco_cmd_.visibility);

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
      if (m_vegetation_pipeline->m_stone_uniforms.camera_pos !=
          Shader::InvalidUniform) {
        stone_shader->set_uniform(m_vegetation_pipeline->m_stone_uniforms.camera_pos,
                                  cam.get_position());
      }

      glBindVertexArray(m_vegetation_pipeline->m_stone_mesh.vao);
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
                              m_vegetation_pipeline->m_stone_mesh.index_count,
                              GL_UNSIGNED_SHORT,
                              nullptr,
                              static_cast<GLsizei>(stone.instance_count));
      glBindVertexArray(0);

      break;
    }
    case TerrainScatterCmd::Species::Plant:
    case TerrainScatterCmd::Species::Pine:
    case TerrainScatterCmd::Species::Olive:
    case TerrainScatterCmd::Species::Cypress:
    case TerrainScatterCmd::Species::Palm: {
      if (!m_vegetation_pipeline) {
        break;
      }

      const FoliageDraw foliage =
          resolve_foliage_draw(*m_vegetation_pipeline, deco_cmd_.species);
      const FoliageBatchParams& params = deco_cmd_.foliage;

      if (deco_cmd_.instance_buffer == nullptr || deco_cmd_.instance_count == 0 ||
          foliage.shader == nullptr || foliage.mesh == nullptr ||
          !foliage.mesh->drawable()) {
        break;
      }

      DepthMaskScope const depth_mask(true);
      glEnable(GL_DEPTH_TEST);
      BlendScope const blend(false);
      GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
      if (prev_cull != 0U) {
        glDisable(GL_CULL_FACE);
      }

      Shader* shader = foliage.shader;
      if (m_last_bound_shader != shader) {
        shader->use();
        m_last_bound_shader = shader;
        m_last_bound_texture = nullptr;
      }
      bind_visibility_mask(*shader, deco_cmd_.visibility);

      const auto& uniforms = *foliage.uniforms;
      if (uniforms.view_proj != Shader::InvalidUniform) {
        shader->set_uniform(uniforms.view_proj, view_proj);
      }
      if (uniforms.time != Shader::InvalidUniform) {
        shader->set_uniform(uniforms.time, params.time);
      }
      if (uniforms.wind_strength != Shader::InvalidUniform) {
        shader->set_uniform(uniforms.wind_strength, params.wind_strength);
      }
      if (uniforms.wind_speed != Shader::InvalidUniform) {
        shader->set_uniform(uniforms.wind_speed, params.wind_speed);
      }
      if (uniforms.light_direction != Shader::InvalidUniform) {
        QVector3D light_dir = params.light_direction;
        if (!light_dir.isNull()) {
          light_dir.normalize();
        }
        shader->set_uniform(uniforms.light_direction, light_dir);
      }
      if (uniforms.camera_pos != Shader::InvalidUniform) {
        shader->set_uniform(uniforms.camera_pos, cam.get_position());
      }

      glBindVertexArray(foliage.mesh->vao);
      deco_cmd_.instance_buffer->bind();
      const auto stride = static_cast<GLsizei>(sizeof(TreeInstanceGpu));
      apply_vertex_attrib_layout({{instance_position,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride,
                                   offsetof(TreeInstanceGpu, pos_scale)},
                                  {instance_scale,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride,
                                   offsetof(TreeInstanceGpu, color_sway)},
                                  {instance_color,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride,
                                   offsetof(TreeInstanceGpu, rotation)}});
      deco_cmd_.instance_buffer->unbind();

      glDrawElementsInstanced(GL_TRIANGLES,
                              foliage.mesh->index_count,
                              GL_UNSIGNED_SHORT,
                              nullptr,
                              static_cast<GLsizei>(deco_cmd_.instance_count));
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
          (m_vegetation_pipeline->m_firecamp_mesh.vao == 0U) ||
          m_vegetation_pipeline->m_firecamp_mesh.index_count == 0) {
        break;
      }

      DepthMaskScope const depth_mask(false);
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

      glBindVertexArray(m_vegetation_pipeline->m_firecamp_mesh.vao);
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
                              m_vegetation_pipeline->m_firecamp_mesh.index_count,
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
    case TerrainScatterCmd::Species::MagicShrine:
    case TerrainScatterCmd::Species::CursedGoldVein:
    case TerrainScatterCmd::Species::AbandonedHome:
    case TerrainScatterCmd::Species::Statue: {
      if (!m_vegetation_pipeline) {
        break;
      }

      const PropDraw prop =
          resolve_prop_draw(*m_vegetation_pipeline, deco_cmd_.species);

      Shader* prop_shader = prop.shader;
      GLuint const prop_vao = prop.mesh != nullptr ? prop.mesh->vao : 0U;
      GLsizei const prop_idx_count = prop.mesh != nullptr ? prop.mesh->index_count : 0;
      const BackendPipelines::VegetationPipeline::PropUniforms* prop_uniforms =
          prop.uniforms;
      const QVector3D prop_light_dir = deco_cmd_.prop.light_direction;

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
      bind_visibility_mask(*prop_shader, deco_cmd_.visibility);

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
        const float magic = deco_cmd_.prop.magic_strength >= 0.0F
                                ? deco_cmd_.prop.magic_strength
                                : prop.magic_strength;
        prop_shader->set_uniform(prop_uniforms->magic_strength, magic);
      }

      glBindVertexArray(prop_vao);
      deco_cmd_.instance_buffer->bind();
      const auto stride2 = static_cast<GLsizei>(sizeof(PropInstanceGpu));
      apply_vertex_attrib_layout({{tex_coord,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride2,
                                   offsetof(PropInstanceGpu, pos_scale)},
                                  {instance_position,
                                   vec4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   stride2,
                                   offsetof(PropInstanceGpu, color_rot)}});
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
