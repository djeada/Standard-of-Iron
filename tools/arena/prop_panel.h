#pragma once

#include <QString>
#include <QWidget>

class QButtonGroup;
class QDoubleSpinBox;

class PropPanel : public QWidget {
  Q_OBJECT

public:
  explicit PropPanel(QWidget* parent = nullptr);

  [[nodiscard]] auto selected_prop_type_id() const -> QString;
  [[nodiscard]] auto selected_scale() const -> float;
  [[nodiscard]] auto selected_rotation_degrees() const -> float;
  [[nodiscard]] auto selected_fire_camp_intensity() const -> float;
  [[nodiscard]] auto selected_fire_camp_radius() const -> float;

signals:
  void world_prop_type_selected(const QString& prop_type);
  void world_prop_scale_changed(float scale);
  void world_prop_rotation_degrees_changed(float rotation_degrees);
  void fire_camp_intensity_changed(float intensity);
  void fire_camp_radius_changed(float radius);
  void spawn_world_prop_requested();
  void clear_world_props_requested();
  void clear_world_props_of_type_requested();

private:
  void update_control_visibility();

  QButtonGroup* m_prop_group = nullptr;
  QDoubleSpinBox* m_scale_box = nullptr;
  QDoubleSpinBox* m_rotation_box = nullptr;
  QDoubleSpinBox* m_fire_camp_intensity_box = nullptr;
  QDoubleSpinBox* m_fire_camp_radius_box = nullptr;
};
