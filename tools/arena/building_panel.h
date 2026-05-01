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

  void setSelectionSummary(const QString &summary);
  [[nodiscard]] auto selectedOwnerId() const -> int;
  [[nodiscard]] auto selectedNationId() const -> QString;
  [[nodiscard]] auto selectedBuildingTypeId() const -> QString;

signals:
  void spawnBuildingsRequested(int count);
  void clearBuildingsRequested();
  void buildingOwnerSelected(int ownerId);
  void buildingNationSelected(const QString &nationId);
  void buildingTypeSelected(const QString &buildingType);

private:
  void populateNationOptions();

  QComboBox *m_owner_box = nullptr;
  QComboBox *m_nation_box = nullptr;
  QComboBox *m_building_box = nullptr;
  QSpinBox *m_spawn_count_box = nullptr;
  QLabel *m_selection_summary_label = nullptr;
};
