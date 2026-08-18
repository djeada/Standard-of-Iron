#pragma once

#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "game/mission/tutorial_director.h"

namespace Engine::Core {
class World;
}

namespace App::ViewModels {
class PlacementViewModel;
}

namespace App::Mission {

struct TutorialFrameNotes {
  bool move_accepted = false;
  bool attack_accepted = false;
  bool hold_accepted = false;
  bool guard_accepted = false;
  bool patrol_accepted = false;
  bool gather_accepted = false;
  bool build_accepted = false;
  bool speed_changed = false;
  bool camera_used = false;
  QString last_rejection_reason;

  void reset() { *this = TutorialFrameNotes{}; }
};

struct TutorialObservationInputs {

  Engine::Core::World* world = nullptr;
  const TutorialFrameNotes& notes;
  int local_owner_id = 1;
  QString victory_state;
  int enemy_troops_defeated = 0;
  bool mission_running = false;
  const App::ViewModels::PlacementViewModel* placement = nullptr;
  QVariantMap wave_status;
};

struct TutorialFocusInputs {

  Engine::Core::World* world = nullptr;
  int local_owner_id = 1;
  Game::Mission::TutorialFocusTarget target = Game::Mission::TutorialFocusTarget::None;
  QVariantList wave_alerts;
};

[[nodiscard]] auto observe_tutorial_frame(const TutorialObservationInputs& inputs)
    -> Game::Mission::TutorialObservation;

[[nodiscard]] auto
resolve_tutorial_focus_points(const TutorialFocusInputs& inputs) -> QVariantList;

} // namespace App::Mission
