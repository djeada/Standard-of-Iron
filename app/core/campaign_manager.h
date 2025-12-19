#pragma once

#include "game/map/mission_definition.h"
#include <QObject>
#include <QString>
#include <QVariantList>
#include <optional>

namespace Game::Mission {
struct MissionDefinition;
}

namespace Game::Systems {
class VictoryService;
}

class CampaignManager : public QObject {
  Q_OBJECT

public:
  explicit CampaignManager(QObject *parent = nullptr);

  void load_campaigns();
  void set_available_campaigns(const QVariantList &campaigns);
  void start_campaign_mission(const QString &mission_path,
                              int &selected_player_id);
  void mark_current_mission_completed();

  [[nodiscard]] QVariantList available_campaigns() const {
    return m_available_campaigns;
  }
  [[nodiscard]] QString current_campaign_id() const {
    return m_current_campaign_id;
  }
  [[nodiscard]] QString current_mission_id() const {
    return m_current_mission_id;
  }
  [[nodiscard]] const std::optional<Game::Mission::MissionDefinition> &
  current_mission_definition() const {
    return m_current_mission_definition;
  }

  void configure_mission_victory_conditions(
      Game::Systems::VictoryService *victory_service, int local_owner_id);

signals:
  void available_campaigns_changed();
  void current_campaign_changed();
  void current_mission_changed();

private:
  QVariantList m_available_campaigns;
  QString m_current_campaign_id;
  QString m_current_mission_id;
  std::optional<Game::Mission::MissionDefinition> m_current_mission_definition;
};
