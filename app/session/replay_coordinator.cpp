#include "app/session/replay_coordinator.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <cstdint>
#include <memory>

#include "game/mission/difficulty_profile.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"

namespace App::Session {

auto ReplayCoordinator::begin_playback(
    const QString& path, const ReplayStarters& starters) -> ReplayPlaybackResult {
  QString error;
  auto file = Game::Command::ReplayFile::load(path, &error);
  if (!file.has_value()) {
    return {ReplayPlaybackResult::Failure::LoadFailed, error};
  }
  const Game::Command::ReplayHeader header = file->header;
  m_pending = std::move(file);

  const QString difficulty = Game::Mission::normalize_difficulty_id(
      header.launch.value(QLatin1String("difficulty")).toString());
  if (header.kind == QLatin1String("campaign-mission")) {
    starters.campaign_mission(header.reference, difficulty);
  } else if (header.kind == QLatin1String("mission-file")) {
    starters.mission_file(header.reference, difficulty);
  } else if (header.kind == QLatin1String("skirmish")) {
    starters.skirmish(
        header.reference,
        header.launch.value(QLatin1String("player_configs")).toArray().toVariantList());
  } else {
    m_pending.reset();
    return {ReplayPlaybackResult::Failure::UnknownKind, header.kind};
  }
  return {};
}

void ReplayCoordinator::arm_for_started_match(Game::Session::SessionContext* session) {
  if (session == nullptr) {
    return;
  }
  if (m_pending.has_value()) {
    auto file = std::move(*m_pending);
    m_pending.reset();
    qInfo() << "Replay: driving" << m_launch.kind << m_launch.reference << "from"
            << file.commands.size() << "commands, last tick" << file.last_tick();
    session->set_replay_player(
        std::make_unique<Game::Command::ReplayPlayer>(std::move(file)));
    return;
  }
  if (m_record_path.isEmpty()) {
    return;
  }
  Game::Command::ReplayHeader header;
  header.kind = m_launch.kind;
  header.reference = m_launch.reference;
  header.launch["player_configs"] =
      QJsonArray::fromVariantList(m_launch.player_configs);
  header.launch["difficulty"] = m_launch.difficulty;
  header.tick_seconds = session->clock().tick_seconds();
  header.rng_seed = session->rng_seed();
  auto recorder = std::make_unique<Game::Command::ReplayRecorder>();
  if (!recorder->begin(m_record_path, header, session->commands())) {
    qWarning() << "Replay: cannot write" << m_record_path;
    return;
  }
  qInfo() << "Replay: recording to" << m_record_path;
  session->set_replay_recorder(std::move(recorder));
}

void ReplayCoordinator::finish_verification_if_done(
    Game::Session::SessionContext* session) {
  if (!m_verify_exit || session == nullptr) {
    return;
  }
  auto* player = session->replay_player();
  if (player == nullptr) {
    return;
  }
  const auto& file = player->file();
  const std::uint64_t last_recorded_tick = std::max<std::uint64_t>(
      file.last_tick(), file.digests.empty() ? 0U : file.digests.back().tick);
  if (!player->finished() || session->clock().tick() <= last_recorded_tick) {
    return;
  }
  if (const auto& divergence = player->divergence(); divergence.has_value()) {
    qCritical() << "SOI_REPLAY_VERIFY: FAIL - diverged at tick" << divergence->tick
                << "(recorded" << divergence->recorded << ", observed"
                << divergence->observed << ")";
    QCoreApplication::exit(12);
  } else {
    qInfo() << "SOI_REPLAY_VERIFY: PASS -" << player->fed_count() << "commands,"
            << player->checked_count() << "digests matched";
    QCoreApplication::exit(0);
  }
  m_verify_exit = false;
}

} // namespace App::Session
