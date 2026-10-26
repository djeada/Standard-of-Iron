#include <QMatrix4x4>
#include <QOpenGLContext>
#include <QtGlobal>
#include <QtGui/qopengl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include "game/map/terrain_service.h"
#include "game/map/visibility_service.h"
#include "render/gl/gl_resource_tracking.h"
#include "render/gl/shader.h"
#include "render/gl/texture.h"
#include "render/graphics_settings.h"
#include "render/ground/ground_ecology.h"
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
constexpr std::size_t k_terrain_field_channels = 4U;
constexpr std::size_t k_cover_channels = 4U;

constexpr int k_microdetail_size_full = 1024;
constexpr int k_microdetail_size_low = 512;
constexpr int k_max_atlas_size_full = 4096;
constexpr int k_max_atlas_size_low = 2048;

auto low_graphics_tier() -> bool {
  return Render::GraphicsSettings::instance().quality() == Render::GraphicsQuality::Low;
}
constexpr float k_microdetail_cells = 32.0F;

} // namespace

void TerrainRenderer::bake_cover_mask() {
  const auto count =
      static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
  if (m_ecology == nullptr || m_ecology->width != m_width ||
      m_ecology->height != m_height) {
    const auto* terrain = world().terrain();
    const auto* height_map = terrain != nullptr ? terrain->get_height_map() : nullptr;
    if (height_map != nullptr && height_map->get_width() == m_width &&
        height_map->get_height() == m_height) {
      m_ecology =
          Render::Ground::shared_ground_ecology(terrain, *height_map, m_noise_seed);
    } else {
      m_ecology = std::make_shared<const Render::Ground::GroundEcologyField>(
          Render::Ground::build_ground_ecology(
              m_height_data,
              m_terrain_types,
              m_width,
              m_height,
              m_tile_size,
              Render::Ground::ecology_seed(m_noise_seed),
              nullptr));
    }
  }
  m_cover_data.assign(count * k_cover_channels, 0.0F);
  auto copy_layer = [&](const std::vector<float>& layer, std::size_t channel) {
    if (layer.size() < count) {
      return;
    }
    for (std::size_t index = 0; index < count; ++index) {
      m_cover_data[index * k_cover_channels + channel] = layer[index];
    }
  };
  copy_layer(m_ecology->forest, 0U);
  copy_layer(m_ecology->road_proximity, 1U);
  copy_layer(m_ecology->margin, 2U);
  copy_layer(m_ecology->talus, 3U);
  stamp_building_ground();
}

void TerrainRenderer::stamp_building_ground() {
  using Render::Ground::ecology_detail::step_between;
  if (m_building_footprints.empty() || m_width <= 0 || m_height <= 0) {
    return;
  }
  const float tile = std::max(m_tile_size, 1e-4F);
  const float half_extent_x = static_cast<float>(m_width) * 0.5F - 0.5F;
  const float half_extent_z = static_cast<float>(m_height) * 0.5F - 0.5F;
  constexpr float k_verge_scale = 0.36F;
  constexpr float k_wall_foot = 0.9F;
  constexpr float k_path_reach = 36.0F;
  constexpr float k_path_value = 1.25F;
  constexpr float k_yard_encoding = 2.0F;
  constexpr float k_deg_to_rad = 0.017453292F;

  auto texel = [&](int x, int z, std::size_t channel) -> float& {
    return m_cover_data[(static_cast<std::size_t>(z) *
                             static_cast<std::size_t>(m_width) +
                         static_cast<std::size_t>(x)) *
                            k_cover_channels +
                        channel];
  };
  auto for_cells =
      [&](float min_wx, float max_wx, float min_wz, float max_wz, auto&& visit) {
        int const x0 =
            std::max(0, static_cast<int>(std::floor(min_wx / tile + half_extent_x)));
        int const x1 = std::min(
            m_width - 1, static_cast<int>(std::ceil(max_wx / tile + half_extent_x)));
        int const z0 =
            std::max(0, static_cast<int>(std::floor(min_wz / tile + half_extent_z)));
        int const z1 = std::min(
            m_height - 1, static_cast<int>(std::ceil(max_wz / tile + half_extent_z)));
        for (int z = z0; z <= z1; ++z) {
          float const wz = (static_cast<float>(z) - half_extent_z) * tile;
          for (int x = x0; x <= x1; ++x) {
            visit(x, z, (static_cast<float>(x) - half_extent_x) * tile, wz);
          }
        }
      };

  const std::vector<Game::Map::RoadSegment>* roads = nullptr;
  if (const auto* terrain = world().terrain(); terrain != nullptr) {
    roads = &terrain->road_segments();
  }

  for (const auto& footprint : m_building_footprints) {
    float const parcel = Render::Ground::hash_to_01(
        Render::Ground::hash_coords(static_cast<int>(std::lround(footprint.x * 4.0F)),
                                    static_cast<int>(std::lround(footprint.z * 4.0F)),
                                    0x2C1B3C6DU));
    float const reach = 1.5F + 1.7F * parcel;
    float const yaw = footprint.yaw_degrees * k_deg_to_rad;
    float const c = std::cos(yaw);
    float const s = std::sin(yaw);
    float const outer = reach / k_verge_scale;
    float const radius = std::sqrt(footprint.half_width * footprint.half_width +
                                   footprint.half_depth * footprint.half_depth) +
                         outer;
    for_cells(footprint.x - radius,
              footprint.x + radius,
              footprint.z - radius,
              footprint.z + radius,
              [&](int x, int z, float wx, float wz) {
                float const dx = wx - footprint.x;
                float const dz = wz - footprint.z;
                float const local_x = c * dx - s * dz;
                float const local_z = s * dx + c * dz;
                float const ox =
                    std::max(std::abs(local_x) - footprint.half_width, 0.0F);
                float const oz =
                    std::max(std::abs(local_z) - footprint.half_depth, 0.0F);
                float const distance = std::sqrt(ox * ox + oz * oz);
                float const trodden =
                    std::clamp(1.0F - k_verge_scale * distance / reach, 0.0F, 1.0F);
                float& verge = texel(x, z, 1U);
                verge = std::max(verge, trodden * k_yard_encoding);
                float const foot = 1.0F - step_between(0.0F, k_wall_foot, distance);
                if (foot > 0.0F) {
                  float& apron = texel(x, z, 3U);
                  apron = std::max(apron, 1.0F + foot);
                }
              });

    if (roads == nullptr || roads->empty()) {
      continue;
    }
    float best = k_path_reach;
    float target_x = 0.0F;
    float target_z = 0.0F;
    for (const auto& road : *roads) {
      float const ax = road.start.x();
      float const az = road.start.z();
      float const rx = road.end.x() - ax;
      float const rz = road.end.z() - az;
      float const length_sq = rx * rx + rz * rz;
      float t = 0.0F;
      if (length_sq > 1e-6F) {
        t = std::clamp(((footprint.x - ax) * rx + (footprint.z - az) * rz) / length_sq,
                       0.0F,
                       1.0F);
      }
      float const px = ax + rx * t;
      float const pz = az + rz * t;
      float const gap =
          std::hypot(px - footprint.x, pz - footprint.z) - road.width * 0.5F;
      if (gap < best) {
        best = gap;
        target_x = px;
        target_z = pz;
      }
    }
    float const clearance = std::max(footprint.half_width, footprint.half_depth);
    if (best >= k_path_reach || best <= clearance) {
      continue;
    }
    float const path_half = 0.55F + 0.35F * parcel;
    float const sx = footprint.x;
    float const sz = footprint.z;
    float const ex = target_x - sx;
    float const ez = target_z - sz;
    float const path_length_sq = std::max(ex * ex + ez * ez, 1e-6F);
    for_cells(std::min(sx, target_x) - path_half - 1.0F,
              std::max(sx, target_x) + path_half + 1.0F,
              std::min(sz, target_z) - path_half - 1.0F,
              std::max(sz, target_z) + path_half + 1.0F,
              [&](int x, int z, float wx, float wz) {
                float const t = std::clamp(
                    ((wx - sx) * ex + (wz - sz) * ez) / path_length_sq, 0.0F, 1.0F);
                float const off = std::hypot(sx + ex * t - wx, sz + ez * t - wz);
                float const value =
                    k_path_value *
                    (1.0F - step_between(path_half * 0.5F, path_half, off));
                float& verge = texel(x, z, 1U);
                verge = std::max(verge, value);
              });
  }
}

void TerrainRenderer::bake_terrain_fields() {
  const std::size_t texel_count =
      static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
  m_terrain_field_data.assign(texel_count * k_terrain_field_channels, 0.0F);

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

      const float slope_dx = (right - left) / (2.0F * span);
      const float slope_dz = (up - down) / (2.0F * span);

      const std::size_t slot =
          (static_cast<std::size_t>(z) * static_cast<std::size_t>(m_width)) +
          static_cast<std::size_t>(x);
      const std::size_t base = slot * k_terrain_field_channels;
      m_terrain_field_data[base] = openness;
      m_terrain_field_data[base + 1U] = curvature;
      m_terrain_field_data[base + 2U] = -slope_dx;
      m_terrain_field_data[base + 3U] = -slope_dz;
    }
  }
}

void TerrainRenderer::bake_terrain_microdetail() {
  if (m_microdetail_texture != 0U) {
    return;
  }
  auto* context = QOpenGLContext::currentContext();
  auto* gl = context != nullptr ? context->functions() : nullptr;
  if (gl == nullptr) {
    return;
  }

  if (m_microdetail_bake_shader == nullptr) {
    m_microdetail_bake_shader = std::make_unique<Shader>();
    if (!m_microdetail_bake_shader->load_from_files(
            QStringLiteral(":/assets/shaders/post_fullscreen.vert"),
            QStringLiteral(":/assets/shaders/terrain_microdetail_bake.frag"))) {
      qWarning() << "TerrainRenderer: microdetail bake shader failed to compile;"
                    " falling back to procedural noise";
      m_microdetail_bake_shader.reset();
      return;
    }
    m_microdetail_bake_shader->set_debug_name(
        QStringLiteral("terrain_microdetail_bake"));
  }

  const int microdetail_size =
      low_graphics_tier() ? k_microdetail_size_low : k_microdetail_size_full;
  GLuint texture = 0U;
  gl->glGenTextures(1, &texture);
  note_textures_created(1);
  gl->glBindTexture(GL_TEXTURE_2D, texture);
  gl->glTexImage2D(GL_TEXTURE_2D,
                   0,
                   GL_RGBA16F,
                   microdetail_size,
                   microdetail_size,
                   0,
                   GL_RGBA,
                   GL_FLOAT,
                   nullptr);
  note_texture_storage(
      texture_transfer_bytes(static_cast<std::size_t>(microdetail_size),
                             static_cast<std::size_t>(microdetail_size),
                             8U),
      false);
  gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  gl->glBindTexture(GL_TEXTURE_2D, 0);

  GLuint fbo = 0U;
  gl->glGenFramebuffers(1, &fbo);
  if (m_noise_atlas_vao == 0U) {
    context->extraFunctions()->glGenVertexArrays(1, &m_noise_atlas_vao);
    note_vertex_arrays_created(1);
  }

  GLint previous_fbo = 0;
  GLint previous_viewport[4]{};
  gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_fbo);
  gl->glGetIntegerv(GL_VIEWPORT, previous_viewport);
  const GLboolean previous_depth = gl->glIsEnabled(GL_DEPTH_TEST);
  const GLboolean previous_blend = gl->glIsEnabled(GL_BLEND);
  const GLboolean previous_cull = gl->glIsEnabled(GL_CULL_FACE);

  gl->glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  gl->glFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  if (gl->glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    qWarning() << "TerrainRenderer: microdetail bake framebuffer incomplete";
    gl->glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_fbo));
    gl->glDeleteFramebuffers(1, &fbo);
    gl->glDeleteTextures(1, &texture);
    return;
  }

  gl->glViewport(0, 0, microdetail_size, microdetail_size);
  gl->glDisable(GL_DEPTH_TEST);
  gl->glDisable(GL_BLEND);
  gl->glDisable(GL_CULL_FACE);

  m_microdetail_bake_shader->use();
  m_microdetail_bake_shader->set_uniform("u_microdetail_cells", k_microdetail_cells);
  context->extraFunctions()->glBindVertexArray(m_noise_atlas_vao);
  gl->glDrawArrays(GL_TRIANGLES, 0, 3);
  context->extraFunctions()->glBindVertexArray(0);
  m_microdetail_bake_shader->release();

  gl->glBindTexture(GL_TEXTURE_2D, texture);
  gl->glGenerateMipmap(GL_TEXTURE_2D);
  gl->glBindTexture(GL_TEXTURE_2D, 0);

  gl->glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_fbo));
  gl->glDeleteFramebuffers(1, &fbo);
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

  m_microdetail_texture = texture;
}

void TerrainRenderer::bake_terrain_noise_atlas() {
  auto* context = QOpenGLContext::currentContext();
  auto* gl = context != nullptr ? context->functions() : nullptr;
  if (gl == nullptr || m_width <= 0 || m_height <= 0) {
    return;
  }

  constexpr int k_texels_per_tile = 2;
  const int max_atlas_size =
      low_graphics_tier() ? k_max_atlas_size_low : k_max_atlas_size_full;
  const int requested = std::max(m_width, m_height) * k_texels_per_tile;
  const int atlas_size = std::clamp(requested, 256, max_atlas_size);

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

  if (m_noise_atlas_texture == 0U || m_noise_atlas_detail_texture == 0U ||
      m_noise_atlas_size != atlas_size) {
    if (m_noise_atlas_texture != 0U) {
      gl->glDeleteTextures(1, &m_noise_atlas_texture);
      m_noise_atlas_texture = 0U;
    }
    if (m_noise_atlas_detail_texture != 0U) {
      gl->glDeleteTextures(1, &m_noise_atlas_detail_texture);
      m_noise_atlas_detail_texture = 0U;
    }
    const auto allocate_atlas = [&](unsigned int& target) {
      gl->glGenTextures(1, &target);
      note_textures_created(1);
      gl->glBindTexture(GL_TEXTURE_2D, target);
      gl->glTexImage2D(GL_TEXTURE_2D,
                       0,
                       GL_RGBA16F,
                       atlas_size,
                       atlas_size,
                       0,
                       GL_RGBA,
                       GL_FLOAT,
                       nullptr);
      note_texture_storage(texture_transfer_bytes(static_cast<std::size_t>(atlas_size),
                                                  static_cast<std::size_t>(atlas_size),
                                                  8U),
                           false);
      gl->glTexParameteri(
          GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      gl->glBindTexture(GL_TEXTURE_2D, 0);
    };
    allocate_atlas(m_noise_atlas_texture);
    allocate_atlas(m_noise_atlas_detail_texture);
    m_noise_atlas_size = atlas_size;
  }

  if (m_noise_atlas_fbo == 0U) {
    gl->glGenFramebuffers(1, &m_noise_atlas_fbo);
  }
  if (m_noise_atlas_vao == 0U) {
    context->extraFunctions()->glGenVertexArrays(1, &m_noise_atlas_vao);
    note_vertex_arrays_created(1);
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
  gl->glFramebufferTexture2D(GL_FRAMEBUFFER,
                             GL_COLOR_ATTACHMENT1,
                             GL_TEXTURE_2D,
                             m_noise_atlas_detail_texture,
                             0);
  const std::array<GLenum, 2> bake_targets{GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
  context->extraFunctions()->glDrawBuffers(static_cast<GLsizei>(bake_targets.size()),
                                           bake_targets.data());
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
  const QVector2D bake_world_size(static_cast<float>(m_width) * m_tile_size,
                                  static_cast<float>(m_height) * m_tile_size);
  m_noise_bake_shader->set_uniform("u_bake_world_min", bake_world_size * -0.5F);
  m_noise_bake_shader->set_uniform("u_bake_world_size", bake_world_size);
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
  gl->glBindTexture(GL_TEXTURE_2D, m_noise_atlas_detail_texture);
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

auto TerrainRenderer::update_height_texture(
    const Render::Ground::BuildingGroundSet& building_ground)
    -> TerrainSurfaceCmd::HeightResources {
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
    note_texture_transfer(texture_transfer_bytes(
        static_cast<std::size_t>(m_width), static_cast<std::size_t>(m_height), 4U));
    m_height_texture_dirty = false;
  }

  const bool field_size_changed = m_terrain_field_texture == nullptr ||
                                  m_terrain_field_texture->get_width() != m_width ||
                                  m_terrain_field_texture->get_height() != m_height;
  if (field_size_changed) {
    m_terrain_field_texture = std::make_unique<Texture>();
    if (!m_terrain_field_texture->create_empty(
            m_width, m_height, Texture::Format::RGBA16F)) {
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
                        GL_RGBA,
                        GL_FLOAT,
                        m_terrain_field_data.data());
    note_texture_transfer(texture_transfer_bytes(
        static_cast<std::size_t>(m_width), static_cast<std::size_t>(m_height), 16U));
    m_terrain_fields_dirty = false;
  }

  const bool cover_size_changed = m_cover_texture == nullptr ||
                                  m_cover_texture->get_width() != m_width ||
                                  m_cover_texture->get_height() != m_height;
  if (cover_size_changed) {
    m_cover_texture = std::make_unique<Texture>();
    if (!m_cover_texture->create_empty(m_width, m_height, Texture::Format::RGBA16F)) {
      m_cover_texture.reset();
    } else {
      m_cover_texture->set_filter(Texture::Filter::Linear, Texture::Filter::Linear);
      m_cover_texture->set_wrap(Texture::Wrap::ClampToEdge, Texture::Wrap::ClampToEdge);
      m_cover_dirty = true;
    }
  }
  if (building_ground.revision != m_building_ground_revision) {
    m_building_ground_revision = building_ground.revision;
    m_building_footprints = building_ground.footprints;
    m_cover_dirty = true;
  }
  if (m_cover_texture != nullptr && m_cover_dirty) {
    bake_cover_mask();
    m_cover_texture->bind();
    gl->glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        m_width,
                        m_height,
                        GL_RGBA,
                        GL_FLOAT,
                        m_cover_data.data());
    note_texture_transfer(texture_transfer_bytes(
        static_cast<std::size_t>(m_width), static_cast<std::size_t>(m_height), 16U));
    m_cover_dirty = false;
  }

  bake_terrain_microdetail();
  if (m_noise_atlas_dirty) {
    bake_terrain_noise_atlas();
  }

  resources.texture = m_height_texture.get();
  resources.field_texture = m_terrain_field_texture.get();
  resources.cover_texture = m_cover_texture.get();
  resources.noise_atlas = m_noise_atlas_texture;
  resources.noise_atlas_detail = m_noise_atlas_detail_texture;
  resources.microdetail = m_microdetail_texture;
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

  TerrainSurfaceCmd::VisibilityResources visibility_resources;
  const TerrainSurfaceCmd::HeightResources height_resources =
      update_height_texture(renderer.building_ground());
  renderer.set_terrain_height_resources(height_resources);
  renderer.set_ground_fog(m_ground_fog);
  if (renderer.static_world_visibility_filter_enabled()) {

    visibility_resources = renderer.visibility_mask();
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

    constexpr float k_inverse_sqrt3 = 0.57735027F;
    TerrainSurfaceCmd cmd;
    cmd.mesh = chunk.mesh.get();
    cmd.model = k_identity_matrix;
    const float half_extent = chunk.cull_radius * k_inverse_sqrt3;
    cmd.aabb.min = chunk.cull_center - QVector3D(half_extent, half_extent, half_extent);
    cmd.aabb.max = chunk.cull_center + QVector3D(half_extent, half_extent, half_extent);
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
