#include <QMatrix4x4>
#include <QVector3D>

#include <cstddef>

#include "render/backend/pipeline_draw_context.h"
#include "render/backend/state_scopes.h"
#include "render/backend/vegetation_pipeline.h"
#include "render/backend/visibility_mask_binding.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/draw_commands.h"
#include "render/gl/buffer.h"
#include "render/gl/draw_tally.h"
#include "render/gl/platform_gl.h"
#include "render/gl/render_constants.h"
#include "render/gl/vertex_attrib_layout.h"
#include "scene/camera.h"

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

auto VegetationPipeline::resolve_foliage_draw(TerrainScatterCmd::Species species) const
    -> FoliageDraw {
  using S = TerrainScatterCmd::Species;
  switch (species) {
  case S::Plant:
    return {plant_shader(), &m_plant_mesh, &m_plant_uniforms};
  case S::Pine:
    return {pine_shader(), &m_pine_mesh, &m_pine_uniforms};
  case S::Olive:
    return {olive_shader(), &m_olive_mesh, &m_olive_uniforms};
  case S::Cypress:
    return {cypress_shader(), &m_cypress_mesh, &m_cypress_uniforms};
  case S::Palm:
    return {palm_shader(), &m_palm_mesh, &m_palm_uniforms};
  default:
    return {};
  }
}

auto VegetationPipeline::resolve_prop_draw(TerrainScatterCmd::Species species) const
    -> PropDraw {
  using S = TerrainScatterCmd::Species;
  switch (species) {
  case S::Tent:
    return {tent_shader(), &m_tent_mesh, &m_tent_uniforms};
  case S::SupplyCart:
    return {supply_cart_shader(), &m_supply_cart_mesh, &m_supply_cart_uniforms};
  case S::WeaponRack:
    return {weapon_rack_shader(), &m_weapon_rack_mesh, &m_weapon_rack_uniforms};
  case S::Ruins:
    return {ruins_shader(), &m_ruins_mesh, &m_ruins_uniforms};
  case S::AbandonedHome:
    return {ruins_shader(), &m_abandoned_home_mesh, &m_ruins_uniforms};
  case S::DeadTree:
    return {dead_tree_shader(), &m_dead_tree_mesh, &m_dead_tree_uniforms};
  case S::Statue:
    return {statue_shader(), &m_statue_mesh, &m_statue_uniforms};
  case S::IronOre:
    return {iron_ore_shader(), &m_iron_ore_mesh, &m_iron_ore_uniforms, 1.15F};
  case S::MagicShrine:
    return {
        magic_shrine_shader(), &m_magic_shrine_mesh, &m_magic_shrine_uniforms, 0.95F};
  case S::CursedGoldVein:
    return {cursed_gold_vein_shader(),
            &m_cursed_gold_vein_mesh,
            &m_cursed_gold_vein_uniforms,
            1.10F};
  default:
    return {};
  }
}

void VegetationPipeline::draw_stone(const TerrainScatterCmd& deco_cmd_,
                                    PipelineDrawContext& context) {
  const Camera& cam = *context.frame.camera;
  const QMatrix4x4& view_proj = context.frame.view_proj;

  struct StoneView {
    Buffer* instance_buffer;
    std::size_t instance_count;
    const StoneBatchParams& params;
  };
  const StoneView stone{
      deco_cmd_.instance_buffer, deco_cmd_.instance_count, deco_cmd_.stone};
  if ((stone.instance_buffer == nullptr) || stone.instance_count == 0 ||
      (stone_shader() == nullptr) || !m_stone_mesh.vao ||
      m_stone_mesh.index_count == 0) {
    return;
  }

  DepthMaskScope const depth_mask(true);
  BlendScope const blend(false);

  Shader* stone_shader = this->stone_shader();
  if (context.state.bound_shader != stone_shader) {
    stone_shader->use();
    context.state.bound_shader = stone_shader;
    context.state.bound_texture = nullptr;
  }
  bind_visibility_mask(*stone_shader, deco_cmd_.visibility);

  if (m_stone_uniforms.view_proj != Shader::InvalidUniform) {
    stone_shader->set_uniform(m_stone_uniforms.view_proj, view_proj);
  }
  if (m_stone_uniforms.light_direction != Shader::InvalidUniform) {
    QVector3D light_dir = stone.params.light_direction;
    if (!light_dir.isNull()) {
      light_dir.normalize();
    }
    stone_shader->set_uniform(m_stone_uniforms.light_direction, light_dir);
  }
  if (m_stone_uniforms.camera_pos != Shader::InvalidUniform) {
    stone_shader->set_uniform(m_stone_uniforms.camera_pos, cam.get_position());
  }

  glBindVertexArray(m_stone_mesh.vao.id());
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
                               offsetof(StoneInstanceGpu, color_rot)},
                              {instance_scale,
                               vec4,
                               GL_FLOAT,
                               GL_FALSE,
                               stride,
                               offsetof(StoneInstanceGpu, ground_fit)}});
  stone.instance_buffer->unbind();

  glDrawElementsInstanced(GL_TRIANGLES,
                          m_stone_mesh.index_count,
                          GL_UNSIGNED_SHORT,
                          nullptr,
                          static_cast<GLsizei>(stone.instance_count));
  tally_draw(static_cast<std::size_t>(m_stone_mesh.index_count), stone.instance_count);
  glBindVertexArray(0);
}

void VegetationPipeline::draw_foliage(const TerrainScatterCmd& deco_cmd_,
                                      PipelineDrawContext& context) {
  const Camera& cam = *context.frame.camera;
  const QMatrix4x4& view_proj = context.frame.view_proj;

  const FoliageDraw foliage = resolve_foliage_draw(deco_cmd_.species);
  const FoliageBatchParams& params = deco_cmd_.foliage;

  if (deco_cmd_.instance_buffer == nullptr || deco_cmd_.instance_count == 0 ||
      foliage.shader == nullptr || foliage.mesh == nullptr ||
      !foliage.mesh->drawable()) {
    return;
  }

  DepthMaskScope const depth_mask(true);
  glEnable(GL_DEPTH_TEST);
  BlendScope const blend(false);
  GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
  if (prev_cull != 0U) {
    glDisable(GL_CULL_FACE);
  }

  Shader* shader = foliage.shader;
  if (context.state.bound_shader != shader) {
    shader->use();
    context.state.bound_shader = shader;
    context.state.bound_texture = nullptr;
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
  if (uniforms.snow_coverage != Shader::InvalidUniform) {
    shader->set_uniform(uniforms.snow_coverage, params.snow_coverage);
  }

  glBindVertexArray(foliage.mesh->vao.id());
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
  tally_draw(static_cast<std::size_t>(foliage.mesh->index_count),
             deco_cmd_.instance_count);
  glBindVertexArray(0);

  if (prev_cull != 0U) {
    glEnable(GL_CULL_FACE);
  }
}

void VegetationPipeline::draw_firecamp(const TerrainScatterCmd& deco_cmd_,
                                       PipelineDrawContext& context) {
  const Camera& cam = *context.frame.camera;
  const QMatrix4x4& view_proj = context.frame.view_proj;

  struct FireCampView {
    Buffer* instance_buffer;
    std::size_t instance_count;
    const FireCampBatchParams& params;
  };
  const FireCampView firecamp{
      deco_cmd_.instance_buffer, deco_cmd_.instance_count, deco_cmd_.firecamp};

  if ((firecamp.instance_buffer == nullptr) || firecamp.instance_count == 0 ||
      (firecamp_shader() == nullptr) || !m_firecamp_mesh.vao ||
      m_firecamp_mesh.index_count == 0) {
    return;
  }

  DepthMaskScope const depth_mask(false);
  glEnable(GL_DEPTH_TEST);
  BlendScope const blend(true);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
  if (prev_cull != 0U) {
    glDisable(GL_CULL_FACE);
  }

  Shader* firecamp_shader = this->firecamp_shader();
  if (context.state.bound_shader != firecamp_shader) {
    firecamp_shader->use();
    context.state.bound_shader = firecamp_shader;
    context.state.bound_texture = nullptr;
  }

  if (m_firecamp_uniforms.view_proj != Shader::InvalidUniform) {
    firecamp_shader->set_uniform(m_firecamp_uniforms.view_proj, view_proj);
  }
  if (m_firecamp_uniforms.time != Shader::InvalidUniform) {
    firecamp_shader->set_uniform(m_firecamp_uniforms.time, firecamp.params.time);
  }
  if (m_firecamp_uniforms.flicker_speed != Shader::InvalidUniform) {
    firecamp_shader->set_uniform(m_firecamp_uniforms.flicker_speed,
                                 firecamp.params.flicker_speed);
  }
  if (m_firecamp_uniforms.flicker_amount != Shader::InvalidUniform) {
    firecamp_shader->set_uniform(m_firecamp_uniforms.flicker_amount,
                                 firecamp.params.flicker_amount);
  }
  if (m_firecamp_uniforms.glow_strength != Shader::InvalidUniform) {
    firecamp_shader->set_uniform(m_firecamp_uniforms.glow_strength,
                                 firecamp.params.glow_strength);
  }
  if (m_firecamp_uniforms.camera_right != Shader::InvalidUniform) {
    QVector3D camera_right = cam.get_right_vector();
    if (camera_right.lengthSquared() < 1e-6F) {
      camera_right = QVector3D(1.0F, 0.0F, 0.0F);
    } else {
      camera_right.normalize();
    }
    firecamp_shader->set_uniform(m_firecamp_uniforms.camera_right, camera_right);
  }
  if (m_firecamp_uniforms.camera_forward != Shader::InvalidUniform) {
    QVector3D camera_forward = cam.get_forward_vector();
    if (camera_forward.lengthSquared() < 1e-6F) {
      camera_forward = QVector3D(0.0F, 0.0F, -1.0F);
    } else {
      camera_forward.normalize();
    }
    firecamp_shader->set_uniform(m_firecamp_uniforms.camera_forward, camera_forward);
  }

  glBindVertexArray(m_firecamp_mesh.vao.id());
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
                               offsetof(FireCampInstanceGpu, radius_phase)},
                              {instance_color,
                               vec4,
                               GL_FLOAT,
                               GL_FALSE,
                               stride,
                               offsetof(FireCampInstanceGpu, ground)}});
  firecamp.instance_buffer->unbind();

  glDrawElementsInstanced(GL_TRIANGLES,
                          m_firecamp_mesh.index_count,
                          GL_UNSIGNED_SHORT,
                          nullptr,
                          static_cast<GLsizei>(firecamp.instance_count));
  tally_draw(static_cast<std::size_t>(m_firecamp_mesh.index_count),
             firecamp.instance_count);
  glBindVertexArray(0);

  if (prev_cull != 0U) {
    glEnable(GL_CULL_FACE);
  }
}

void VegetationPipeline::draw_prop(const TerrainScatterCmd& deco_cmd_,
                                   PipelineDrawContext& context) {
  const Camera& cam = *context.frame.camera;
  const QMatrix4x4& view_proj = context.frame.view_proj;

  const PropDraw prop = resolve_prop_draw(deco_cmd_.species);

  Shader* prop_shader = prop.shader;
  GLuint const prop_vao = prop.mesh != nullptr ? prop.mesh->vao.id() : 0U;
  GLsizei const prop_idx_count = prop.mesh != nullptr ? prop.mesh->index_count : 0;
  const PropUniforms* prop_uniforms = prop.uniforms;
  const QVector3D prop_light_dir = deco_cmd_.prop.light_direction;

  if (prop_shader == nullptr || prop_vao == 0U || prop_idx_count == 0 ||
      deco_cmd_.instance_buffer == nullptr || deco_cmd_.instance_count == 0 ||
      prop_uniforms == nullptr) {
    return;
  }

  DepthMaskScope const depth_mask(true);
  BlendScope const blend(false);
  GLboolean const prev_cull2 = glIsEnabled(GL_CULL_FACE);
  if (prev_cull2 != 0U) {
    glDisable(GL_CULL_FACE);
  }

  if (context.state.bound_shader != prop_shader) {
    prop_shader->use();
    context.state.bound_shader = prop_shader;
    context.state.bound_texture = nullptr;
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
    prop_shader->set_uniform(prop_uniforms->time, context.frame.animation_time);
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
  tally_draw(static_cast<std::size_t>(prop_idx_count), deco_cmd_.instance_count);
  glBindVertexArray(0);

  if (prev_cull2 != 0U) {
    glEnable(GL_CULL_FACE);
  }
}

} // namespace Render::GL::BackendPipelines
