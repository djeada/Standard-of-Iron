#include "campaign_manager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QStringList>
#include <QVariantMap>

#include "game/map/map_definition.h"
#include "game/map/mission_loader.h"
#include "game/map/mission_victory_rules.h"
#include "game/systems/save_load_service.h"
#include "game/systems/save_storage.h"
#include "game/systems/victory_service.h"

CampaignManager::CampaignManager(QObject* parent)
    : QObject(parent) {
}

void CampaignManager::load_campaigns() {
  emit available_campaigns_changed();
}

void CampaignManager::set_available_campaigns(const QVariantList& campaigns) {
  m_available_campaigns = campaigns;
  emit available_campaigns_changed();
}

void CampaignManager::start_campaign_mission(const QString& mission_path, int&) {
  const QStringList parts = mission_path.split('/');
  if (parts.size() != 2) {
    qWarning() << "Invalid mission path format. Expected: campaign_id/mission_id";
    return;
  }

  const QString& campaign_id = parts[0];
  const QString& mission_id = parts[1];

  QStringList const search_paths = {
      QString("assets/missions/%1.json").arg(mission_id),
      QString("../assets/missions/%1.json").arg(mission_id),
      QString("../../assets/missions/%1.json").arg(mission_id),
      QCoreApplication::applicationDirPath() +
          QString("/assets/missions/%1.json").arg(mission_id),
      QCoreApplication::applicationDirPath() +
          QString("/../assets/missions/%1.json").arg(mission_id)};

  QString mission_file_path;
  bool found = false;

  for (const QString& path : search_paths) {
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
  if (!Game::Mission::MissionLoader::load_from_json_file(
          mission_file_path, mission, &error)) {
    qWarning() << QString("Failed to load mission %1: %2").arg(mission_id).arg(error);
    return;
  }

  m_current_campaign_id = campaign_id;
  m_current_mission_id = mission_id;
  m_current_mission_definition = mission;

  m_current_mission_context.mode = "campaign";
  m_current_mission_context.campaign_id = campaign_id;
  m_current_mission_context.mission_id = mission_id;
  m_current_mission_context.difficulty = "normal";

  emit current_campaign_changed();
  emit current_mission_changed();
}

void CampaignManager::mark_current_mission_completed() {
  if (m_current_campaign_id.isEmpty() || m_current_mission_id.isEmpty()) {
    qWarning() << "No active campaign mission to mark as completed";
    return;
  }

  qInfo() << "Campaign mission" << m_current_campaign_id << "/" << m_current_mission_id
          << "marked as completed";

  auto* save_service = Game::Systems::SaveLoadService::instance();
  if (save_service != nullptr) {
    QString error;

    bool const saved =
        save_service->save_mission_result(m_current_mission_id,
                                          m_current_mission_context.mode,
                                          m_current_campaign_id,
                                          true,
                                          "victory",
                                          m_current_mission_context.difficulty,
                                          0.0F,
                                          &error);

    if (!saved) {
      qWarning() << "Failed to save mission result:" << error;
    } else {

      if (m_current_mission_context.is_campaign()) {
        bool const unlocked = save_service->unlock_next_campaign_mission(
            m_current_campaign_id, m_current_mission_id, &error);
        if (!unlocked) {
          qWarning() << "Failed to unlock next mission:" << error;
        } else {
          qInfo() << "Next mission unlocked successfully, reloading campaigns";
          emit available_campaigns_changed();
        }
      }
    }
  }
}

void CampaignManager::set_skirmish_context(const QString& map_path) {
  m_current_campaign_id.clear();
  m_current_mission_id.clear();
  m_current_mission_definition.reset();

  m_current_mission_context.mode = "skirmish";
  m_current_mission_context.campaign_id = "";
  m_current_mission_context.mission_id = map_path;
  m_current_mission_context.difficulty = "normal";

  emit current_campaign_changed();
  emit current_mission_changed();
}

void CampaignManager::configure_mission_victory_conditions(
    Game::Systems::VictoryService* victory_service, int local_owner_id) {
  if ((victory_service == nullptr) || !m_current_mission_context.is_campaign() ||
      !m_current_mission_definition.has_value()) {
    return;
  }

  const auto& mission = *m_current_mission_definition;
  victory_service->configure(Game::Mission::build_victory_rules(mission),
                             local_owner_id);
  qInfo() << "Applied mission victory conditions from" << m_current_mission_id;
}
