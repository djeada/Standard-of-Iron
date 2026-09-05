#pragma once

#include <QJsonObject>

#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "game/core/event_manager.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {
class AISystem;
}

namespace Game::Mission {

class AttackPlanSource {
public:
  struct AttackPlan {
    bool committed = false;
    float committed_at = -1000.0F;
    Engine::Core::EntityID target_id = 0;
  };

  AttackPlanSource() = default;
  AttackPlanSource(const AttackPlanSource&) = default;
  AttackPlanSource(AttackPlanSource&&) = default;
  auto operator=(const AttackPlanSource&) -> AttackPlanSource& = default;
  auto operator=(AttackPlanSource&&) -> AttackPlanSource& = default;
  virtual ~AttackPlanSource() = default;

  [[nodiscard]] virtual auto
  attack_plan(int owner_id) const -> std::optional<AttackPlan> = 0;
};

class AiSystemAttackPlanSource final : public AttackPlanSource {
public:
  explicit AiSystemAttackPlanSource(const Game::Systems::AISystem* ai)
      : m_ai(ai) {}
  [[nodiscard]] auto
  attack_plan(int owner_id) const -> std::optional<AttackPlan> override;

private:
  const Game::Systems::AISystem* m_ai = nullptr;
};

struct CommanderVoiceTuning {
  float poll_interval_seconds = 0.5F;

  int under_attack_damage = 120;
  int under_attack_hits = 6;
  float under_attack_window_seconds = 10.0F;
  float under_attack_cooloff_seconds = 25.0F;

  float losses_window_seconds = 30.0F;
  int losses_minimum = 6;
  float losses_share_of_peak = 0.25F;
  float losses_rearm_seconds = 60.0F;

  int near_defeat_units = 3;
  int near_defeat_peak_units = 10;
};

class CommanderVoiceObserver {
public:
  using Tuning = CommanderVoiceTuning;

  CommanderVoiceObserver() = default;
  ~CommanderVoiceObserver();

  CommanderVoiceObserver(const CommanderVoiceObserver&) = delete;
  auto operator=(const CommanderVoiceObserver&) -> CommanderVoiceObserver& = delete;
  CommanderVoiceObserver(CommanderVoiceObserver&&) = delete;
  auto operator=(CommanderVoiceObserver&&) -> CommanderVoiceObserver& = delete;

  void configure(std::vector<int> watched_owners,
                 int local_owner_id,
                 Tuning tuning = Tuning());
  void clear();

  [[nodiscard]] auto is_configured() const -> bool { return !m_owners.empty(); }

  void
  update(Engine::Core::World& world, const AttackPlanSource* plans, float delta_time);

  [[nodiscard]] auto elapsed() const -> float { return m_elapsed; }

  [[nodiscard]] auto serialize() const -> QJsonObject;
  void restore(const QJsonObject& state);

private:
  struct Hit {
    float at = 0.0F;
    int damage = 0;
  };

  struct Assault {
    std::deque<Hit> hits;
    bool hot = false;
    float last_hit_at = -1000.0F;
  };

  struct Death {
    float at = 0.0F;
    int killer_owner_id = -1;
  };

  struct OwnerState {
    float last_committed_at = -1000.0F;
    std::map<int, Assault> assaults;
    std::deque<Death> deaths;
    float losses_fired_at = -1000.0F;
    int peak_units = 0;
    int peak_structures = 0;
    bool had_anything = false;
    bool near_defeat_fired = false;
    bool eliminated = false;
    int last_killer_owner_id = -1;
  };

  struct BuildingHitFact {
    int owner_id;
    int attacker_owner_id;
    int damage;
  };
  struct ContactFact {
    int attacker_owner_id;
    int target_owner_id;
  };
  struct DeathFact {
    int owner_id;
    int killer_owner_id;
    bool is_building;
  };

  void subscribe();
  void unsubscribe();
  void drain_inboxes();
  void note_building_hit(const BuildingHitFact& fact);
  void note_contact(const ContactFact& fact);
  void note_death(const DeathFact& fact);
  void poll(Engine::Core::World& world, const AttackPlanSource* plans);
  [[nodiscard]] auto state_for(int owner_id) -> OwnerState*;

  std::vector<int> m_owners;
  int m_local_owner_id = 1;
  Tuning m_tuning;
  float m_elapsed = 0.0F;
  float m_poll_accumulator = 0.0F;

  std::map<int, OwnerState> m_states;
  std::set<std::pair<int, int>> m_contacted;

  mutable std::mutex m_inbox_mutex;
  std::vector<BuildingHitFact> m_building_hits;
  std::vector<ContactFact> m_contacts;
  std::vector<DeathFact> m_deaths;

  Engine::Core::ScopedEventSubscription<Engine::Core::BuildingAttackedEvent>
      m_building_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::CombatHitEvent>
      m_combat_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_death_subscription;
};

} // namespace Game::Mission
