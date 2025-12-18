#pragma once

#include <QString>
#include <optional>
#include <vector>

namespace Game::Campaign {

struct CampaignMission {
  QString mission_id;
  int order_index = 0;
  std::optional<QString> intro_text;
  std::optional<QString> outro_text;
  std::optional<float> difficulty_modifier;
};

struct CampaignDefinition {
  QString id;
  QString title;
  QString description;
  std::vector<CampaignMission> missions;
};

} 
