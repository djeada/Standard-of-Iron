#include "minimap_image_provider.h"

MinimapImageProvider::MinimapImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage MinimapImageProvider::requestImage(const QString &id, QSize *size,
                                          const QSize &requested_size) {
  Q_UNUSED(id);

  if (m_minimap_image.isNull()) {

    QImage placeholder(64, 64, QImage::Format_RGBA8888);
    placeholder.fill(QColor(15, 26, 34));
    if (size) {
      *size = placeholder.size();
    }
    return placeholder;
  }

  if (size) {
    *size = m_minimap_image.size();
  }

  if (requested_size.isValid() && !requested_size.isEmpty()) {
    return m_minimap_image.scaled(requested_size, Qt::KeepAspectRatio,
                                  Qt::SmoothTransformation);
  }

  return m_minimap_image;
}

void MinimapImageProvider::set_minimap_image(const QImage &image) {
  m_minimap_image = image;
}
