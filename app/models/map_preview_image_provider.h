#pragma once

#include <QImage>
#include <QMap>
#include <QMutex>
#include <QQuickImageProvider>
#include <QString>

class MapPreviewImageProvider : public QQuickImageProvider {
public:
  MapPreviewImageProvider();

  QImage requestImage(const QString &id, QSize *size,
                      const QSize &requested_size) override;

  void set_preview_image(const QString &map_id, const QImage &image);
  void clear_preview(const QString &map_id);

private:
  QMap<QString, QImage> m_preview_images;
  QMutex m_mutex;
};
