#pragma once

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QPointF>
#include <QString>

class QPainter;

namespace MapEditor {

struct WorldBackdrop {
  QImage image;
  int grid_width = 0;
  int grid_height = 0;
  float pixels_per_cell = 0.0F;
  float night = 0.0F;
  QString error;

  [[nodiscard]] auto ok() const -> bool { return !image.isNull(); }
};

struct WorldBackdropOptions {
  int max_dimension_px = 2048;
  float min_pixels_per_cell = 2.0F;
  float max_pixels_per_cell = 8.0F;
};

[[nodiscard]] auto
render_world_backdrop(const QByteArray& map_json,
                      const WorldBackdropOptions& options = {}) -> WorldBackdrop;

enum class NaturalSprite {
  None,
  Pine,
  Olive,
  Cypress,
  Palm,
  DeadTree,
  Boulder,
  IronOre,
  Plant,
};

[[nodiscard]] auto natural_sprite_for(const QString& prop_type) -> NaturalSprite;

void paint_natural_sprite(QPainter& painter,
                          NaturalSprite sprite,
                          const QPointF& centre,
                          float cell_px,
                          float scale,
                          unsigned seed);

void paint_ground_shadow(QPainter& painter, const QPointF& centre, float radius_px);

void paint_cloud_shadows(QPainter& painter,
                         const QRectF& map_rect,
                         float cell_px,
                         double seconds,
                         float strength);

void paint_world_surround(QPainter& painter,
                          const QRectF& canvas_rect,
                          const QRectF& map_rect);

} // namespace MapEditor
