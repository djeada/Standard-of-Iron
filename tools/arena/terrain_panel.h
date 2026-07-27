#pragma once

#include <QString>
#include <QWidget>

class TerrainPanel : public QWidget {
  Q_OBJECT

public:
  explicit TerrainPanel(QWidget* parent = nullptr);

signals:
  void seed_changed(int seed);
  void height_scale_changed(float value);
  void octaves_changed(int value);
  void frequency_changed(float value);
  void regenerate_requested();
  void wireframe_toggled(bool enabled);
  void normals_toggled(bool enabled);
  void ground_type_changed(const QString& ground_type);
  void rain_toggled(bool enabled);
  void rain_intensity_changed(float intensity);
  void environment_time_changed(float hour);
  void lighting_profile_changed(const QString& profile);
  void time_mode_changed(const QString& mode);
  void day_length_changed(float seconds);
  void shadow_quality_changed(const QString& quality);
};
