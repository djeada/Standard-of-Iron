#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace App::ViewModels {

class WaveViewModel : public QObject {
  Q_OBJECT

  Q_PROPERTY(bool active READ active NOTIFY status_changed)
  Q_PROPERTY(int total_phases READ total_phases NOTIFY status_changed)
  Q_PROPERTY(int cleared_phases READ cleared_phases NOTIFY status_changed)
  Q_PROPERTY(int current_phase READ current_phase NOTIFY status_changed)
  Q_PROPERTY(qreal seconds_until_next READ seconds_until_next NOTIFY status_changed)
  Q_PROPERTY(bool warning READ warning NOTIFY status_changed)
  Q_PROPERTY(int live_enemies READ live_enemies NOTIFY status_changed)
  Q_PROPERTY(QString state READ state NOTIFY status_changed)
  Q_PROPERTY(QVariantList alerts READ alerts NOTIFY status_changed)

public:
  explicit WaveViewModel(QObject* parent = nullptr);

  void set_status(const QVariantMap& status);
  void clear();

  [[nodiscard]] auto active() const -> bool;
  [[nodiscard]] auto total_phases() const -> int;
  [[nodiscard]] auto cleared_phases() const -> int;
  [[nodiscard]] auto current_phase() const -> int;
  [[nodiscard]] auto seconds_until_next() const -> qreal;
  [[nodiscard]] auto warning() const -> bool;
  [[nodiscard]] auto live_enemies() const -> int;
  [[nodiscard]] auto state() const -> QString;
  [[nodiscard]] auto alerts() const -> QVariantList;

signals:
  void status_changed();

private:
  QVariantMap m_status;
};

} // namespace App::ViewModels
