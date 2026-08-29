#ifndef SOI_UI_HINTS_H
#define SOI_UI_HINTS_H

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class UiHints : public QObject {
  Q_OBJECT

  Q_PROPERTY(QVariantMap showing READ showing_map NOTIFY changed)
  Q_PROPERTY(QVariantMap enabled READ enabled_map NOTIFY changed)
  Q_PROPERTY(QVariantList catalog READ catalog NOTIFY changed)

public:
  static auto instance() -> UiHints*;
  static auto create(QQmlEngine* engine, QJSEngine* script_engine) -> UiHints*;

  [[nodiscard]] auto showing_map() const -> QVariantMap;
  [[nodiscard]] auto enabled_map() const -> QVariantMap;
  [[nodiscard]] auto catalog() const -> QVariantList;

  Q_INVOKABLE [[nodiscard]] bool is_showing(const QString& id) const;
  Q_INVOKABLE [[nodiscard]] bool is_enabled(const QString& id) const;

  Q_INVOKABLE void show(const QString& id);
  Q_INVOKABLE void reveal(const QString& id);
  Q_INVOKABLE void show_once(const QString& id);
  Q_INVOKABLE void dismiss(const QString& id);
  Q_INVOKABLE void dismiss_all();
  Q_INVOKABLE void on_selection_changed();
  Q_INVOKABLE void suppress(const QString& id);
  Q_INVOKABLE void set_enabled(const QString& id, bool enabled);
  Q_INVOKABLE void restore_all();

signals:
  void changed();

private:
  explicit UiHints(QObject* parent = nullptr);

  struct Definition {
    QString id;
    const char* settings_key;
    bool stores_dismissal;
    bool selection_scoped;
  };

  struct State {
    bool enabled = true;
    bool armed = false;
  };

  [[nodiscard]] auto definition(const QString& id) const -> const Definition*;
  void store(const Definition& definition, bool enabled);

  static auto definitions() -> const QVector<Definition>&;

  static UiHints* m_instance;

  QHash<QString, State> m_state;
};

#endif
