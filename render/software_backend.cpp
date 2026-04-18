#include "software_backend.h"

#include "draw_part.h"
#include "draw_queue.h"
#include "gl/camera.h"

#include <variant>

namespace Render::GL {

namespace {

void submit_as_cube(Render::Software::SoftwareRasterizer &r,
                    const QMatrix4x4 &world, const QVector3D &color,
                    float alpha) {
  // The software tier treats every mesh as a coloured cube occupying the
  // mesh's model-space unit volume. Combined with the caller's `world`
  // transform (scale/rotate/translate) this reproduces the silhouette of
  // the actual mesh closely enough for the no-GPU tier.
  QMatrix4x4 proxy = world;
  proxy.scale(0.5F);
  r.submit_cube(proxy, color, alpha);
}

} // namespace

void SoftwareBackend::execute(const DrawQueue &queue, const Camera &cam) {
  QMatrix4x4 const vp =
      cam.get_projection_matrix() * cam.get_view_matrix();
  m_rasterizer.set_view_projection(vp);

  for (auto const &item : queue.items()) {
    switch (static_cast<DrawCmdType>(item.index())) {
    case DrawCmdType::Mesh: {
      auto const &c = std::get<MeshCmd>(item);
      submit_as_cube(m_rasterizer, c.model, c.color, c.alpha);
      break;
    }
    case DrawCmdType::DrawPart: {
      auto const &c = std::get<DrawPartCmd>(item);
      submit_as_cube(m_rasterizer, c.world, c.color, c.alpha);
      break;
    }
    case DrawCmdType::RiggedCreature: {
      // Stage 16: software tier draws rigged creatures as their
      // unit-space cube proxy. Bone palette is GPU-only (UBO);
      // the silhouette via cmd.world is enough for the no-GPU tier.
      auto const &c = std::get<RiggedCreatureCmd>(item);
      submit_as_cube(m_rasterizer, c.world, c.color, c.alpha);
      break;
    }
    default:
      // Particles, batched vegetation, effects and UI helpers are
      // intentionally skipped at this tier.
      break;
    }
  }

  if (m_rasterizer.settings().width <= 0 ||
      m_rasterizer.settings().height <= 0) {
    m_image = QImage();
    return;
  }
  m_image = m_rasterizer.render();
}

} // namespace Render::GL
