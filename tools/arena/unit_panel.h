#pragma once

#include <QGroupBox>
#include <QString>

class QCheckBox;
class QComboBox;

class UnitPanel : public QGroupBox {
  Q_OBJECT

public:
  explicit UnitPanel(QWidget *parent = nullptr);

  void setAnimationPaused(bool paused);
  [[nodiscard]] auto selectedNationId() const -> QString;
  [[nodiscard]] auto selectedUnitTypeId() const -> QString;

signals:
  void spawnUnitRequested();
  void clearUnitsRequested();
  void nationSelected(const QString &nationId);
  void unitTypeSelected(const QString &unitType);
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
  QComboBox *m_nation_box = nullptr;
  QComboBox *m_unit_box = nullptr;
};
