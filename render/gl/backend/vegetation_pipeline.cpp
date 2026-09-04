#include "vegetation_pipeline.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QString>
#include <qglobal.h>
#include <qopenglcontext.h>
#include <qopenglext.h>
#include <qstringliteral.h>
#include <qvectornd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "gl/shader_cache.h"
#include "mesh_buffers.h"
#include "prop_mesh_builder.h"
#include "render/gl/platform_gl.h"
#include "render/gl/render_constants.h"

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;
using namespace Render::GL::Geometry;

VegetationPipeline::VegetationPipeline(ShaderCache* shader_cache)
    : m_shader_cache(shader_cache) {
}

VegetationPipeline::~VegetationPipeline() {
  shutdown();
}

auto VegetationPipeline::initialize() -> bool {
  initializeOpenGLFunctions();

  if (m_shader_cache == nullptr) {
    return false;
  }

  const std::array<std::pair<GL::Shader**, const char*>, 14> shaders{{
      {&m_stone_shader, "stone_instanced"},
      {&m_plant_shader, "plant_instanced"},
      {&m_pine_shader, "pine_instanced"},
      {&m_olive_shader, "olive_instanced"},
      {&m_firecamp_shader, "firecamp"},
      {&m_tent_shader, "tent_instanced"},
      {&m_supply_cart_shader, "supply_cart_instanced"},
      {&m_weapon_rack_shader, "weapon_rack_instanced"},
      {&m_ruins_shader, "ruins_instanced"},
      {&m_dead_tree_shader, "dead_tree_instanced"},
      {&m_iron_ore_shader, "iron_ore_instanced"},
      {&m_magic_shrine_shader, "magic_shrine_instanced"},
      {&m_cursed_gold_vein_shader, "cursed_gold_vein_instanced"},
      {&m_statue_shader, "statue_instanced"},
  }};

  for (const auto& [slot, name] : shaders) {
    *slot = m_shader_cache->get(QString::fromLatin1(name));
    if (*slot == nullptr) {
      qWarning() << "VegetationPipeline: shader missing:" << name;
    }
  }

  initialize_stone_pipeline();
  initialize_plant_pipeline();
  initialize_pine_pipeline();
  initialize_olive_pipeline();
  initialize_cypress_pipeline();
  initialize_palm_pipeline();
  initialize_fire_camp_pipeline();
  initialize_tent_pipeline();
  initialize_supply_cart_pipeline();
  initialize_weapon_rack_pipeline();
  initialize_ruins_pipeline();
  initialize_dead_tree_pipeline();
  initialize_iron_ore_pipeline();
  initialize_magic_shrine_pipeline();
  initialize_cursed_gold_vein_pipeline();
  initialize_abandoned_home_pipeline();
  initialize_statue_pipeline();
  cache_uniforms();

  m_initialized = true;
  return true;
}

auto VegetationPipeline::all_meshes() -> std::array<StaticMeshBuffers*, k_mesh_count> {
  return {&m_stone_mesh,
          &m_plant_mesh,
          &m_pine_mesh,
          &m_olive_mesh,
          &m_cypress_mesh,
          &m_palm_mesh,
          &m_firecamp_mesh,
          &m_tent_mesh,
          &m_supply_cart_mesh,
          &m_weapon_rack_mesh,
          &m_ruins_mesh,
          &m_dead_tree_mesh,
          &m_iron_ore_mesh,
          &m_magic_shrine_mesh,
          &m_cursed_gold_vein_mesh,
          &m_abandoned_home_mesh,
          &m_statue_mesh};
}

void VegetationPipeline::shutdown() {
  if (QOpenGLContext::currentContext() != nullptr) {
    initializeOpenGLFunctions();
  }
  for (StaticMeshBuffers* mesh : all_meshes()) {
    release_mesh_buffers(*this, *mesh);
  }
  m_initialized = false;
}

void VegetationPipeline::cache_uniforms() {
  if (m_stone_shader != nullptr) {
    m_stone_uniforms.view_proj = m_stone_shader->optional_uniform_handle("u_view_proj");
    m_stone_uniforms.light_direction =
        m_stone_shader->optional_uniform_handle("u_light_direction");
    m_stone_uniforms.camera_pos =
        m_stone_shader->optional_uniform_handle("u_camera_pos");
  }

  auto cache_foliage_uniforms = [](FoliageUniforms& u, GL::Shader* shader) {
    if (shader == nullptr) {
      return;
    }
    u.view_proj = shader->optional_uniform_handle("u_view_proj");
    u.time = shader->uniform_handle("u_time");
    u.wind_strength = shader->uniform_handle("u_wind_strength");
    u.wind_speed = shader->uniform_handle("u_wind_speed");
    u.light_direction = shader->optional_uniform_handle("u_light_direction");
    u.camera_pos = shader->optional_uniform_handle("u_camera_pos");
  };

  cache_foliage_uniforms(m_plant_uniforms, m_plant_shader);
  cache_foliage_uniforms(m_pine_uniforms, m_pine_shader);
  cache_foliage_uniforms(m_olive_uniforms, m_olive_shader);

  if (m_firecamp_shader != nullptr) {
    m_firecamp_uniforms.view_proj =
        m_firecamp_shader->optional_uniform_handle("u_view_proj");
    m_firecamp_uniforms.time = m_firecamp_shader->uniform_handle("u_time");
    m_firecamp_uniforms.flicker_speed =
        m_firecamp_shader->uniform_handle("u_flicker_speed");
    m_firecamp_uniforms.flicker_amount =
        m_firecamp_shader->uniform_handle("u_flicker_amount");
    m_firecamp_uniforms.glow_strength =
        m_firecamp_shader->uniform_handle("u_glow_strength");
    m_firecamp_uniforms.fire_texture =
        m_firecamp_shader->uniform_handle("fire_texture");
    m_firecamp_uniforms.camera_right =
        m_firecamp_shader->uniform_handle("u_camera_right");
    m_firecamp_uniforms.camera_forward =
        m_firecamp_shader->uniform_handle("u_camera_forward");
  }

  auto cache_prop_uniforms = [&](PropUniforms& u, GL::Shader* shader) {
    if (shader == nullptr) {
      return;
    }
    u.view_proj = shader->optional_uniform_handle("u_view_proj");
    u.light_direction = shader->optional_uniform_handle("u_light_direction");
    u.camera_pos = shader->optional_uniform_handle("u_camera_pos");
    u.time = shader->optional_uniform_handle("u_time");
    u.magic_strength = shader->optional_uniform_handle("u_magic_strength");
  };

  cache_prop_uniforms(m_tent_uniforms, m_tent_shader);
  cache_prop_uniforms(m_supply_cart_uniforms, m_supply_cart_shader);
  cache_prop_uniforms(m_weapon_rack_uniforms, m_weapon_rack_shader);
  cache_prop_uniforms(m_ruins_uniforms, m_ruins_shader);
  cache_prop_uniforms(m_dead_tree_uniforms, m_dead_tree_shader);
  cache_prop_uniforms(m_iron_ore_uniforms, m_iron_ore_shader);
  cache_prop_uniforms(m_magic_shrine_uniforms, m_magic_shrine_shader);
  cache_prop_uniforms(m_cursed_gold_vein_uniforms, m_cursed_gold_vein_shader);
  cache_prop_uniforms(m_statue_uniforms, m_statue_shader);
}

void VegetationPipeline::upload_prop_mesh_impl(
    const std::vector<std::pair<QVector3D, QVector3D>>& verts,
    const std::vector<uint16_t>& idx,
    StaticMeshBuffers& mesh) {

  using namespace Render::GL::VertexAttrib;
  using namespace Render::GL::ComponentCount;

  struct V {
    QVector3D pos;
    QVector3D nrm;
  };
  std::vector<V> flat;
  flat.reserve(verts.size());
  for (const auto& [p, n] : verts) {
    flat.push_back({p, n});
  }

  glGenVertexArrays(1, &mesh.vao);
  glBindVertexArray(mesh.vao);

  glGenBuffers(1, &mesh.vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, mesh.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(flat.size() * sizeof(V)),
               flat.data(),
               GL_STATIC_DRAW);
  mesh.vertex_count = static_cast<GLsizei>(flat.size());

  glEnableVertexAttribArray(position);
  glVertexAttribPointer(position,
                        vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(V),
                        reinterpret_cast<void*>(offsetof(V, pos)));
  glEnableVertexAttribArray(normal);
  glVertexAttribPointer(normal,
                        vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(V),
                        reinterpret_cast<void*>(offsetof(V, nrm)));

  glGenBuffers(1, &mesh.index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(idx.size() * sizeof(uint16_t)),
               idx.data(),
               GL_STATIC_DRAW);
  mesh.index_count = static_cast<GLsizei>(idx.size());

  glEnableVertexAttribArray(tex_coord);
  glVertexAttribDivisor(tex_coord, 1);
  glEnableVertexAttribArray(instance_position);
  glVertexAttribDivisor(instance_position, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

} // namespace Render::GL::BackendPipelines
