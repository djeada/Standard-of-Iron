#include "app/viewmodels/commander_message_view_model.h"

namespace App::ViewModels {

CommanderMessageViewModel::CommanderMessageViewModel(QObject* parent)
    : QObject(parent) {
}

void CommanderMessageViewModel::set_message(const QVariantMap& message) {
  if (m_message == message) {
    return;
  }
  m_message = message;
  emit message_changed();
}

void CommanderMessageViewModel::clear() {
  if (m_message.isEmpty()) {
    return;
  }
  m_message.clear();
  emit message_changed();
}

auto CommanderMessageViewModel::active() const -> bool {
  return !m_message.isEmpty() && !text().isEmpty();
}

auto CommanderMessageViewModel::message_id() const -> QString {
  return m_message.value("id").toString();
}

auto CommanderMessageViewModel::speaker_name() const -> QString {
  return m_message.value("speaker_name").toString();
}

auto CommanderMessageViewModel::speaker_role() const -> QString {
  return m_message.value("speaker_role").toString();
}

auto CommanderMessageViewModel::nation() const -> QString {
  return m_message.value("nation").toString();
}

auto CommanderMessageViewModel::relationship() const -> QString {
  return m_message.value("relationship").toString();
}

auto CommanderMessageViewModel::is_ally() const -> bool {
  return relationship() == QStringLiteral("ally");
}

auto CommanderMessageViewModel::speaker_id() const -> QString {
  return m_message.value("speaker_id").toString();
}

auto CommanderMessageViewModel::pose() const -> QString {
  return m_message.value("pose").toString();
}

auto CommanderMessageViewModel::text() const -> QString {
  return m_message.value("text").toString();
}

auto CommanderMessageViewModel::duration() const -> qreal {
  return m_message.value("duration", 0.0).toReal();
}

auto CommanderMessageViewModel::holds_outcome() const -> bool {
  return active() && m_message.value("holds_outcome", false).toBool();
}

void CommanderMessageViewModel::dismiss() {
  if (!active()) {
    return;
  }
  emit dismiss_requested();
}

} // namespace App::ViewModels
