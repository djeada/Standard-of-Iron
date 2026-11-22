#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <cstdint>

namespace App::Models {

class GraphicsSettingsProxy : public QObject {
  Q_OBJECT
  Q_PROPERTY(int quality_level READ quality_level WRITE set_quality_level NOTIFY
                 quality_level_changed)
  Q_PROPERTY(QString quality_name READ quality_name NOTIFY quality_level_changed)
  Q_PROPERTY(QStringList quality_options READ quality_options CONSTANT)
  Q_PROPERTY(QString quality_description READ get_quality_description NOTIFY
                 quality_level_changed)

public:
  explicit GraphicsSettingsProxy(QObject* parent = nullptr);
  ~GraphicsSettingsProxy() override = default;

  [[nodiscard]] int quality_level() const;
  void set_quality_level(int level);

  [[nodiscard]] QString quality_name() const;

  [[nodiscard]] QStringList quality_options() const;

  Q_INVOKABLE void set_quality_by_name(const QString& name);
  Q_INVOKABLE QString get_quality_description() const;

signals:
  void quality_level_changed();
  // The game stepped down to `preset_name` because battles ran too slowly.
  void quality_lowered_automatically(const QString& preset_name);

private:
  void poll_render_settings();

  // The preset can change outside this proxy: the renderer picks Low for a
  // weak adapter at start-up and steps down during slow battles. Both happen
  // on the render thread, so the proxy notices them here, on the GUI thread.
  QTimer m_poll;
  std::uint32_t m_seen_generation = 0;
  std::uint32_t m_seen_auto_lowered = 0;
};

} // namespace App::Models
