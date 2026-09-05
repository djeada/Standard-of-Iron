#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector3D>

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "game/core/event_manager.h"
#include "game/map/mission_definition.h"
#include "game/map/mission_stage_tracker.h"
#include "game/mission/commander_speaker_roster.h"
#include "game/mission/commander_voice_bank.h"

namespace Game::Mission {

using StructurePositionLookup =
    std::function<std::optional<QVector3D>(Engine::Core::EntityID)>;

using RelationshipLookup = std::function<bool(int, int)>;

struct CommanderMessageCue {
  QString id;

  QString speaker_id;

  QString speaker_name;

  QString speaker_role;

  QString nation;

  QString relationship;

  int speaker_owner_id = -1;

  QString pose;

  QString text;
  QString voice_cue;
  float duration = k_default_commander_message_seconds;

  const char* text_context = nullptr;

  bool holds_outcome = false;
};

struct CommanderMessageScript {
  std::vector<CommanderMessage> mission_lines;
  std::vector<CommanderSpeaker> speakers;
  const CommanderVoiceLibrary* voices = nullptr;
  CommanderVoicesPolicy policy;
};

struct CommanderMessageFact {
  CommanderMessageTrigger trigger = CommanderMessageTrigger::MissionStart;
  int subject_owner_id = -1;
  int actor_owner_id = -1;
  QString subject_type;
  QString nation;
  std::optional<Engine::Core::EntityID> structure_id;
  std::optional<bool> final_wave;
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

  void configure(const CommanderMessageScript& script,
                 int local_owner_id,
                 const MissionPositionToWorld& to_world);

  void set_structure_position_lookup(StructurePositionLookup lookup) {
    m_structure_position = std::move(lookup);
  }
  void set_relationship_lookup(RelationshipLookup lookup) {
    m_are_allies = std::move(lookup);
  }
  void clear();

  [[nodiscard]] auto has_messages() const -> bool { return !m_rules.empty(); }

  [[nodiscard]] auto speaker_ids() const -> const QStringList& { return m_speaker_ids; }

  void notify_mission_start();
  void notify_victory();
  void notify_defeat();

  void notify_fact(const CommanderMessageFact& fact);

  auto update(float delta_time) -> bool;

  [[nodiscard]] auto has_active() const -> bool { return m_active.has_value(); }
  [[nodiscard]] auto active() const -> const CommanderMessageCue&;
  [[nodiscard]] auto active_remaining() const -> float { return m_active_remaining; }

  [[nodiscard]] auto elapsed() const -> float { return m_elapsed; }

  auto dismiss_active() -> bool;

  [[nodiscard]] auto serialize() const -> QJsonObject;
  void restore(const QJsonObject& state);

private:
  struct Rule {
    CommanderMessage authored;
    CommanderMessageCue cue;
    std::optional<QVector3D> world_target;
    bool fired = false;

    int speaker_owner_id = -1;

    bool generic = false;

    int variant_group = -1;
    float last_fired_at = -1.0e9F;
  };

  struct Pending {
    std::size_t rule_index = 0;
    float delay_remaining = 0.0F;

    std::optional<float> expires_in;
  };

  struct SpeakerTriggerKey {
    int owner_id;
    CommanderMessageTrigger trigger;
    auto operator<(const SpeakerTriggerKey& other) const -> bool {
      return std::tie(owner_id, trigger) < std::tie(other.owner_id, other.trigger);
    }
  };

  void add_mission_rules(const std::vector<CommanderMessage>& lines,
                         const std::vector<CommanderSpeaker>& speakers,
                         const MissionPositionToWorld& to_world);
  void add_bank_rules(const CommanderMessageScript& script,
                      const MissionPositionToWorld& to_world);
  auto make_rule(const CommanderMessage& authored,
                 const MissionPositionToWorld& to_world) -> Rule;

  void subscribe();
  void unsubscribe();

  void queue_fact(const CommanderMessageFact& fact);
  [[nodiscard]] auto rule_matches(const Rule& rule,
                                  const CommanderMessageFact& fact) const -> bool;
  [[nodiscard]] auto matches_owner(const CommanderMessageCondition& condition,
                                   int owner_id,
                                   int by_owner_id) const -> bool;
  [[nodiscard]] auto matches_role(CommanderMessageRole role,
                                  int owner_id,
                                  int speaker_owner_id) const -> bool;
  [[nodiscard]] auto rule_is_available(std::size_t index) const -> bool;
  [[nodiscard]] auto is_queued(std::size_t index) const -> bool;
  [[nodiscard]] auto pick_variant(const std::vector<std::size_t>& group) const
      -> std::optional<std::size_t>;
  void queue_rule(std::size_t index);
  void drop_pending_chatter();

  auto promote_next() -> bool;
  void end_active();
  [[nodiscard]] auto is_chatter(const Rule& rule) const -> bool;

  std::vector<Rule> m_rules;
  QStringList m_speaker_ids;
  std::vector<Pending> m_pending;

  std::optional<CommanderMessageCue> m_active;
  std::optional<std::size_t> m_active_rule;
  float m_active_remaining = 0.0F;

  float m_elapsed = 0.0F;
  float m_last_line_ended_at = -1.0e9F;
  bool m_outcome_reached = false;

  std::map<int, int> m_chatter_budget;
  std::map<int, int> m_chatter_spent;
  std::map<SpeakerTriggerKey, float> m_speaker_trigger_fired_at;

  int m_local_owner_id = 1;
  StructurePositionLookup m_structure_position;
  RelationshipLookup m_are_allies;

  mutable std::mutex m_inbox_mutex;
  std::vector<CommanderMessageFact> m_inbox;

  Engine::Core::ScopedEventSubscription<Engine::Core::BarrackCapturedEvent>
      m_capture_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_death_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::AiAttackLaunchedEvent>
      m_attack_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::OwnerUnderAttackEvent>
      m_under_attack_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::OwnersFirstContactEvent>
      m_contact_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::OwnerHeavyLossesEvent>
      m_losses_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::OwnerNearDefeatEvent>
      m_near_defeat_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::OwnerEliminatedEvent>
      m_eliminated_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::MissionWaveIncomingEvent>
      m_wave_incoming_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::MissionWaveClearedEvent>
      m_wave_cleared_subscription;
};

} // namespace Game::Mission
