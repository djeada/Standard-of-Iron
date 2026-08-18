#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector3D>

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include "game/core/event_manager.h"
#include "game/map/mission_definition.h"
#include "game/map/mission_stage_tracker.h"

namespace Game::Mission {

using StructurePositionLookup =
    std::function<std::optional<QVector3D>(Engine::Core::EntityID)>;

struct CommanderMessageCue {
  QString id;

  QString speaker_id;

  QString speaker_name;

  QString speaker_role;

  QString nation;

  QString pose;

  QString text;
  QString voice_cue;
  float duration = k_default_commander_message_seconds;

  bool holds_outcome = false;
};

class CommanderMessageDirector {
public:
  CommanderMessageDirector() = default;
  ~CommanderMessageDirector();

  CommanderMessageDirector(const CommanderMessageDirector&) = delete;
  auto operator=(const CommanderMessageDirector&) -> CommanderMessageDirector& = delete;
  CommanderMessageDirector(CommanderMessageDirector&&) = delete;
  auto operator=(CommanderMessageDirector&&) -> CommanderMessageDirector& = delete;

  void configure(const MissionDefinition& mission,
                 int local_owner_id,
                 const MissionPositionToWorld& to_world);

  void set_structure_position_lookup(StructurePositionLookup lookup) {
    m_structure_position = std::move(lookup);
  }
  void clear();

  [[nodiscard]] auto has_messages() const -> bool { return !m_rules.empty(); }

  void notify_mission_start();
  void notify_victory();
  void notify_defeat();

  auto update(float delta_time) -> bool;

  [[nodiscard]] auto has_active() const -> bool { return m_active.has_value(); }
  [[nodiscard]] auto active() const -> const CommanderMessageCue&;
  [[nodiscard]] auto active_remaining() const -> float { return m_active_remaining; }

  auto dismiss_active() -> bool;

  [[nodiscard]] auto serialize() const -> QJsonObject;
  void restore(const QJsonObject& state);

private:
  struct Rule {
    CommanderMessage authored;
    CommanderMessageCue cue;
    std::optional<QVector3D> world_target;
    bool fired = false;
  };

  struct Pending {
    std::size_t rule_index = 0;
    float delay_remaining = 0.0F;
  };

  struct CaptureFact {
    Engine::Core::EntityID structure_id = 0;
    int previous_owner_id = 0;
    int new_owner_id = 0;
  };

  struct CommanderDeathFact {
    int owner_id = 0;
    int killer_owner_id = 0;
    QString troop_type;
    QString nation;
  };

  void subscribe();
  void unsubscribe();

  void queue_trigger(CommanderMessageTrigger trigger);
  void queue_capture(const CaptureFact& fact);
  void queue_commander_death(const CommanderDeathFact& fact);
  void queue_rule(std::size_t index);

  [[nodiscard]] auto matches_owner(const CommanderMessageCondition& condition,
                                   int owner_id,
                                   int by_owner_id) const -> bool;

  auto promote_next() -> bool;

  std::vector<Rule> m_rules;
  std::vector<Pending> m_pending;

  std::optional<CommanderMessageCue> m_active;
  float m_active_remaining = 0.0F;

  int m_local_owner_id = 1;
  StructurePositionLookup m_structure_position;

  mutable std::mutex m_inbox_mutex;
  std::vector<CaptureFact> m_capture_inbox;
  std::vector<CommanderDeathFact> m_death_inbox;

  Engine::Core::ScopedEventSubscription<Engine::Core::BarrackCapturedEvent>
      m_capture_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_death_subscription;
};

} // namespace Game::Mission
