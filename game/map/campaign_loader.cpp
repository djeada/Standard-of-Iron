#include "campaign_loader.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Game::Campaign {

auto CampaignLoader::parseCampaignMission(const QJsonObject &obj)
    -> CampaignMission {
  CampaignMission mission;
  mission.mission_id = obj["mission_id"].toString();
  mission.order_index = obj["order_index"].toInt(0);

  if (obj.contains("intro_text")) {
    mission.intro_text = obj["intro_text"].toString();
  }

  if (obj.contains("outro_text")) {
    mission.outro_text = obj["outro_text"].toString();
  }

  if (obj.contains("difficulty_modifier")) {
    mission.difficulty_modifier =
        static_cast<float>(obj["difficulty_modifier"].toDouble());
  }

  if (obj.contains("world_region_id")) {
    mission.world_region_id = obj["world_region_id"].toString();
  }

  return mission;
}

auto CampaignLoader::loadFromJsonFile(const QString &file_path,
                                      CampaignDefinition &out_campaign,
                                      QString *error_msg) -> bool {
  QFile file(file_path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (error_msg != nullptr) {
      *error_msg = QString("Failed to open file: %1").arg(file_path);
    }
    return false;
  }

  QJsonParseError parse_error;
  const QJsonDocument doc =
      QJsonDocument::fromJson(file.readAll(), &parse_error);
  file.close();

  if (parse_error.error != QJsonParseError::NoError) {
    if (error_msg != nullptr) {
      *error_msg =
          QString("JSON parse error: %1").arg(parse_error.errorString());
    }
    return false;
  }

  if (!doc.isObject()) {
    if (error_msg != nullptr) {
      *error_msg = "JSON root is not an object";
    }
    return false;
  }

  const QJsonObject root = doc.object();

  out_campaign.id = root["id"].toString();
  out_campaign.title = root["title"].toString();
  out_campaign.description = root["description"].toString();

  if (root.contains("missions")) {
    const QJsonArray missions = root["missions"].toArray();
    for (const auto &mission_val : missions) {
      out_campaign.missions.push_back(
          parseCampaignMission(mission_val.toObject()));
    }
  }

  return true;
}

} 
