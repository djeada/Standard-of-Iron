#include "game/mission/campaign_manager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QVariantMap>

#include "game/map/map_definition.h"
#include "game/map/mission_loader.h"
#include "game/map/mission_victory_rules.h"
#include "game/mission/difficulty_profile.h"
#include "game/systems/save_load_service.h"
#include "game/systems/save_storage.h"
#include "game/systems/victory_service.h"

CampaignManager::CampaignManager(QObject* parent)
    : QObject(parent) {
}

namespace {

auto find_mission_file(const QString& mission_id) -> QString {
  const QStringList search_paths = {
      QString("assets/missions/%1.json").arg(mission_id),
      QString("../assets/missions/%1.json").arg(mission_id),
      QString("../../assets/missions/%1.json").arg(mission_id),
      QCoreApplication::applicationDirPath() +
          QString("/assets/missions/%1.json").arg(mission_id),
      QCoreApplication::applicationDirPath() +
          QString("/../assets/missions/%1.json").arg(mission_id)};

  for (const QString& path : search_paths) {
    if (QFile::exists(path)) {
      return path;
    }
  }
  return QString(":/assets/missions/%1.json").arg(mission_id);
}

auto canonical_mission_reference(const QString& file_path,
                                 const QString& mission_id) -> QString {
  if (mission_id.isEmpty()) {
    return file_path;
  }
  const QString bundled = QString(":/assets/missions/%1.json").arg(mission_id);
  if (!QFile::exists(bundled)) {
    return file_path;
  }
  if (file_path.startsWith(QLatin1String(":/"))) {
    return bundled;
  }
  const QString resolved = find_mission_file(mission_id);
  if (resolved == file_path) {
    return bundled;
  }
  const QFileInfo given(file_path);
  const QFileInfo found(resolved);
  if (given.exists() && found.exists() &&
      given.canonicalFilePath() == found.canonicalFilePath()) {
    return bundled;
  }
  return file_path;
}

} // namespace

auto CampaignManager::save_service() const -> Game::Systems::SaveLoadService* {
  return m_save_service != nullptr ? m_save_service
                                   : Game::Systems::SaveLoadService::instance();
}

void CampaignManager::restore_mission_context(
    const Game::Mission::MissionContext& context) {
  m_current_mission_context = context;
  m_current_campaign_id = context.campaign_id;
  m_current_mission_id = context.mission_id;
  m_campaign_completed = false;

  if (!context.has_mission() || context.mission_id.isEmpty()) {
    m_current_mission_definition.reset();
    emit current_campaign_changed();
    emit current_mission_changed();
    return;
  }

  const bool has_original = !context.mission_file.isEmpty();
  const QString mission_file =
      has_original ? context.mission_file : find_mission_file(context.mission_id);
  Game::Mission::MissionDefinition mission;
  QString error;
  if (!Game::Mission::MissionLoader::load_from_json_file(
          mission_file, mission, &error)) {
    m_current_mission_definition.reset();
    qWarning() << "CampaignManager: could not reload mission" << context.mission_id
               << "from" << mission_file << "after loading a save:" << error
               << "- victory conditions and campaign progression will not apply";
  } else if (has_original && !context.mission_id.isEmpty() &&
             mission.id != context.mission_id) {
    m_current_mission_definition.reset();
    qWarning() << "CampaignManager:" << mission_file << "now holds mission"
               << mission.id << "but the save was made in" << context.mission_id
               << "- victory conditions and campaign progression will not apply";
  } else {
    m_current_mission_definition = mission;
  }

  emit current_campaign_changed();
  emit current_mission_changed();
}

void CampaignManager::load_campaigns() {
  emit available_campaigns_changed();
}

void CampaignManager::set_available_campaigns(const QVariantList& campaigns) {
  m_available_campaigns = campaigns;
  emit available_campaigns_changed();
}

void CampaignManager::start_campaign_mission(const QString& mission_path,
                                             int&,
                                             const QString& difficulty) {
  const QStringList parts = mission_path.split('/');
  if (parts.size() != 2) {
    qWarning() << "Invalid mission path format. Expected: campaign_id/mission_id";
    return;
  }

  const QString& campaign_id = parts[0];
  const QString& mission_id = parts[1];

  const QString mission_file_path = find_mission_file(mission_id);
  qInfo() << "Loading mission from" << mission_file_path;

  Game::Mission::MissionDefinition mission;
  QString error;
  if (!Game::Mission::MissionLoader::load_from_json_file(
          mission_file_path, mission, &error)) {
    qWarning() << QString("Failed to load mission %1: %2").arg(mission_id).arg(error);
    return;
  }

  m_current_campaign_id = campaign_id;
  m_current_mission_id = mission_id;
  m_current_mission_definition = mission;
  m_campaign_completed = false;

  m_current_mission_context.mode = "campaign";
  m_current_mission_context.campaign_id = campaign_id;
  m_current_mission_context.mission_id = mission_id;
  m_current_mission_context.mission_file =
      canonical_mission_reference(mission_file_path, mission_id);
  m_current_mission_context.difficulty =
      Game::Mission::normalize_difficulty_id(difficulty);

  emit current_campaign_changed();
  emit current_mission_changed();
}

bool CampaignManager::start_mission_file(const QString& file_path,
                                         int& selected_player_id,
                                         QString* out_error,
                                         const QString& difficulty) {
  Game::Mission::MissionDefinition mission;
  QString error;
  if (!Game::Mission::MissionLoader::load_from_json_file(file_path, mission, &error)) {
    if (out_error != nullptr) {
      *out_error = error;
    }
    return false;
  }

  selected_player_id = 1;
  m_campaign_completed = false;
  m_current_campaign_id.clear();
  m_current_mission_id = mission.id;
  m_current_mission_definition = mission;

  m_current_mission_context.mode = QStringLiteral("mission");
  m_current_mission_context.campaign_id.clear();
  m_current_mission_context.mission_id = mission.id;
  m_current_mission_context.mission_file =
      canonical_mission_reference(file_path, mission.id);
  m_current_mission_context.difficulty =
      Game::Mission::normalize_difficulty_id(difficulty);
  emit current_campaign_changed();
  emit current_mission_changed();
  return true;
}

void CampaignManager::mark_current_mission_completed() {
  if (m_current_mission_id.isEmpty() || !m_current_mission_context.has_mission()) {
    qWarning() << "No active mission to mark as completed";
    return;
  }

  auto* save_service = this->save_service();
  if (save_service == nullptr) {
    qWarning() << "Save/Load service not initialized";
    return;
  }

  QString error;
  if (!save_service->save_mission_result(m_current_mission_id,
                                         m_current_mission_context.mode,
                                         m_current_campaign_id,
                                         true,
                                         "victory",
                                         m_current_mission_context.difficulty,
                                         0.0F,
                                         &error)) {
    qWarning() << "Failed to save mission result:" << error;
    return;
  }

  if (!m_current_mission_context.is_campaign()) {
    return;
  }

  const auto advance = save_service->complete_campaign_mission(
      m_current_campaign_id, m_current_mission_id, &error);
  if (!advance.has_value()) {
    qWarning() << "Failed to advance campaign:" << error;
    return;
  }

  m_campaign_completed = advance->campaign_completed;
  if (advance->unlocked_mission_id.isEmpty()) {
    qInfo() << "Campaign" << m_current_campaign_id
            << (advance->campaign_completed ? "completed" : "has no further missions");
  } else {
    qInfo() << "Unlocked next mission:" << advance->unlocked_mission_id;
  }
  emit available_campaigns_changed();
}

void CampaignManager::set_current_difficulty(const QString& difficulty) {
  const QString normalized = Game::Mission::normalize_difficulty_id(difficulty);
  if (m_current_mission_context.difficulty == normalized) {
    return;
  }
  m_current_mission_context.difficulty = normalized;
  emit current_mission_changed();
}

void CampaignManager::set_skirmish_context(const QString& map_path) {
  m_campaign_completed = false;
  m_current_campaign_id.clear();
  m_current_mission_id.clear();
  m_current_mission_definition.reset();

  m_current_mission_context.mode = "skirmish";
  m_current_mission_context.campaign_id = "";
  m_current_mission_context.mission_id = map_path;
  m_current_mission_context.mission_file.clear();
  m_current_mission_context.difficulty = Game::Mission::default_difficulty_id();

  emit current_campaign_changed();
  emit current_mission_changed();
}

void CampaignManager::configure_mission_victory_conditions(
    Game::Systems::VictoryService* victory_service, int local_owner_id) {
  if ((victory_service == nullptr) || !m_current_mission_context.has_mission() ||
      !m_current_mission_definition.has_value()) {
    return;
  }

  const auto& mission = *m_current_mission_definition;
  victory_service->configure(Game::Mission::build_victory_rules(mission),
                             local_owner_id);
  qInfo() << "Applied mission victory conditions from" << m_current_mission_id;
}
