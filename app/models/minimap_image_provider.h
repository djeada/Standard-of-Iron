#pragma once

#include <QImage>
#include <QQuickImageProvider>

class MinimapImageProvider : public QQuickImageProvider {
public:
  MinimapImageProvider();

  QImage requestImage(const QString &id, QSize *size,
                      const QSize &requested_size) override;

  void set_minimap_image(const QImage &image);

private:
  QImage m_minimap_image;
};
