#include "minimap_image_provider.h"

#include <QMutexLocker>
MinimapImageProvider::MinimapImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage MinimapImageProvider::requestImage(const QString &id, QSize *size,
                                          const QSize &requested_size) {
  Q_UNUSED(id);

  QImage image_copy;
  {
    QMutexLocker locker(&m_mutex);
    image_copy = m_minimap_image;
  }

  if (image_copy.isNull()) {

    QImage placeholder(64, 64, QImage::Format_RGBA8888);
    placeholder.fill(QColor(15, 26, 34));
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

void MinimapImageProvider::set_minimap_image(const QImage &image) {
  QMutexLocker locker(&m_mutex);
  m_minimap_image = image;
}
