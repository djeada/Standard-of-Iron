#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace App::Models {

class GraphicsSettingsProxy : public QObject {
  Q_OBJECT
  Q_PROPERTY(int qualityLevel READ qualityLevel WRITE setQualityLevel NOTIFY
                 qualityLevelChanged)
  Q_PROPERTY(QString qualityName READ qualityName NOTIFY qualityLevelChanged)
  Q_PROPERTY(QStringList qualityOptions READ qualityOptions CONSTANT)

public:
  explicit GraphicsSettingsProxy(QObject *parent = nullptr);
  ~GraphicsSettingsProxy() override = default;

  [[nodiscard]] int qualityLevel() const;
  void setQualityLevel(int level);

  [[nodiscard]] QString qualityName() const;

  [[nodiscard]] QStringList qualityOptions() const;

  Q_INVOKABLE void setQualityByName(const QString &name);
  Q_INVOKABLE QString getQualityDescription() const;

signals:
  void qualityLevelChanged();
};

} // namespace App::Models
