#pragma once

#include <QString>

namespace Game::Mission {

struct MissionContext {
  QString mode;        // "campaign" or "skirmish"
  QString campaign_id; // nullable for skirmish
  QString mission_id;
  QString difficulty; // "easy", "normal", "hard", etc.

  [[nodiscard]] bool is_campaign() const { return mode == "campaign"; }
  [[nodiscard]] bool is_skirmish() const { return mode == "skirmish"; }
};

} // namespace Game::Mission
