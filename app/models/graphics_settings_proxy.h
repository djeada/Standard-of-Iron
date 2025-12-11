#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace App::Models {

class GraphicsSettingsProxy : public QObject {
  Q_OBJECT
  Q_PROPERTY(int quality_level READ quality_level WRITE set_quality_level NOTIFY
                 quality_level_changed)
  Q_PROPERTY(
      QString quality_name READ quality_name NOTIFY quality_level_changed)
  Q_PROPERTY(QStringList quality_options READ quality_options CONSTANT)

public:
  explicit GraphicsSettingsProxy(QObject *parent = nullptr);
  ~GraphicsSettingsProxy() override = default;

  [[nodiscard]] int quality_level() const;
  void set_quality_level(int level);

  [[nodiscard]] QString quality_name() const;

  [[nodiscard]] QStringList quality_options() const;

  Q_INVOKABLE void set_quality_by_name(const QString &name);
  Q_INVOKABLE QString get_quality_description() const;

signals:
  void quality_level_changed();
};

} // namespace App::Models
