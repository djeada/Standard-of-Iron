#include <QMatrix4x4>
#include <QOpenGLContext>
#include <QtGlobal>
#include <QtGui/qopengl.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "game/map/render_visibility_rules.h"
#include "game/map/visibility_service.h"
#include "render/gl/shader.h"
#include "render/gl/texture.h"
#include "render/scene_renderer.h"
#include "terrain_renderer.h"

namespace {

const QMatrix4x4 k_identity_matrix;

}

namespace Render::GL {

namespace {

constexpr int k_openness_directions = 6;
constexpr int k_openness_rings = 3;
constexpr float k_openness_radii[k_openness_rings] = {1.5F, 4.0F, 9.0F};
constexpr float k_openness_dir_x[k_openness_directions] = {
    1.0F, 0.5F, -0.5F, -1.0F, -0.5F, 0.5F};
constexpr float k_openness_dir_z[k_openness_directions] = {
    0.0F, 0.866F, 0.866F, 0.0F, -0.866F, -0.866F};

} // namespace

void TerrainRenderer::bake_terrain_fields() {
  const std::size_t texel_count =
      static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
  m_terrain_field_data.assign(texel_count * 2U, 0.0F);

  const float world_per_texel = std::max(m_tile_size, 1e-6F);

  auto height_at = [this](int x, int z) -> float {
    const int cx = std::clamp(x, 0, m_width - 1);
    const int cz = std::clamp(z, 0, m_height - 1);
    return m_height_data[static_cast<std::size_t>(cz) *
                             static_cast<std::size_t>(m_width) +
                         static_cast<std::size_t>(cx)];
  };

  auto height_bilinear = [&](float x, float z) -> float {
    const float fx = std::floor(x);
    const float fz = std::floor(z);
    const auto ix = static_cast<int>(fx);
    const auto iz = static_cast<int>(fz);
    const float tx = x - fx;
    const float tz = z - fz;
    const float h00 = height_at(ix, iz);
    const float h10 = height_at(ix + 1, iz);
    const float h01 = height_at(ix, iz + 1);
    const float h11 = height_at(ix + 1, iz + 1);
    return (h00 * (1.0F - tx) + h10 * tx) * (1.0F - tz) +
           (h01 * (1.0F - tx) + h11 * tx) * tz;
  };

  for (int z = 0; z < m_height; ++z) {
    for (int x = 0; x < m_width; ++x) {
      const float center = height_at(x, z);

      const float left = height_at(x - 2, z);
      const float right = height_at(x + 2, z);
      const float down = height_at(x, z - 2);
      const float up = height_at(x, z + 2);
      const float span = 2.0F * world_per_texel;
      const float curve_x =
          (left + right - 2.0F * center) / std::max(span * span, 1e-5F);
      const float curve_z = (down + up - 2.0F * center) / std::max(span * span, 1e-5F);
      const float curvature = 0.5F * (curve_x + curve_z);

      float horizon_sum = 0.0F;
      for (int direction = 0; direction < k_openness_directions; ++direction) {
        float highest_tangent = 0.0F;
        for (int ring = 0; ring < k_openness_rings; ++ring) {
          const float step_x = k_openness_dir_x[direction] * k_openness_radii[ring];
          const float step_z = k_openness_dir_z[direction] * k_openness_radii[ring];
          const float reach = std::max(
              std::sqrt(step_x * step_x + step_z * step_z) * world_per_texel, 1e-3F);
          const float sampled = height_bilinear(static_cast<float>(x) + step_x,
                                                static_cast<float>(z) + step_z);
          highest_tangent = std::max(highest_tangent, (sampled - center) / reach);
        }
        horizon_sum +=
            highest_tangent / std::sqrt(highest_tangent * highest_tangent + 1.0F);
      }
      const float openness = std::clamp(
          1.0F - horizon_sum / static_cast<float>(k_openness_directions), 0.0F, 1.0F);

      const std::size_t slot =
          (static_cast<std::size_t>(z) * static_cast<std::size_t>(m_width)) +
          static_cast<std::size_t>(x);
      m_terrain_field_data[slot * 2U] = openness;
      m_terrain_field_data[(slot * 2U) + 1U] = curvature;
    }
  }
}

void TerrainRenderer::bake_terrain_noise_atlas() {
  auto* context = QOpenGLContext::currentContext();
  auto* gl = context != nullptr ? context->functions() : nullptr;
  if (gl == nullptr || m_width <= 0 || m_height <= 0) {
    return;
  }

  constexpr int k_texels_per_tile = 2;
  constexpr int k_max_atlas_size = 4096;
  const int requested = std::max(m_width, m_height) * k_texels_per_tile;
  const int atlas_size = std::clamp(requested, 256, k_max_atlas_size);

  if (m_noise_bake_shader == nullptr) {
    m_noise_bake_shader = std::make_unique<Shader>();
    if (!m_noise_bake_shader->load_from_files(
            QStringLiteral(":/assets/shaders/post_fullscreen.vert"),
            QStringLiteral(":/assets/shaders/terrain_field_bake.frag"))) {
      qWarning() << "TerrainRenderer: terrain field bake shader failed to compile;"
                    " falling back to procedural noise";
      m_noise_bake_shader.reset();
      m_noise_atlas_dirty = false;
      return;
    }
    m_noise_bake_shader->set_debug_name(QStringLiteral("terrain_field_bake"));
  }

  if (m_noise_atlas_texture == 0U || m_noise_atlas_size != atlas_size) {
    if (m_noise_atlas_texture != 0U) {
      gl->glDeleteTextures(1, &m_noise_atlas_texture);
      m_noise_atlas_texture = 0U;
    }
    gl->glGenTextures(1, &m_noise_atlas_texture);
    gl->glBindTexture(GL_TEXTURE_2D, m_noise_atlas_texture);
    gl->glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA16F,
                     atlas_size,
                     atlas_size,
                     0,
                     GL_RGBA,
                     GL_FLOAT,
                     nullptr);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->glBindTexture(GL_TEXTURE_2D, 0);
    m_noise_atlas_size = atlas_size;
  }

  if (m_noise_atlas_fbo == 0U) {
    gl->glGenFramebuffers(1, &m_noise_atlas_fbo);
  }
  if (m_noise_atlas_vao == 0U) {
    context->extraFunctions()->glGenVertexArrays(1, &m_noise_atlas_vao);
  }

  GLint previous_fbo = 0;
  GLint previous_viewport[4]{};
  gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_fbo);
  gl->glGetIntegerv(GL_VIEWPORT, previous_viewport);
  const GLboolean previous_depth = gl->glIsEnabled(GL_DEPTH_TEST);
  const GLboolean previous_blend = gl->glIsEnabled(GL_BLEND);
  const GLboolean previous_cull = gl->glIsEnabled(GL_CULL_FACE);

  gl->glBindFramebuffer(GL_FRAMEBUFFER, m_noise_atlas_fbo);
  gl->glFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_noise_atlas_texture, 0);
  const GLenum status = gl->glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    qWarning() << "TerrainRenderer: terrain field bake framebuffer incomplete"
               << Qt::hex << status;
    gl->glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_fbo));
    m_noise_atlas_dirty = false;
    return;
  }

  gl->glViewport(0, 0, atlas_size, atlas_size);
  gl->glDisable(GL_DEPTH_TEST);
  gl->glDisable(GL_BLEND);
  gl->glDisable(GL_CULL_FACE);

  const QVector2D noise_offset =
      m_chunks.empty() ? QVector2D(0.0F, 0.0F) : m_chunks.front().params.noise_offset;

  m_noise_bake_shader->use();
  m_noise_bake_shader->set_uniform("u_bake_world_min", QVector2D(0.0F, 0.0F));
  m_noise_bake_shader->set_uniform(
      "u_bake_world_size",
      QVector2D(static_cast<float>(m_width) * m_tile_size,
                static_cast<float>(m_height) * m_tile_size));
  m_noise_bake_shader->set_uniform("u_noise_offset", noise_offset);
  m_noise_bake_shader->set_uniform("u_tile_size", m_tile_size);
  m_noise_bake_shader->set_uniform("u_macro_noise_scale",
                                   m_biome_settings.terrain_macro_noise_scale);

  context->extraFunctions()->glBindVertexArray(m_noise_atlas_vao);
  gl->glDrawArrays(GL_TRIANGLES, 0, 3);
  context->extraFunctions()->glBindVertexArray(0);
  m_noise_bake_shader->release();

  gl->glBindTexture(GL_TEXTURE_2D, m_noise_atlas_texture);
  gl->glGenerateMipmap(GL_TEXTURE_2D);
  gl->glBindTexture(GL_TEXTURE_2D, 0);

  gl->glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_fbo));
  gl->glViewport(previous_viewport[0],
                 previous_viewport[1],
                 previous_viewport[2],
                 previous_viewport[3]);
  if (previous_depth != 0U) {
    gl->glEnable(GL_DEPTH_TEST);
  }
  if (previous_blend != 0U) {
    gl->glEnable(GL_BLEND);
  }
  if (previous_cull != 0U) {
    gl->glEnable(GL_CULL_FACE);
  }

  m_noise_atlas_dirty = false;
  qInfo() << "TerrainRenderer: baked terrain noise atlas" << atlas_size << "squared";
}

auto TerrainRenderer::update_height_texture() -> TerrainSurfaceCmd::HeightResources {
  TerrainSurfaceCmd::HeightResources resources;
  if (m_width <= 0 || m_height <= 0 || m_height_data.empty()) {
    return resources;
  }

  auto* context = QOpenGLContext::currentContext();
  auto* gl = context != nullptr ? context->functions() : nullptr;
  if (gl == nullptr) {
    return resources;
  }

  const bool size_changed = m_height_texture == nullptr ||
                            m_height_texture->get_width() != m_width ||
                            m_height_texture->get_height() != m_height;
  if (size_changed) {
    m_height_texture = std::make_unique<Texture>();
    if (!m_height_texture->create_empty(m_width, m_height, Texture::Format::R32F)) {
      m_height_texture.reset();
      return resources;
    }
    m_height_texture->set_filter(Texture::Filter::Linear, Texture::Filter::Linear);
    m_height_texture->set_wrap(Texture::Wrap::ClampToEdge, Texture::Wrap::ClampToEdge);
    m_height_texture_dirty = true;
  }

  if (m_height_texture_dirty) {
    m_height_texture->bind();
    gl->glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        m_width,
                        m_height,
                        GL_RED,
                        GL_FLOAT,
                        m_height_data.data());
    m_height_texture_dirty = false;
  }

  const bool field_size_changed = m_terrain_field_texture == nullptr ||
                                  m_terrain_field_texture->get_width() != m_width ||
                                  m_terrain_field_texture->get_height() != m_height;
  if (field_size_changed) {
    m_terrain_field_texture = std::make_unique<Texture>();
    if (!m_terrain_field_texture->create_empty(
            m_width, m_height, Texture::Format::RG16F)) {
      m_terrain_field_texture.reset();
    } else {
      m_terrain_field_texture->set_filter(Texture::Filter::Linear,
                                          Texture::Filter::Linear);
      m_terrain_field_texture->set_wrap(Texture::Wrap::ClampToEdge,
                                        Texture::Wrap::ClampToEdge);
      m_terrain_fields_dirty = true;
    }
  }

  if (m_terrain_field_texture != nullptr && m_terrain_fields_dirty) {
    bake_terrain_fields();
    m_terrain_field_texture->bind();
    gl->glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        m_width,
                        m_height,
                        GL_RG,
                        GL_FLOAT,
                        m_terrain_field_data.data());
    m_terrain_fields_dirty = false;
  }

  static const bool noise_bake_allowed =
      qEnvironmentVariableIntValue("SOI_TERRAIN_NOISE_BAKE") != 0 ||
      !qEnvironmentVariableIsSet("SOI_TERRAIN_NOISE_BAKE");
  if (m_noise_atlas_dirty && noise_bake_allowed) {
    bake_terrain_noise_atlas();
  }

  resources.texture = m_height_texture.get();
  resources.field_texture = m_terrain_field_texture.get();
  resources.noise_atlas = m_noise_atlas_texture;
  resources.noise_atlas_world_size =
      QVector2D(static_cast<float>(m_width) * m_tile_size,
                static_cast<float>(m_height) * m_tile_size);
  resources.texel_size = QVector2D(1.0F / static_cast<float>(m_width),
                                   1.0F / static_cast<float>(m_height));
  resources.uv_scale = QVector2D(1.0F / (std::max(1, m_width) * m_tile_size),
                                 1.0F / (std::max(1, m_height) * m_tile_size));
  resources.uv_offset = QVector2D(0.5F, 0.5F);
  resources.to_world = 1.0F;
  resources.enabled = true;
  return resources;
}

void TerrainRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  if (m_chunks.empty()) {
    return;
  }

  Q_UNUSED(resources);

  const auto* visibility_snapshot = renderer.static_world_visibility_filter_enabled()
                                        ? renderer.submission_visibility().snapshot()
                                        : nullptr;
  TerrainSurfaceCmd::VisibilityResources visibility_resources;
  const TerrainSurfaceCmd::HeightResources height_resources = update_height_texture();
  renderer.set_terrain_height_resources(height_resources);
  renderer.set_ground_fog(m_ground_fog);
  if (visibility_snapshot != nullptr) {
    visibility_resources = renderer.visibility_mask();
    if (m_chunk_visibility_cache.size() != m_chunks.size()) {
      m_chunk_visibility_cache.assign(m_chunks.size(), {});
    }
  }

  for (std::size_t chunk_index = 0; chunk_index < m_chunks.size(); ++chunk_index) {
    const auto& chunk = m_chunks[chunk_index];
    if (!chunk.mesh) {
      continue;
    }

    constexpr float k_chunk_enter_margin = 2.0F;
    constexpr float k_chunk_keep_margin = 24.0F;
    if (m_chunk_visibility_cache.size() != m_chunks.size()) {
      m_chunk_visibility_cache.assign(m_chunks.size(), {});
    }
    auto& submission_cache = m_chunk_visibility_cache[chunk_index];
    const float cull_margin =
        submission_cache.was_submitted ? k_chunk_keep_margin : k_chunk_enter_margin;
    if (!renderer.submission_visibility().accepts_sphere(chunk.cull_center,
                                                         chunk.cull_radius +
                                                             cull_margin,
                                                         SubmissionFogMode::Ignore)) {
      submission_cache.was_submitted = false;
      continue;
    }
    submission_cache.was_submitted = true;

    if (visibility_snapshot != nullptr) {
      auto& cache = m_chunk_visibility_cache[chunk_index];
      if (cache.visibility_version != visibility_snapshot->version) {
        bool any_revealed = false;
        for (int gz = chunk.min_z; gz <= chunk.max_z && !any_revealed; ++gz) {
          for (int gx = chunk.min_x; gx <= chunk.max_x; ++gx) {
            if (Game::Map::classify_static_world_cell_visibility(
                    *visibility_snapshot, gx, gz) !=
                Game::Map::RenderVisibilityState::Hidden) {
              any_revealed = true;
              break;
            }
          }
        }
        cache.any_revealed = any_revealed;
        cache.visibility_version = visibility_snapshot->version;
      }
      if (!cache.any_revealed) {
        continue;
      }
    }

    TerrainSurfaceCmd cmd;
    cmd.mesh = chunk.mesh.get();
    cmd.model = k_identity_matrix;
    cmd.params = chunk.params;
    cmd.height = height_resources;
    cmd.visibility = visibility_resources;
    cmd.params.light_direction = m_light_direction;
    cmd.sort_key = 0x0080U;
    cmd.depth_write = true;
    cmd.wireframe = m_wireframe;
    cmd.depth_bias = 0.0F;
    renderer.terrain_surface(cmd);
  }
}

} // namespace Render::GL
