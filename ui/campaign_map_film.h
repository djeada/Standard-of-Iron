#pragma once

#include <QString>
#include <QVector2D>

#include <vector>

namespace CampaignMapFilm {

class TerrainHeightField {
public:
  auto load(const QString& image_resource, const QString& meta_resource) -> bool;
  auto load_default() -> bool;

  [[nodiscard]] auto ready() const -> bool { return m_ready; }
  [[nodiscard]] auto height_at(float u, float v) const -> float;
  [[nodiscard]] auto height_at(const QVector2D& uv) const -> float {
    return height_at(uv.x(), uv.y());
  }
  [[nodiscard]] auto smoothed_height_at(const QVector2D& uv, float radius) const -> float;
  [[nodiscard]] auto is_land(const QVector2D& uv) const -> bool;

private:
  [[nodiscard]] auto raw_at(float u, float v) const -> float;

  std::vector<float> m_samples;
  int m_width = 0;
  int m_height = 0;
  float m_land_scale = 1.0F;
  bool m_ready = false;
};

} // namespace CampaignMapFilm
