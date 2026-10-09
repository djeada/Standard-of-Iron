#include "campaign_map_film.h"

#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

#include "../utils/resource_utils.h"

namespace CampaignMapFilm {

namespace {

constexpr float k_edge_margin = 0.03F;

} // namespace

auto TerrainHeightField::load_default() -> bool {
  return load(QStringLiteral(":/assets/campaign_map/terrain_height.png"),
              QStringLiteral(":/assets/campaign_map/terrain_height.json"));
}

auto TerrainHeightField::load(const QString& image_resource,
                              const QString& meta_resource) -> bool {
  m_ready = false;
  QImage image(Utils::Resources::resolve_resource_path(image_resource));
  if (image.isNull()) {
    return false;
  }
  if (image.format() != QImage::Format_Grayscale16) {
    image = image.convertToFormat(QImage::Format_Grayscale16);
  }
  if (image.isNull() || image.width() <= 1 || image.height() <= 1) {
    return false;
  }

  float min_m = -6000.0F;
  float max_m = 4000.0F;
  QFile meta(Utils::Resources::resolve_resource_path(meta_resource));
  if (meta.open(QIODevice::ReadOnly)) {
    const QJsonObject obj = QJsonDocument::fromJson(meta.readAll()).object();
    if (obj.contains(QStringLiteral("min_m"))) {
      min_m = static_cast<float>(obj.value(QStringLiteral("min_m")).toDouble());
    }
    if (obj.contains(QStringLiteral("max_m"))) {
      max_m = static_cast<float>(obj.value(QStringLiteral("max_m")).toDouble());
    }
  }
  if (!(min_m < max_m)) {
    return false;
  }
  const float max_abs = std::max(std::abs(min_m), std::abs(max_m));
  if (max_abs <= 0.0F) {
    return false;
  }

  m_width = image.width();
  m_height = image.height();
  m_samples.assign(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height),
                   0.0F);
  const float range = max_m - min_m;
  float land_max = 0.0F;
  for (int y = 0; y < m_height; ++y) {
    const auto* row = reinterpret_cast<const quint16*>(image.constScanLine(y));
    for (int x = 0; x < m_width; ++x) {
      const float norm = static_cast<float>(row[x]) / 65535.0F;
      const float value = (min_m + norm * range) / max_abs;
      m_samples[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) +
                static_cast<std::size_t>(x)] = value;
      land_max = std::max(land_max, value);
    }
  }
  m_land_scale = land_max > 1e-6F ? 1.0F / land_max : 1.0F;
  m_ready = true;
  return true;
}

auto TerrainHeightField::raw_at(float u, float v) const -> float {
  const float cu = std::clamp(u, 0.0F, 1.0F);
  const float cv = std::clamp(v, 0.0F, 1.0F);
  const float x = cu * static_cast<float>(m_width - 1);
  const float y = (1.0F - cv) * static_cast<float>(m_height - 1);
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const int x1 = std::min(x0 + 1, m_width - 1);
  const int y1 = std::min(y0 + 1, m_height - 1);
  const float fx = x - static_cast<float>(x0);
  const float fy = y - static_cast<float>(y0);
  auto at = [this](int px, int py) {
    return m_samples[static_cast<std::size_t>(py) * static_cast<std::size_t>(m_width) +
                     static_cast<std::size_t>(px)];
  };
  const float top = at(x0, y0) + (at(x1, y0) - at(x0, y0)) * fx;
  const float bottom = at(x0, y1) + (at(x1, y1) - at(x0, y1)) * fx;
  return top + (bottom - top) * fy;
}

auto TerrainHeightField::height_at(float u, float v) const -> float {
  if (!m_ready) {
    return 0.0F;
  }
  const float raw = raw_at(u, v);
  float height = raw >= 0.0F ? raw * m_land_scale : 0.0F;
  const float edge = std::min(std::min(u, 1.0F - u), std::min(v, 1.0F - v));
  if (edge < k_edge_margin) {
    height *= std::max(0.0F, edge / k_edge_margin);
  }
  return height;
}

} // namespace CampaignMapFilm
