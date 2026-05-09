#pragma once

#include "campaign_definition.h"
#include <QJsonObject>
#include <QString>

namespace Game::Campaign {

class CampaignLoader {
public:
  static auto load_from_json_file(const QString &file_path,
                               CampaignDefinition &out_campaign,
                               QString *error_msg = nullptr) -> bool;

private:
  static auto parse_campaign_mission(const QJsonObject &obj) -> CampaignMission;
};

} // namespace Game::Campaign
