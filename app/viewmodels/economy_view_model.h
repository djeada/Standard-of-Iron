#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace App::ViewModels {

class EconomyViewModel : public QObject {
  Q_OBJECT

  Q_PROPERTY(QVariantList resources READ resources NOTIFY resources_changed)
  Q_PROPERTY(QVariantMap help READ help NOTIFY help_changed)
  Q_PROPERTY(QVariantMap coach READ coach NOTIFY coach_changed)
  Q_PROPERTY(bool coach_enabled READ coach_enabled WRITE set_coach_enabled NOTIFY
                 coach_enabled_changed)
  Q_PROPERTY(bool coach_visible READ coach_visible NOTIFY coach_visible_changed)

public:
  explicit EconomyViewModel(QObject* parent = nullptr);

  [[nodiscard]] auto resources() const -> QVariantList { return m_resources; }
  [[nodiscard]] auto help() const -> QVariantMap { return m_help; }
  [[nodiscard]] auto coach() const -> QVariantMap { return m_coach; }
  [[nodiscard]] auto coach_enabled() const -> bool { return m_coach_enabled; }
  [[nodiscard]] auto coach_visible() const -> bool;

  Q_INVOKABLE [[nodiscard]] QVariantMap resource(const QString& key) const;
  Q_INVOKABLE void dismiss_coach();

  void set_coach_enabled(bool enabled);
  void set_resources(const QVariantList& resources);
  void set_help(const QVariantMap& help);
  void set_coach(const QVariantMap& coach);
  void set_coach_available(bool available);
  void clear();

signals:
  void resources_changed();
  void help_changed();
  void coach_changed();
  void coach_enabled_changed();
  void coach_visible_changed();

private:
  void update_coach_visible();

  QVariantList m_resources;
  QVariantMap m_help;
  QVariantMap m_coach;
  bool m_coach_enabled = true;
  bool m_coach_available = false;
  bool m_coach_visible = false;
};

} // namespace App::ViewModels
