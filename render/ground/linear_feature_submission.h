#pragma once

#include "../../game/map/visibility_service.h"
#include "../gl/mesh.h"
#include "../scene_renderer.h"
#include "../terrain_scene_types.h"
#include "linear_feature_visibility.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cstddef>
#include <memory>
#include <vector>

namespace Render::Ground {

template <typename Segment>
void submit_linear_feature_segments(
    GL::Renderer &renderer, const std::vector<Segment> &segments,
    const std::vector<std::unique_ptr<GL::Mesh>> &meshes,
    GL::LinearFeatureKind kind, const QVector3D &base_color,
    const LinearFeatureVisibilityOptions &visibility_options = {}) {
  if (segments.empty() || meshes.empty()) {
    return;
  }

  auto &visibility = Game::Map::VisibilityService::instance();
  const bool use_visibility = visibility.is_initialized();
  Game::Map::VisibilityService::Snapshot visibility_snapshot;
  if (use_visibility) {
    visibility_snapshot = visibility.snapshot();
  }

  QMatrix4x4 model;
  model.setToIdentity();

  std::size_t mesh_index = 0;
  for (const auto &segment : segments) {
    if (mesh_index >= meshes.size()) {
      break;
    }

    auto *mesh = meshes[mesh_index].get();
    ++mesh_index;
    if (mesh == nullptr) {
      continue;
    }

    float alpha = 1.0F;
    QVector3D color_multiplier(1.0F, 1.0F, 1.0F);

    if (use_visibility) {
      const auto visibility_result = evaluate_linear_feature_visibility(
          &visibility_snapshot, segment.start, segment.end, visibility_options);
      if (!visibility_result.visible) {
        continue;
      }
      alpha = visibility_result.alpha;
      color_multiplier = visibility_result.color_multiplier;
    }

    GL::TerrainFeatureCmd cmd;
    cmd.mesh = mesh;
    cmd.kind = kind;
    cmd.model = model;
    cmd.color = {base_color.x() * color_multiplier.x(),
                 base_color.y() * color_multiplier.y(),
                 base_color.z() * color_multiplier.z()};
    cmd.alpha = alpha;
    renderer.terrain_feature(cmd);
  }
}

} // namespace Render::Ground
