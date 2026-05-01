#pragma once

#include <QString>
#include <QWidget>

class QComboBox;
class QLabel;
class QSpinBox;

class BuildingPanel : public QWidget {
  Q_OBJECT

public:
  explicit BuildingPanel(QWidget *parent = nullptr);

  void set_selection_summary(const QString &summary);
  [[nodiscard]] auto selected_owner_id() const -> int;
  [[nodiscard]] auto selected_nation_id() const -> QString;
  [[nodiscard]] auto selected_building_type_id() const -> QString;

signals:
  void spawn_buildings_requested(int count);
  void clear_buildings_requested();
  void building_owner_selected(int ownerId);
  void building_nation_selected(const QString &nationId);
  void building_type_selected(const QString &buildingType);

private:
  void populate_nation_options();

  QComboBox *m_owner_box = nullptr;
  QComboBox *m_nation_box = nullptr;
  QComboBox *m_building_box = nullptr;
  QSpinBox *m_spawn_count_box = nullptr;
  QLabel *m_selection_summary_label = nullptr;
};
