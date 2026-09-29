#pragma once

#include <QString>

#include <optional>

#include "app/core/player_feedback.h"
#include "app/orders/order_feedback.h"
#include "app/orders/order_markers.h"
#include "game/command/command.h"
#include "game/command/command_validator.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {
class SelectionController;
}

namespace App::ViewModels {
class MinimapViewModel;
}

namespace App::Mission {
class TutorialRuntime;
}

namespace App::Core {

struct OrderFeedbackSources {
  Engine::Core::World* world = nullptr;
  App::ViewModels::MinimapViewModel* minimap = nullptr;
  PlayerFeedbackBus* feedback = nullptr;
  App::Mission::TutorialRuntime* tutorial = nullptr;
  const Game::Systems::SelectionController* selection = nullptr;
  const int* local_owner_id = nullptr;
};

struct OrderAnnouncement {
  QString kind;
  bool accepted = false;
  QString message;
  QString failure;
};

class OrderFeedbackPresenter {
public:
  explicit OrderFeedbackPresenter(const OrderFeedbackSources& sources)
      : m_sources(sources) {}

  [[nodiscard]] static auto
  late_rejection(const Game::Command::Command& command,
                 Game::Command::Rejection reason) -> std::optional<OrderOutcome>;

  [[nodiscard]] auto
  present(const OrderOutcome& outcome) -> std::optional<OrderAnnouncement>;

  void warn(const char* cue_id) const;

  void update_markers(float dt) { m_markers.update(dt, m_sources.world); }
  [[nodiscard]] auto markers() const -> const std::vector<OrderMarker>& {
    return m_markers.markers();
  }

private:
  [[nodiscard]] auto present_accepted(const OrderOutcome& outcome) const -> QString;
  [[nodiscard]] auto present_refused(const OrderOutcome& outcome) const -> QString;

  OrderFeedbackSources m_sources;
  OrderMarkerStore m_markers;
};

} // namespace App::Core
