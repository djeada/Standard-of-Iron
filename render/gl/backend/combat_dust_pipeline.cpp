#include "combat_dust_pipeline.h"

#include <QDebug>
#include <QMatrix4x4>
#include <QOpenGLContext>

#include <cmath>
#include <numbers>
#include <string>
#include <vector>

#include "gl_error_check.h"
#include "render/gl/render_constants.h"
#include "render/gl/shader_cache.h"
#include "spark_orientation.h"
#include "static_mesh_upload.h"

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

namespace {

constexpr float k_min_dust_intensity = 0.01F;

struct EffectRenderState {
  GLboolean cull_enabled{GL_FALSE};
  GLboolean depth_test_enabled{GL_FALSE};
  GLboolean blend_enabled{GL_FALSE};
  GLboolean depth_mask_enabled{GL_TRUE};
  GLint blend_src_rgb{GL_ONE};
  GLint blend_dst_rgb{GL_ZERO};
  GLint blend_equation_rgb{GL_FUNC_ADD};
};

auto capture_effect_render_state() -> EffectRenderState {
  EffectRenderState state;
  state.cull_enabled = glIsEnabled(GL_CULL_FACE);
  state.depth_test_enabled = glIsEnabled(GL_DEPTH_TEST);
  state.blend_enabled = glIsEnabled(GL_BLEND);
  glGetBooleanv(GL_DEPTH_WRITEMASK, &state.depth_mask_enabled);
  glGetIntegerv(GL_BLEND_SRC_RGB, &state.blend_src_rgb);
  glGetIntegerv(GL_BLEND_DST_RGB, &state.blend_dst_rgb);
  glGetIntegerv(GL_BLEND_EQUATION_RGB, &state.blend_equation_rgb);
  return state;
}

void apply_alpha_blend_effect_state() {
  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void apply_overlay_alpha_effect_state() {
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
}

void apply_additive_effect_state() {
  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void restore_effect_render_state(const EffectRenderState& state) {
  glDepthMask(state.depth_mask_enabled);
  glBlendFunc(static_cast<GLenum>(state.blend_src_rgb),
              static_cast<GLenum>(state.blend_dst_rgb));
  Platform::set_blend_equation(static_cast<GLenum>(state.blend_equation_rgb));
  if (state.blend_enabled != 0U) {
    glEnable(GL_BLEND);
  } else {
    glDisable(GL_BLEND);
  }
  if (state.depth_test_enabled != 0U) {
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }
  if (state.cull_enabled != 0U) {
    glEnable(GL_CULL_FACE);
  } else {
    glDisable(GL_CULL_FACE);
  }
}

} // namespace

auto CombatDustPipeline::initialize() -> bool {
  if (m_shader_cache == nullptr) {
    qWarning() << "CombatDustPipeline::initialize: null ShaderCache";
    return false;
  }

  initializeOpenGLFunctions();
  clear_gl_errors();

  m_dust_shader = m_shader_cache->get("combat_dust");
  if (m_dust_shader == nullptr) {
    m_dust_shader =
        m_shader_cache->load("combat_dust",
                             QStringLiteral(":/assets/shaders/combat_dust.vert"),
                             QStringLiteral(":/assets/shaders/combat_dust.frag"));
  }
  if (m_dust_shader == nullptr) {
    qWarning() << "CombatDustPipeline: Failed to get combat_dust shader";
    return false;
  }

  m_blood_shader = m_shader_cache->get("blood_pool");
  if (m_blood_shader == nullptr) {
    m_blood_shader =
        m_shader_cache->load("blood_pool",
                             QStringLiteral(":/assets/shaders/blood_pool.vert"),
                             QStringLiteral(":/assets/shaders/blood_pool.frag"));
    if (m_blood_shader == nullptr) {
      qWarning() << "CombatDustPipeline: Blood pools disabled; failed to load "
                    "blood_pool shader";
    }
  }

  cache_uniforms();

  if (!create_dust_geometry()) {
    qWarning() << "CombatDustPipeline: Failed to create dust geometry";
    return false;
  }

  if (!create_fireball_geometry()) {
    qWarning() << "CombatDustPipeline: Failed to create fireball geometry";
    return false;
  }

  if (!create_metal_spark_geometry()) {
    qWarning() << "CombatDustPipeline: Failed to create metal spark geometry";
    return false;
  }

  if (!create_weapon_arc_geometry()) {
    qWarning() << "CombatDustPipeline: Failed to create weapon arc geometry";
    return false;
  }

  if (m_blood_shader != nullptr && !create_blood_geometry()) {
    qWarning() << "CombatDustPipeline: Blood pools disabled; failed to create "
                  "blood geometry";
    release_mesh_buffers(*this, m_blood_mesh);
    m_blood_shader = nullptr;
    m_blood_uniforms = {};
  }

  qInfo() << "CombatDustPipeline initialized successfully";
  return is_initialized();
}

void CombatDustPipeline::shutdown() {
  release_geometry();
  m_blood_shader = nullptr;
  m_dust_shader = nullptr;
}

void CombatDustPipeline::release_geometry() {
  if (QOpenGLContext::currentContext() != nullptr) {
    initializeOpenGLFunctions();
    clear_gl_errors();
  }
  for (StaticMeshBuffers* mesh : {&m_dust_mesh,
                                  &m_fireball_mesh,
                                  &m_metal_spark_mesh,
                                  &m_weapon_arc_mesh,
                                  &m_blood_mesh}) {
    release_mesh_buffers(*this, *mesh);
  }
}

void CombatDustPipeline::cache_uniforms() {
  m_uniforms = {};
  m_blood_uniforms = {};

  if (m_dust_shader != nullptr) {
    m_uniforms.mvp = m_dust_shader->uniform_handle("u_mvp");
    m_uniforms.model = m_dust_shader->uniform_handle("u_model");
    m_uniforms.time = m_dust_shader->uniform_handle("u_time");
    m_uniforms.center = m_dust_shader->uniform_handle("u_center");
    m_uniforms.radius = m_dust_shader->uniform_handle("u_radius");
    m_uniforms.intensity = m_dust_shader->uniform_handle("u_intensity");
    m_uniforms.dust_color = m_dust_shader->uniform_handle("u_dust_color");
    m_uniforms.effect_type = m_dust_shader->uniform_handle("u_effect_type");
    m_uniforms.camera_pos = m_dust_shader->uniform_handle("u_camera_pos");
    m_uniforms.span = m_dust_shader->optional_uniform_handle("u_span");
  }

  if (m_blood_shader != nullptr) {
    m_blood_uniforms.mvp = m_blood_shader->uniform_handle("u_mvp");
    m_blood_uniforms.radius = m_blood_shader->optional_uniform_handle("u_radius");
    m_blood_uniforms.alpha_scale = m_blood_shader->uniform_handle("u_alpha_scale");
    m_blood_uniforms.rotation = m_blood_shader->uniform_handle("u_rotation");
    m_blood_uniforms.aspect_ratio = m_blood_shader->uniform_handle("u_aspect_ratio");
    m_blood_uniforms.seed = m_blood_shader->uniform_handle("u_seed");
  }
}

auto CombatDustPipeline::is_initialized() const -> bool {
  return m_dust_shader != nullptr && m_dust_mesh.vao != 0 &&
         m_dust_mesh.index_count > 0 && m_fireball_mesh.vao != 0 &&
         m_fireball_mesh.index_count > 0 && m_metal_spark_mesh.vao != 0 &&
         m_metal_spark_mesh.index_count > 0 && m_weapon_arc_mesh.vao != 0 &&
         m_weapon_arc_mesh.index_count > 0;
}

struct DustVertex {
  float position[3];
  float normal[3];
  float tex_coord[2];
};

namespace {

auto upload_dust_mesh(QOpenGLFunctions_3_3_Core& gl,
                      StaticMeshBuffers& mesh,
                      const char* label,
                      const std::vector<DustVertex>& vertices,
                      const std::vector<unsigned int>& indices) -> bool {
  return upload_static_effect_mesh(gl,
                                   mesh,
                                   label,
                                   vertices.data(),
                                   vertices.size(),
                                   sizeof(DustVertex),
                                   k_position_normal_texcoord_layout,
                                   indices);
}

} // namespace

auto CombatDustPipeline::create_dust_geometry() -> bool {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_dust_mesh);
  clear_gl_errors();

  std::vector<DustVertex> vertices;
  std::vector<unsigned int> indices;

  constexpr float pi = std::numbers::pi_v<float>;

  constexpr int height_levels = 8;
  constexpr int angle_segments = 12;
  constexpr float max_height = 1.0F;

  vertices.reserve(static_cast<size_t>((height_levels + 1) * (angle_segments + 1)));

  for (int h = 0; h <= height_levels; ++h) {
    float const height_t = static_cast<float>(h) / static_cast<float>(height_levels);
    float const y = height_t * max_height;

    float const radius_at_height = 1.0F - height_t * 0.3F;

    for (int a = 0; a <= angle_segments; ++a) {
      float const angle_t = static_cast<float>(a) / static_cast<float>(angle_segments);
      float const theta = angle_t * pi * 2.0F;
      float const x = radius_at_height * std::cos(theta);
      float const z = radius_at_height * std::sin(theta);

      DustVertex v{};
      v.position[0] = x;
      v.position[1] = y;
      v.position[2] = z;

      v.normal[0] = std::cos(theta);
      v.normal[1] = 0.3F;
      v.normal[2] = std::sin(theta);

      v.tex_coord[0] = angle_t;
      v.tex_coord[1] = height_t;
      vertices.push_back(v);
    }
  }

  indices.reserve(static_cast<size_t>(height_levels * angle_segments * 6));
  for (int h = 0; h < height_levels; ++h) {
    for (int a = 0; a < angle_segments; ++a) {
      auto const curr = static_cast<unsigned int>(h * (angle_segments + 1) + a);
      unsigned int const next = curr + static_cast<unsigned int>(angle_segments + 1);

      indices.push_back(curr);
      indices.push_back(next);
      indices.push_back(curr + 1);

      indices.push_back(curr + 1);
      indices.push_back(next);
      indices.push_back(next + 1);
    }
  }

  return upload_dust_mesh(*this, m_dust_mesh, "dust", vertices, indices);
}

auto CombatDustPipeline::create_fireball_geometry() -> bool {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_fireball_mesh);
  clear_gl_errors();

  std::vector<DustVertex> vertices;
  std::vector<unsigned int> indices;

  constexpr int latitude_segments = 12;
  constexpr int longitude_segments = 24;
  constexpr float pi = std::numbers::pi_v<float>;

  vertices.reserve(
      static_cast<size_t>((latitude_segments + 1) * (longitude_segments + 1)));
  indices.reserve(static_cast<size_t>(latitude_segments * longitude_segments * 6));

  for (int lat = 0; lat <= latitude_segments; ++lat) {
    float const v = static_cast<float>(lat) / static_cast<float>(latitude_segments);
    float const theta = v * pi;
    float const sin_theta = std::sin(theta);
    float const cos_theta = std::cos(theta);

    for (int lon = 0; lon <= longitude_segments; ++lon) {
      float const u = static_cast<float>(lon) / static_cast<float>(longitude_segments);
      float const phi = u * pi * 2.0F;
      float const sin_phi = std::sin(phi);
      float const cos_phi = std::cos(phi);

      DustVertex vertex{};
      vertex.position[0] = sin_theta * cos_phi;
      vertex.position[1] = cos_theta;
      vertex.position[2] = sin_theta * sin_phi;
      vertex.normal[0] = vertex.position[0];
      vertex.normal[1] = vertex.position[1];
      vertex.normal[2] = vertex.position[2];
      vertex.tex_coord[0] = u;
      vertex.tex_coord[1] = v;
      vertices.push_back(vertex);
    }
  }

  int const row = longitude_segments + 1;
  for (int lat = 0; lat < latitude_segments; ++lat) {
    for (int lon = 0; lon < longitude_segments; ++lon) {
      auto const a = static_cast<unsigned int>(lat * row + lon);
      unsigned int const b = a + 1U;
      unsigned int const c = a + static_cast<unsigned int>(row) + 1U;
      unsigned int const d = a + static_cast<unsigned int>(row);
      indices.push_back(a);
      indices.push_back(b);
      indices.push_back(c);
      indices.push_back(c);
      indices.push_back(d);
      indices.push_back(a);
    }
  }

  return upload_dust_mesh(*this, m_fireball_mesh, "fireball", vertices, indices);
}

auto CombatDustPipeline::create_metal_spark_geometry() -> bool {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_metal_spark_mesh);
  clear_gl_errors();

  std::vector<DustVertex> vertices;
  std::vector<unsigned int> indices;

  constexpr int ray_count = 10;
  constexpr float pi = std::numbers::pi_v<float>;
  vertices.reserve(static_cast<std::size_t>(ray_count * 4));
  indices.reserve(static_cast<std::size_t>(ray_count * 6));

  for (int ray = 0; ray < ray_count; ++ray) {
    float const seed =
        std::fmod(std::sin(static_cast<float>(ray + 1) * 78.233F) * 43758.5453F, 1.0F);
    float const unit_seed = seed < 0.0F ? seed + 1.0F : seed;
    float const angle =
        (static_cast<float>(ray) + unit_seed * 0.42F) * (2.0F * pi / ray_count);
    float const elevation = 0.10F + unit_seed * 0.58F;
    QVector3D direction(std::cos(angle) * std::cos(elevation),
                        std::sin(elevation),
                        std::sin(angle) * std::cos(elevation));
    direction.normalize();

    QVector3D side = QVector3D::crossProduct(direction, QVector3D(0.0F, 1.0F, 0.0F));
    if (side.lengthSquared() < 1.0e-5F) {
      side = QVector3D(1.0F, 0.0F, 0.0F);
    } else {
      side.normalize();
    }

    float const base_distance = 0.05F + unit_seed * 0.05F;
    float const tip_distance = 0.58F + unit_seed * 0.36F;
    float const base_width = 0.030F + unit_seed * 0.012F;
    float const tip_width = base_width * 0.28F;
    QVector3D const base = direction * base_distance;
    QVector3D const tip = direction * tip_distance;

    auto append_vertex =
        [&vertices, &direction](const QVector3D& position, float along, float across) {
          DustVertex vertex{};
          vertex.position[0] = position.x();
          vertex.position[1] = position.y();
          vertex.position[2] = position.z();
          vertex.normal[0] = direction.x();
          vertex.normal[1] = direction.y();
          vertex.normal[2] = direction.z();
          vertex.tex_coord[0] = along;
          vertex.tex_coord[1] = across;
          vertices.push_back(vertex);
        };

    unsigned int const first = static_cast<unsigned int>(vertices.size());
    append_vertex(base - side * base_width, 0.0F, 0.0F);
    append_vertex(base + side * base_width, 0.0F, 1.0F);
    append_vertex(tip - side * tip_width, 1.0F, 0.0F);
    append_vertex(tip + side * tip_width, 1.0F, 1.0F);
    indices.insert(indices.end(),
                   {first, first + 1U, first + 2U, first + 2U, first + 1U, first + 3U});
  }

  return upload_dust_mesh(*this, m_metal_spark_mesh, "metal spark", vertices, indices);
}

auto CombatDustPipeline::create_weapon_arc_geometry() -> bool {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_weapon_arc_mesh);
  clear_gl_errors();

  std::vector<DustVertex> vertices;
  std::vector<unsigned int> indices;

  constexpr int segment_count = 64;
  constexpr int band_count = 3;
  constexpr float pi = std::numbers::pi_v<float>;
  constexpr float inner_radius = 0.50F;
  vertices.reserve(static_cast<std::size_t>((segment_count + 1) * (band_count + 1)));
  indices.reserve(static_cast<std::size_t>(segment_count * band_count * 6));

  for (int segment = 0; segment <= segment_count; ++segment) {
    float const along = static_cast<float>(segment) / static_cast<float>(segment_count);
    float const angle = (along - 0.5F) * 2.0F * pi;
    float const sin_a = std::sin(angle);
    float const cos_a = std::cos(angle);
    for (int band = 0; band <= band_count; ++band) {
      float const across = static_cast<float>(band) / static_cast<float>(band_count);
      float const ring = inner_radius + (1.0F - inner_radius) * across;
      DustVertex vertex{};
      vertex.position[0] = sin_a * ring;
      vertex.position[1] = 0.0F;
      vertex.position[2] = cos_a * ring;
      vertex.normal[0] = 0.0F;
      vertex.normal[1] = 1.0F;
      vertex.normal[2] = 0.0F;
      vertex.tex_coord[0] = along;
      vertex.tex_coord[1] = across;
      vertices.push_back(vertex);
    }
  }

  auto const stride = static_cast<unsigned int>(band_count + 1);
  for (int segment = 0; segment < segment_count; ++segment) {
    for (int band = 0; band < band_count; ++band) {
      unsigned int const a =
          static_cast<unsigned int>(segment) * stride + static_cast<unsigned int>(band);
      unsigned int const b = a + stride;
      indices.insert(indices.end(), {a, b, a + 1U, a + 1U, b, b + 1U});
    }
  }

  return upload_dust_mesh(*this, m_weapon_arc_mesh, "weapon arc", vertices, indices);
}

auto CombatDustPipeline::create_blood_geometry() -> bool {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_blood_mesh);
  clear_gl_errors();

  std::vector<DustVertex> vertices;
  std::vector<unsigned int> indices;

  constexpr int segments = 16;
  constexpr float pi = std::numbers::pi_v<float>;

  vertices.reserve(static_cast<size_t>(segments + 1));
  indices.reserve(static_cast<size_t>(segments * 3));

  DustVertex center{};
  center.position[0] = 0.0F;
  center.position[1] = 0.0F;
  center.position[2] = 0.0F;
  center.normal[0] = 0.0F;
  center.normal[1] = 1.0F;
  center.normal[2] = 0.0F;
  center.tex_coord[0] = 0.5F;
  center.tex_coord[1] = 0.5F;
  vertices.push_back(center);

  for (int i = 0; i < segments; ++i) {
    float const angle =
        static_cast<float>(i) / static_cast<float>(segments) * pi * 2.0F;
    float const x = std::cos(angle);
    float const z = std::sin(angle);

    DustVertex vertex{};
    vertex.position[0] = x;
    vertex.position[1] = 0.0F;
    vertex.position[2] = z;
    vertex.normal[0] = 0.0F;
    vertex.normal[1] = 1.0F;
    vertex.normal[2] = 0.0F;
    vertex.tex_coord[0] = 0.5F + 0.5F * x;
    vertex.tex_coord[1] = 0.5F + 0.5F * z;
    vertices.push_back(vertex);
  }

  for (int i = 1; i <= segments; ++i) {
    auto const next = static_cast<unsigned int>((i % segments) + 1);
    indices.push_back(0);
    indices.push_back(static_cast<unsigned int>(i));
    indices.push_back(next);
  }

  return upload_dust_mesh(*this, m_blood_mesh, "blood", vertices, indices);
}

void CombatDustPipeline::render_dust_batch(const DustInstanceData* instances,
                                           std::size_t count,
                                           const QMatrix4x4& view_proj) {
  if (!is_initialized() || instances == nullptr || count == 0) {
    return;
  }

  EffectRenderState const state = capture_effect_render_state();

  m_dust_shader->use();
  GLuint current_vao = 0;
  GLsizei current_index_count = 0;
  enum class EffectStateMode : std::uint8_t {
    AlphaDepth,
    AlphaOverlay,
    Additive
  };
  EffectStateMode current_state = EffectStateMode::AlphaDepth;
  bool state_initialized = false;

  for (std::size_t idx = 0; idx < count; ++idx) {
    const DustInstanceData& inst = instances[idx];
    if (inst.intensity < k_min_dust_intensity) {
      continue;
    }

    bool const use_fireball_geometry = inst.effect_type == EffectType::Fireball;
    bool const use_weapon_arc_geometry = inst.effect_type == EffectType::WeaponArc;
    bool const use_additive_blending = use_fireball_geometry ||
                                       inst.effect_type == EffectType::MetalSpark ||
                                       use_weapon_arc_geometry;
    EffectStateMode const target_state =
        use_additive_blending ? EffectStateMode::Additive
                              : (inst.overlay ? EffectStateMode::AlphaOverlay
                                              : EffectStateMode::AlphaDepth);
    if (!state_initialized || current_state != target_state) {
      if (target_state == EffectStateMode::Additive) {
        apply_additive_effect_state();
      } else if (target_state == EffectStateMode::AlphaOverlay) {
        apply_overlay_alpha_effect_state();
      } else {
        apply_alpha_blend_effect_state();
      }
      current_state = target_state;
      state_initialized = true;
    }

    bool const use_metal_spark_geometry = inst.effect_type == EffectType::MetalSpark;
    StaticMeshBuffers const& target_mesh =
        use_fireball_geometry
            ? m_fireball_mesh
            : (use_metal_spark_geometry
                   ? m_metal_spark_mesh
                   : (use_weapon_arc_geometry ? m_weapon_arc_mesh : m_dust_mesh));
    GLuint const target_vao = target_mesh.vao;
    GLsizei const target_index_count = target_mesh.index_count;
    if (target_vao == 0 || target_index_count <= 0) {
      continue;
    }
    if (current_vao != target_vao) {
      glBindVertexArray(target_vao);
      current_vao = target_vao;
      current_index_count = target_index_count;
    }

    QMatrix4x4 model;
    if (use_metal_spark_geometry) {
      model = spark_model_matrix(inst.position, inst.radius, inst.direction);
    } else if (use_weapon_arc_geometry) {
      model = weapon_arc_model_matrix(
          inst.position, inst.radius, inst.direction, inst.tilt);
    } else {
      model.setToIdentity();
      model.translate(inst.position);
      model.scale(inst.radius);
    }

    QMatrix4x4 const mvp = view_proj * model;

    m_dust_shader->set_uniform(m_uniforms.mvp, mvp);
    m_dust_shader->set_uniform(m_uniforms.model, model);
    m_dust_shader->set_uniform(m_uniforms.time, inst.time);
    m_dust_shader->set_uniform(m_uniforms.center, inst.position);
    m_dust_shader->set_uniform(m_uniforms.radius, inst.radius);
    m_dust_shader->set_uniform(m_uniforms.intensity, inst.intensity);
    m_dust_shader->set_uniform(m_uniforms.dust_color, inst.color);
    m_dust_shader->set_uniform(m_uniforms.effect_type,
                               static_cast<int>(inst.effect_type));
    m_dust_shader->set_uniform(m_uniforms.camera_pos, m_view_position);
    if (m_uniforms.span != GL::Shader::InvalidUniform) {
      m_dust_shader->set_uniform(m_uniforms.span, inst.span);
    }

    glDrawElements(GL_TRIANGLES, current_index_count, GL_UNSIGNED_INT, nullptr);
  }

  glBindVertexArray(0);
  restore_effect_render_state(state);
}

void CombatDustPipeline::render_blood_pool_batch(const BloodPoolInstanceData* instances,
                                                 std::size_t count,
                                                 const QMatrix4x4& view_proj) {
  if (!is_initialized() || instances == nullptr || count == 0 ||
      m_blood_shader == nullptr || m_blood_mesh.vao == 0 ||
      m_blood_mesh.index_count <= 0) {
    return;
  }

  GLboolean const cull_enabled = glIsEnabled(GL_CULL_FACE);
  GLboolean const depth_test_enabled = glIsEnabled(GL_DEPTH_TEST);
  GLboolean const blend_enabled = glIsEnabled(GL_BLEND);
  GLboolean const polygon_offset_enabled = glIsEnabled(GL_POLYGON_OFFSET_FILL);
  GLboolean depth_mask_enabled = GL_TRUE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask_enabled);

  GLfloat polygon_offset_factor = 0.0F;
  GLfloat polygon_offset_units = 0.0F;
  glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &polygon_offset_factor);
  glGetFloatv(GL_POLYGON_OFFSET_UNITS, &polygon_offset_units);

  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(-1.0F, -1.0F);

  m_blood_shader->use();
  glBindVertexArray(m_blood_mesh.vao);

  for (std::size_t idx = 0; idx < count; ++idx) {
    BloodPoolInstanceData const& blood = instances[idx];
    if (blood.alpha_scale <= 0.0F || blood.radius <= 0.0F) {
      continue;
    }

    QMatrix4x4 model;
    model.setToIdentity();
    model.translate(blood.position);
    model.scale(blood.radius, 1.0F, blood.radius);

    QMatrix4x4 const mvp = view_proj * model;
    m_blood_shader->set_uniform(m_blood_uniforms.mvp, mvp);
    m_blood_shader->set_uniform(m_blood_uniforms.radius, blood.radius);
    m_blood_shader->set_uniform(m_blood_uniforms.alpha_scale, blood.alpha_scale);
    m_blood_shader->set_uniform(m_blood_uniforms.rotation, blood.rotation);
    m_blood_shader->set_uniform(m_blood_uniforms.aspect_ratio, blood.aspect_ratio);
    m_blood_shader->set_uniform(m_blood_uniforms.seed, blood.seed);

    glDrawElements(GL_TRIANGLES, m_blood_mesh.index_count, GL_UNSIGNED_INT, nullptr);
  }

  glBindVertexArray(0);

  glPolygonOffset(polygon_offset_factor, polygon_offset_units);
  if (polygon_offset_enabled == 0U) {
    glDisable(GL_POLYGON_OFFSET_FILL);
  }
  glDepthMask(depth_mask_enabled);
  if (blend_enabled == 0U) {
    glDisable(GL_BLEND);
  }
  if (depth_test_enabled != 0U) {
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }
  if (cull_enabled != 0U) {
    glEnable(GL_CULL_FACE);
  }
}

} // namespace Render::GL::BackendPipelines
