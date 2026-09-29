#include "app/mission/commander_message_runtime.h"

#include <QDebug>
#include <QVariantMap>
#include <QVector3D>

#include <optional>
#include <vector>

#include "app/viewmodels/commander_message_view_model.h"
#include "app/world/ally_announcements.h"
#include "game/audio/audio_cues.h"
#include "game/core/component_core.h"
#include "game/core/world.h"
#include "game/mission/campaign_manager.h"
#include "game/mission/commander_speaker_roster.h"
#include "game/mission/mission_setup_coordinator.h"
#include "game/session/session_context.h"
#include "game/systems/ai_system.h"
#include "game/systems/match_snapshot.h"
#include "game/systems/owner_registry.h"
#include "game/util/asset_text.h"

namespace App::Mission {

CommanderMessageRuntime::CommanderMessageRuntime(
    App::ViewModels::CommanderMessageViewModel* view_model)
    : m_view_model(view_model) {
}

auto CommanderMessageRuntime::voices() -> const Game::Mission::CommanderVoiceLibrary& {
  if (!m_voices_loaded) {
    m_voices_loaded = true;
    QString error;
    m_voices = Game::Mission::CommanderVoiceLibrary::load_default(&error);
    if (!error.isEmpty()) {
      qWarning() << "Commander voice banks:" << error;
    }
  }
  return m_voices;
}

void CommanderMessageRuntime::clear() {
  m_start_cue_pending = false;
  m_director.clear();
  m_observer.clear();
  if (m_view_model != nullptr) {
    m_view_model->clear();
    m_view_model->set_outcome_line_pending(false);
  }
}

void CommanderMessageRuntime::configure(const CommanderMessageBinding& binding) {
  m_start_cue_pending = false;
  m_director.clear();
  m_observer.clear();
  publish(binding);

  if (binding.world == nullptr || binding.session == nullptr) {
    return;
  }
  Engine::Core::World* world = binding.world;
  Game::Session::SessionContext* session = binding.session;
  const int local = binding.local_owner_id;

  Game::Mission::CommanderMessageScript script;
  const bool has_mission = binding.campaign != nullptr &&
                           binding.campaign->current_mission_definition().has_value();
  if (has_mission) {
    const auto& mission = *binding.campaign->current_mission_definition();
    script.mission_lines = mission.commander_messages;
    script.policy = mission.commander_voices;
  }
  script.speakers = Game::Mission::build_commander_speaker_roster(
      *world, session->owners(), session->nations(), local);
  script.local_speaker = Game::Mission::local_commander_speaker(*world, local);
  script.voices = &voices();

  m_director.configure(
      script, local, Game::Mission::make_mission_position_to_world(*binding.level));

  std::vector<int> watched;
  watched.reserve(script.speakers.size() + 1);
  watched.push_back(local);
  for (const auto& speaker : script.speakers) {
    watched.push_back(speaker.owner_id);
  }
  m_observer.configure(std::move(watched), local);

  m_director.set_relationship_lookup([session](int owner_a, int owner_b) -> bool {
    return session->owners().are_allies(owner_a, owner_b);
  });
  m_director.set_structure_position_lookup(
      [world](Engine::Core::EntityID id) -> std::optional<QVector3D> {
        const auto* transform = world->try_get<Engine::Core::TransformComponent>(id);
        if (transform == nullptr) {
          return std::nullopt;
        }
        return QVector3D(
            transform->position.x, transform->position.y, transform->position.z);
      });
}

void CommanderMessageRuntime::release_pending_start_cue(bool allowed) {
  if (!m_start_cue_pending || !allowed) {
    return;
  }
  m_start_cue_pending = false;
  m_director.notify_mission_start();
}

void CommanderMessageRuntime::update(const CommanderMessageBinding& binding,
                                     const CommanderMessageFrame& frame,
                                     float dt) {
  if (!m_director.has_messages()) {
    return;
  }
  release_pending_start_cue(frame.may_release_start_cue);
  if (binding.world != nullptr && !frame.match_decided && m_observer.is_configured()) {
    const Game::Mission::AiSystemAttackPlanSource plans(
        binding.world->get_system<Game::Systems::AISystem>());
    m_observer.update(*binding.world, &plans, dt);
  }
  if (m_director.update(dt)) {
    publish(binding);
  }
  if (m_view_model != nullptr) {
    m_view_model->set_outcome_line_pending(m_director.outcome_line_pending());
  }
}

void CommanderMessageRuntime::dismiss_active(const CommanderMessageBinding& binding) {
  if (m_director.dismiss_active()) {
    publish(binding);
  }
}

void CommanderMessageRuntime::notify_outcome(const QString& state) {
  if (state == "victory") {
    m_director.notify_victory();
  } else if (state == "defeat") {
    m_director.notify_defeat();
  }
  if (m_view_model != nullptr) {
    m_view_model->set_outcome_line_pending(m_director.outcome_line_pending());
  }
}

auto CommanderMessageRuntime::serialize() const -> QJsonObject {
  QJsonObject state = m_director.serialize();
  state["observer"] = m_observer.serialize();
  return state;
}

void CommanderMessageRuntime::restore(const CommanderMessageBinding& binding,
                                      const QJsonObject& state) {
  m_director.restore(state);
  if (state.contains("observer")) {
    m_observer.restore(state["observer"].toObject());
  }
  publish(binding);
}

void CommanderMessageRuntime::publish(const CommanderMessageBinding& binding) {
  if (m_view_model == nullptr) {
    return;
  }
  if (!m_director.has_active()) {
    m_view_model->clear();
    return;
  }

  const auto& cue = m_director.active();
  QVariantMap message;
  message["id"] = cue.id;
  message["speaker_id"] = cue.speaker_id;
  message["speaker_name"] =
      Game::Util::tr_asset(Game::Util::k_commanders_context, cue.speaker_name);
  message["speaker_role"] =
      Game::Util::tr_asset(Game::Util::k_commanders_context, cue.speaker_role);
  message["nation"] = cue.nation;
  message["relationship"] = cue.speaker_owner_id == binding.local_owner_id
                                ? QStringLiteral("own")
                                : cue.relationship;
  message["speaker_owner_id"] = cue.speaker_owner_id;
  message["pose"] = cue.pose;
  QString text = Game::Util::tr_asset(
      cue.text_context != nullptr ? cue.text_context : Game::Util::k_missions_context,
      cue.text);
  if (cue.amount > 0) {
    text.replace(QStringLiteral("{amount}"), QString::number(cue.amount));
  }
  if (!cue.resource.isEmpty()) {
    text.replace(QStringLiteral("{resource}"),
                 App::World::ally_resource_word(cue.resource));
  }
  message["text"] = text;
  if (cue.request_owner_id >= 0 && cue.amount > 0 && !cue.resource.isEmpty()) {
    QVariantMap request;
    request["owner_id"] = cue.request_owner_id;
    request["owner_name"] =
        App::World::owner_display_name(binding.session, cue.request_owner_id);
    request["resource"] = cue.resource;
    request["resource_label"] = App::World::ally_resource_word(cue.resource);
    request["amount"] = cue.amount;
    message["request"] = request;
  }
  message["duration"] = cue.duration;
  message["holds_outcome"] = cue.holds_outcome;
  m_view_model->set_message(message);

  Game::Audio::play_cue(Game::Audio::Cue::k_alert_commander_message);

  if (!cue.voice_cue.isEmpty()) {
    Game::Audio::play_cue(cue.voice_cue.toStdString());
  }
}

} // namespace App::Mission
