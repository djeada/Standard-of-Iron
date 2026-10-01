#include "software_backend.h"

#include <variant>

#include "draw_part.h"
#include "draw_queue.h"
#include "scene/camera.h"
#include "static_building_batch.h"

namespace Render::GL {

namespace {

void submit_as_cube(Render::Software::SoftwareRasterizer& r,
                    const QMatrix4x4& world,
                    const QVector3D& color,
                    float alpha) {

  QMatrix4x4 proxy = world;
  proxy.scale(0.5F);
  r.submit_cube(proxy, color, alpha);
}

} // namespace

void SoftwareBackend::execute(const DrawQueue& queue, const Camera& cam) {
  QMatrix4x4 const vp = cam.get_view_projection_matrix();
  m_rasterizer.set_view_projection(vp);

  auto submit_item = [&](const DrawCmd& item) {
    switch (static_cast<DrawCmdType>(item.index())) {
    case DrawCmdType::Mesh: {
      auto const& c = std::get<MeshCmd>(item);
      submit_as_cube(m_rasterizer, c.model, c.color, c.alpha);
      break;
    }
    case DrawCmdType::DrawPart: {
      auto const& c = std::get<DrawPartCmd>(item);
      submit_as_cube(m_rasterizer, c.world, c.color, c.alpha);
      break;
    }
    case DrawCmdType::RiggedCreature: {

      auto const& c = std::get<RiggedCreatureCmd>(item);
      submit_as_cube(m_rasterizer, c.world, c.color, c.alpha);
      break;
    }
    default:

      break;
    }
  };

  if (const StaticBuildingBatch* batch = queue.static_batch(); batch != nullptr) {
    for (const StaticBatchDraw& draw : batch->draws()) {
      for (std::uint32_t i = draw.first; i < draw.first + draw.count; ++i) {
        const BuildingInstanceGpu& record = batch->instances()[i];
        QMatrix4x4 model(record.model_col0[0],
                         record.model_col1[0],
                         record.model_col2[0],
                         record.model_col0[3],
                         record.model_col0[1],
                         record.model_col1[1],
                         record.model_col2[1],
                         record.model_col1[3],
                         record.model_col0[2],
                         record.model_col1[2],
                         record.model_col2[2],
                         record.model_col2[3],
                         0.0F,
                         0.0F,
                         0.0F,
                         1.0F);
        model.translate(draw.mesh->bounds_center);
        model.scale(draw.mesh->bounds_radius);
        const QVector3D color =
            record.palette0[3] >= 0.0F
                ? QVector3D(record.palette0[0], record.palette0[1], record.palette0[2])
                : QVector3D(0.6F, 0.55F, 0.5F);
        submit_as_cube(m_rasterizer, model, color, 1.0F);
      }
    }
  }

  if (!queue.prepared_batches().empty()) {
    for (const PreparedBatch& batch : queue.prepared_batches()) {
      for (std::size_t i = batch.start; i < batch.end(); ++i) {
        submit_item(queue.get_sorted(i));
      }
    }
  } else {
    for (auto const& item : queue.items()) {
      submit_item(item);
    }
  }

  if (m_rasterizer.settings().width <= 0 || m_rasterizer.settings().height <= 0) {
    m_image = QImage();
    return;
  }
  m_image = m_rasterizer.render();
}

} // namespace Render::GL
