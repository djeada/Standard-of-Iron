#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

namespace App::Audio {

class AudioStatusHud : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString overlay_text READ overlay_text NOTIFY overlay_changed)
  Q_PROPERTY(bool enabled READ enabled WRITE set_enabled NOTIFY enabled_changed)

public:
  explicit AudioStatusHud(QObject* parent = nullptr);

  [[nodiscard]] auto overlay_text() const -> QString { return m_overlay; }
  [[nodiscard]] auto enabled() const -> bool { return m_enabled; }

  void set_enabled(bool on);

signals:
  void overlay_changed();
  void enabled_changed();

private:
  void refresh();

  QString m_overlay;
  QTimer m_timer;
  bool m_enabled{false};
};

} // namespace App::Audio
