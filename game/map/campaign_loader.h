#pragma once

#include "campaign_definition.h"
#include <QJsonObject>
#include <QString>

namespace Game::Campaign {

class CampaignLoader {
public:
  static auto loadFromJsonFile(const QString &file_path,
                               CampaignDefinition &out_campaign,
                               QString *error_msg = nullptr) -> bool;

private:
  static auto parseCampaignMission(const QJsonObject &obj) -> CampaignMission;
};

} 
