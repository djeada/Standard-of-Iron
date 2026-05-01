#pragma once

#include <QGroupBox>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QSpinBox;

class UnitPanel : public QGroupBox {
  Q_OBJECT

public:
  explicit UnitPanel(QWidget *parent = nullptr);

  void setAnimationPaused(bool paused);
  void setSelectionSummary(const QString &summary);
  [[nodiscard]] auto selectedOwnerId() const -> int;
  [[nodiscard]] auto selectedNationId() const -> QString;
  [[nodiscard]] auto selectedUnitTypeId() const -> QString;

signals:
  void spawnUnitsRequested(int count);
  void clearUnitsRequested();
  void spawnOwnerSelected(int ownerId);
  void nationSelected(const QString &nationId);
  void unitTypeSelected(const QString &unitType);
  void spawnIndividualsPerUnitChanged(int count);
  void spawnRiderVisibilityChanged(bool visible);
  void applyVisualOverridesRequested();
  void spawnOpposingBatchRequested(int count);
  void spawnMirrorMatchRequested(int count);
  void resetArenaRequested();
  void animationSelected(const QString &animationName);
  void playAnimationRequested();
  void animationPausedToggled(bool paused);
  void moveSelectedUnitRequested();
  void movementSpeedChanged(float speed);
  void skeletonDebugToggled(bool enabled);

private:
  void populateNationOptions();
  void populateUnitOptions(const QString &nationId,
                           const QString &preferredUnitType = QString());

  QCheckBox *m_pause_checkbox = nullptr;
  QComboBox *m_owner_box = nullptr;
  QComboBox *m_nation_box = nullptr;
  QComboBox *m_unit_box = nullptr;
  QSpinBox *m_spawn_count_box = nullptr;
  QSpinBox *m_individuals_per_unit_box = nullptr;
  QCheckBox *m_render_rider_checkbox = nullptr;
  QLabel *m_selection_summary_label = nullptr;
};
