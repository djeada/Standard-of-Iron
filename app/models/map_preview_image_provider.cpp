#include "map_preview_image_provider.h"

#include <QMutexLocker>

MapPreviewImageProvider::MapPreviewImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage MapPreviewImageProvider::requestImage(const QString &id, QSize *size,
                                             const QSize &requested_size) {
  QImage image_copy;
  {
    QMutexLocker locker(&m_mutex);
    if (m_preview_images.contains(id)) {
      image_copy = m_preview_images[id];
    }
  }

  if (image_copy.isNull()) {
    QImage placeholder(200, 200, QImage::Format_RGBA8888);
    placeholder.fill(QColor(40, 40, 40));
    if (size) {
      *size = placeholder.size();
    }
    return placeholder;
  }

  if (size) {
    *size = image_copy.size();
  }

  if (requested_size.isValid() && !requested_size.isEmpty()) {
    return image_copy.scaled(requested_size, Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
  }

  return image_copy;
}

void MapPreviewImageProvider::set_preview_image(const QString &map_id,
                                                const QImage &image) {
  QMutexLocker locker(&m_mutex);
  m_preview_images[map_id] = image;
}

void MapPreviewImageProvider::clear_preview(const QString &map_id) {
  QMutexLocker locker(&m_mutex);
  m_preview_images.remove(map_id);
}
