#include "campaign_manager.h"

#include "game/map/map_definition.h"
#include "game/map/mission_loader.h"
#include "game/systems/save_load_service.h"
#include "game/systems/victory_service.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QStringList>
#include <QVariantMap>

CampaignManager::CampaignManager(QObject *parent) : QObject(parent) {}

void CampaignManager::load_campaigns() {
  // This method now just provides structure
  // Actual loading will be done via SaveLoadService passed from GameEngine
  emit available_campaigns_changed();
}

void CampaignManager::start_campaign_mission(const QString &mission_path,
                                            int &selected_player_id) {
  const QStringList parts = mission_path.split('/');
  if (parts.size() != 2) {
    qWarning() << "Invalid mission path format. Expected: campaign_id/mission_id";
    return;
  }

  const QString campaign_id = parts[0];
  const QString mission_id = parts[1];

  QStringList search_paths = {
      QString("assets/missions/%1.json").arg(mission_id),
      QString("../assets/missions/%1.json").arg(mission_id),
      QString("../../assets/missions/%1.json").arg(mission_id),
      QCoreApplication::applicationDirPath() +
          QString("/assets/missions/%1.json").arg(mission_id),
      QCoreApplication::applicationDirPath() +
          QString("/../assets/missions/%1.json").arg(mission_id)};

  QString mission_file_path;
  bool found = false;

  for (const QString &path : search_paths) {
    if (QFile::exists(path)) {
      mission_file_path = path;
      found = true;
      qInfo() << "Loading mission from filesystem:" << mission_file_path;
      break;
    }
  }

  if (!found) {
    mission_file_path = QString(":/assets/missions/%1.json").arg(mission_id);
    qInfo() << "Loading mission from Qt resources:" << mission_file_path;
  }

  Game::Mission::MissionDefinition mission;
  QString error;
  if (!Game::Mission::MissionLoader::loadFromJsonFile(mission_file_path,
                                                      mission, &error)) {
    qWarning() << QString("Failed to load mission %1: %2")
                      .arg(mission_id)
                      .arg(error);
    return;
  }

  m_current_campaign_id = campaign_id;
  m_current_mission_id = mission_id;
  m_current_mission_definition = mission;

  emit current_campaign_changed();
  emit current_mission_changed();
}

void CampaignManager::mark_current_mission_completed() {
  if (m_current_campaign_id.isEmpty()) {
    qWarning() << "No active campaign mission to mark as completed";
    return;
  }

  qInfo() << "Campaign mission" << m_current_campaign_id
          << "marked as completed";
}

void CampaignManager::configure_mission_victory_conditions(
    Game::Systems::VictoryService *victory_service, int local_owner_id) {
  if (!victory_service || !m_current_mission_definition.has_value()) {
    return;
  }

  const auto &mission = *m_current_mission_definition;
  Game::Map::VictoryConfig mission_victory_config;

  if (!mission.victory_conditions.empty()) {
    const auto &first_condition = mission.victory_conditions[0];
    if (first_condition.type == "destroy_all_enemies") {
      mission_victory_config.victoryType = "elimination";
      mission_victory_config.keyStructures = {"barracks"};
    } else if (first_condition.type == "survive_duration" &&
               first_condition.duration.has_value()) {
      mission_victory_config.victoryType = "survive_time";
      mission_victory_config.surviveTimeDuration = *first_condition.duration;
    } else {
      mission_victory_config.victoryType = "elimination";
      mission_victory_config.keyStructures = {"barracks"};
    }
  }

  for (const auto &defeat_condition : mission.defeat_conditions) {
    if (defeat_condition.type == "lose_structure" &&
        defeat_condition.structure_type.has_value()) {
      mission_victory_config.defeatConditions.push_back("no_key_structures");
      mission_victory_config.keyStructures.push_back(
          *defeat_condition.structure_type);
    } else if (defeat_condition.type == "lose_all_units") {
      mission_victory_config.defeatConditions.push_back("no_units");
    }
  }

  victory_service->configure(mission_victory_config, local_owner_id);
  qInfo() << "Applied mission victory conditions from" << m_current_mission_id;
}
