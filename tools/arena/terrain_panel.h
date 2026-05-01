#pragma once

#include <QString>
#include <QWidget>

class TerrainPanel : public QWidget {
  Q_OBJECT

public:
  explicit TerrainPanel(QWidget *parent = nullptr);

signals:
  void seed_changed(int seed);
  void height_scale_changed(float value);
  void octaves_changed(int value);
  void frequency_changed(float value);
  void regenerate_requested();
  void wireframe_toggled(bool enabled);
  void normals_toggled(bool enabled);
  void ground_type_changed(const QString &groundType);
  void rain_toggled(bool enabled);
  void rain_intensity_changed(float intensity);
};
