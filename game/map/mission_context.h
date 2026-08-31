#pragma once

#include <QString>

namespace Game::Mission {

struct MissionContext {
  QString mode;
  QString campaign_id;
  QString mission_id;
  QString difficulty;

  [[nodiscard]] bool is_campaign() const { return mode == "campaign"; }
  [[nodiscard]] bool is_standalone_mission() const { return mode == "mission"; }
  [[nodiscard]] bool has_mission() const {
    return is_campaign() || is_standalone_mission();
  }
  [[nodiscard]] bool is_skirmish() const { return mode == "skirmish"; }
};

} // namespace Game::Mission
