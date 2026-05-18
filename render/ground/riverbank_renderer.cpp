#include "riverbank_renderer.h"

#include <QVector2D>
#include <QVector3D>
#include <qglobal.h>
#include <qmatrix4x4.h>
#include <qvectornd.h>

#include <GL/gl.h>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "../../game/map/render_visibility_rules.h"
#include "../../game/map/visibility_service.h"
#include "../gl/mesh.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "linear_feature_geometry.h"
#include "map/terrain.h"

namespace Render::GL {

RiverbankRenderer::RiverbankRenderer() = default;
RiverbankRenderer::~RiverbankRenderer() = default;

void RiverbankRenderer::configure(
    const std::vector<Game::Map::RiverSegment>& river_segments,
    const Game::Map::TerrainHeightMap& height_map) {
  m_river_segments = river_segments;
  m_tile_size = height_map.get_tile_size();
  m_visibility_texture.reset();
  m_cached_visibility_version = 0;
  m_visibility_width = 0;
  m_visibility_height = 0;
  build_meshes(height_map);
}

void RiverbankRenderer::build_meshes(const Game::Map::TerrainHeightMap& height_map) {
  m_meshes.clear();
  m_visibility_samples.clear();

  if (m_river_segments.empty()) {
    return;
  }

  for (const auto& segment : m_river_segments) {
    auto mesh_result = Ground::build_riverbank_mesh(segment, height_map);
    m_meshes.push_back(std::move(mesh_result.mesh));
    m_visibility_samples.push_back(std::move(mesh_result.visibility_samples));
  }
}

void RiverbankRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  if (m_meshes.empty() || m_river_segments.empty()) {
    return;
  }

  Q_UNUSED(resources);

  auto* backend = renderer.backend();
  auto& visibility = Game::Map::VisibilityService::instance();
  const bool use_visibility =
      renderer.static_world_visibility_filter_enabled() && visibility.is_initialized();
  auto visibility_snapshot = use_visibility ? visibility.snapshot_ptr() : nullptr;

  Texture* visibility_tex = nullptr;
  QVector2D visibility_size(0.0F, 0.0F);

  if (visibility_snapshot != nullptr) {
    const int vis_w = visibility_snapshot->width;
    const int vis_h = visibility_snapshot->height;
    bool const size_changed =
        (vis_w != m_visibility_width) || (vis_h != m_visibility_height);

    if (!m_visibility_texture || size_changed) {
      m_visibility_texture = std::make_unique<Texture>();
      m_visibility_texture->create_empty(vis_w, vis_h, Texture::Format::RGBA);
      m_visibility_texture->set_filter(Texture::Filter::Nearest,
                                       Texture::Filter::Nearest);
      m_visibility_texture->set_wrap(Texture::Wrap::ClampToEdge,
                                     Texture::Wrap::ClampToEdge);
      m_visibility_width = vis_w;
      m_visibility_height = vis_h;
      m_cached_visibility_version = 0;
    }

    if (visibility_snapshot->version != m_cached_visibility_version || size_changed) {
      const auto& cells = visibility_snapshot->cells;
      std::vector<unsigned char> texels(static_cast<std::size_t>(vis_w * vis_h * 4),
                                        0U);

      for (int z = 0; z < vis_h; ++z) {
        for (int x = 0; x < vis_w; ++x) {
          int const idx = z * vis_w + x;
          unsigned char val = 0U;
          switch (static_cast<Game::Map::VisibilityState>(cells[idx])) {
          case Game::Map::VisibilityState::Visible:
            val = 255U;
            break;
          case Game::Map::VisibilityState::Explored:
            val = 128U;
            break;
          case Game::Map::VisibilityState::Unseen:
          default:
            val = 0U;
            break;
          }
          texels[static_cast<std::size_t>(idx * 4)] = val;
          texels[static_cast<std::size_t>(idx * 4 + 3)] = 255;
        }
      }

      m_visibility_texture->bind();
      glTexSubImage2D(GL_TEXTURE_2D,
                      0,
                      0,
                      0,
                      vis_w,
                      vis_h,
                      GL_RGBA,
                      GL_UNSIGNED_BYTE,
                      texels.data());
      visibility_tex = m_visibility_texture.get();
      m_cached_visibility_version = visibility_snapshot->version;
    } else {
      visibility_tex = m_visibility_texture.get();
    }

    visibility_size = QVector2D(static_cast<float>(vis_w), static_cast<float>(vis_h));
  }

  QMatrix4x4 model;
  model.setToIdentity();

  TerrainSurfaceCmd::VisibilityResources visibility_resources;
  visibility_resources.texture = visibility_tex;
  visibility_resources.size = visibility_size;
  visibility_resources.tile_size = m_tile_size;
  visibility_resources.explored_alpha = m_explored_dim_factor;
  visibility_resources.enabled = use_visibility && visibility_tex != nullptr;

  size_t mesh_index = 0;
  for (const auto& segment : m_river_segments) {
    if (mesh_index >= m_meshes.size()) {
      break;
    }

    auto* mesh = m_meshes[mesh_index].get();
    ++mesh_index;

    if (mesh == nullptr) {
      continue;
    }

    float segment_visibility = 1.0F;
    if (visibility_snapshot != nullptr) {
      const auto& samples = m_visibility_samples[mesh_index - 1];
      if (samples.empty()) {
        segment_visibility = 1.0F;
      } else {
        auto state = Game::Map::RenderVisibilityState::Hidden;
        for (const auto& sample : samples) {
          const auto sample_state =
              Game::Map::classify_world_visibility(*visibility_snapshot,
                                                   sample.x(),
                                                   sample.z());
          if (sample_state == Game::Map::RenderVisibilityState::Visible) {
            state = Game::Map::RenderVisibilityState::Visible;
            break;
          }
          if (sample_state == Game::Map::RenderVisibilityState::Explored) {
            state = Game::Map::RenderVisibilityState::Explored;
          }
        }

        if (state == Game::Map::RenderVisibilityState::Hidden) {
          continue;
        }
        segment_visibility = state == Game::Map::RenderVisibilityState::Visible
                                 ? 1.0F
                                 : m_explored_dim_factor;
      }
    }

    TerrainFeatureCmd cmd;
    cmd.mesh = mesh;
    cmd.kind = LinearFeatureKind::Riverbank;
    cmd.model = model;
    cmd.color = QVector3D(1.0F, 1.0F, 1.0F);
    cmd.alpha = segment_visibility;
    cmd.visibility = visibility_resources;
    renderer.terrain_feature(cmd);
  }
}

} // namespace Render::GL
