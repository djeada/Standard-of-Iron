#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace App::ViewModels {

class CommanderMessageViewModel : public QObject {
  Q_OBJECT

  Q_PROPERTY(bool active READ active NOTIFY message_changed)
  Q_PROPERTY(QString message_id READ message_id NOTIFY message_changed)
  Q_PROPERTY(QString speaker_name READ speaker_name NOTIFY message_changed)
  Q_PROPERTY(QString speaker_role READ speaker_role NOTIFY message_changed)
  Q_PROPERTY(QString nation READ nation NOTIFY message_changed)
  Q_PROPERTY(QString relationship READ relationship NOTIFY message_changed)
  Q_PROPERTY(bool is_ally READ is_ally NOTIFY message_changed)
  Q_PROPERTY(QString speaker_id READ speaker_id NOTIFY message_changed)
  Q_PROPERTY(QString pose READ pose NOTIFY message_changed)
  Q_PROPERTY(QString text READ text NOTIFY message_changed)
  Q_PROPERTY(qreal duration READ duration NOTIFY message_changed)
  Q_PROPERTY(bool holds_outcome READ holds_outcome NOTIFY message_changed)

public:
  explicit CommanderMessageViewModel(QObject* parent = nullptr);

  void set_message(const QVariantMap& message);
  void clear();

  [[nodiscard]] auto active() const -> bool;
  [[nodiscard]] auto message_id() const -> QString;
  [[nodiscard]] auto speaker_name() const -> QString;
  [[nodiscard]] auto speaker_role() const -> QString;
  [[nodiscard]] auto nation() const -> QString;
  [[nodiscard]] auto relationship() const -> QString;
  [[nodiscard]] auto is_ally() const -> bool;
  [[nodiscard]] auto speaker_id() const -> QString;
  [[nodiscard]] auto pose() const -> QString;
  [[nodiscard]] auto text() const -> QString;
  [[nodiscard]] auto duration() const -> qreal;

  [[nodiscard]] auto holds_outcome() const -> bool;

  Q_INVOKABLE void dismiss();

signals:
  void message_changed();
  void dismiss_requested();

private:
  QVariantMap m_message;
};

} // namespace App::ViewModels
