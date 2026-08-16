#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

#include <cstddef>
#include <vector>

namespace App::Core {

struct TutorialObservation {
  bool mission_running = false;
  bool victory = false;
  bool defeat = false;

  int selected_troop_count = 0;
  int selected_builder_count = 0;
  int selected_barracks_count = 0;
  int selected_building_count = 0;
  bool commander_selected = false;

  bool move_order_accepted = false;
  bool attack_order_accepted = false;
  bool hold_order_accepted = false;
  bool guard_order_accepted = false;
  bool patrol_order_accepted = false;
  bool gather_order_accepted = false;
  bool build_order_accepted = false;
  QString last_rejection_reason;

  int enemy_troops_defeated = 0;
  int harvested_wood = 0;
  int harvested_stone = 0;
  int harvested_iron = 0;
  int wood = 0;
  int stone = 0;
  int iron = 0;

  int home_count = 0;
  int soldier_count = 0;
  int barracks_manpower = 0;
  bool production_in_progress = false;
  bool construction_preview_active = false;
  bool construction_preview_valid = true;

  bool aura_ready = false;
  bool aura_active = false;
  bool speed_changed = false;
  bool camera_used = false;
  bool objectives_opened = false;

  int waves_cleared = 0;
  bool wave_live = false;
  bool wave_pending = false;
  int enemy_commanders_alive = 0;
};

enum class TutorialStepId {
  SelectTroops,
  MoveTroops,
  AttackScouts,
  GatherWood,
  GatherStoneAndIron,
  BuildHome,
  RecruitSoldier,
  AssembleArmy,
  DefendCamp,
  Stances,
  Commander,
  Camera,
  GameSpeed,
  Objectives,
  Assault,
};

inline constexpr int k_tutorial_scout_count = 2;
inline constexpr int k_tutorial_wood_target = 40;
inline constexpr int k_tutorial_stone_target = 35;
inline constexpr int k_tutorial_iron_target = 30;
inline constexpr int k_tutorial_army_size = 8;

class TutorialDirector : public QObject {
  Q_OBJECT

  Q_PROPERTY(bool active READ active NOTIFY state_changed)
  Q_PROPERTY(bool finished READ finished NOTIFY state_changed)
  Q_PROPERTY(bool visible READ visible WRITE set_visible NOTIFY state_changed)
  Q_PROPERTY(int step_index READ step_index NOTIFY step_changed)
  Q_PROPERTY(int step_count READ step_count CONSTANT)
  Q_PROPERTY(QString step_id READ step_id NOTIFY step_changed)
  Q_PROPERTY(QString title READ title NOTIFY step_changed)
  Q_PROPERTY(QString body READ body NOTIFY step_changed)
  Q_PROPERTY(QString objective READ objective NOTIFY step_changed)
  Q_PROPERTY(QString objective_state READ objective_state NOTIFY state_changed)
  Q_PROPERTY(qreal progress READ progress NOTIFY state_changed)
  Q_PROPERTY(QString progress_text READ progress_text NOTIFY state_changed)
  Q_PROPERTY(QString hint READ hint NOTIFY state_changed)
  Q_PROPERTY(bool step_complete READ step_complete NOTIFY state_changed)
  Q_PROPERTY(bool holds_mission_clock READ holds_mission_clock NOTIFY state_changed)
  Q_PROPERTY(QVariantList steps READ steps NOTIFY state_changed)

public:
  explicit TutorialDirector(QObject* parent = nullptr);

  void begin();
  void end();
  void advance(const TutorialObservation& observation, float real_dt);

  Q_INVOKABLE void start();
  Q_INVOKABLE void skip_step();
  Q_INVOKABLE void replay_step();
  Q_INVOKABLE void continue_step();
  Q_INVOKABLE void restart();
  Q_INVOKABLE void stop();
  Q_INVOKABLE void set_visible(bool visible);
  Q_INVOKABLE void note_objectives_opened();

  [[nodiscard]] auto active() const -> bool { return m_active; }
  [[nodiscard]] auto finished() const -> bool { return m_finished; }
  [[nodiscard]] auto visible() const -> bool { return m_visible; }
  [[nodiscard]] auto step_index() const -> int { return static_cast<int>(m_index); }
  [[nodiscard]] static auto step_count() -> int;
  [[nodiscard]] auto step() const -> TutorialStepId;
  [[nodiscard]] auto step_id() const -> QString;
  [[nodiscard]] auto title() const -> QString;
  [[nodiscard]] auto body() const -> QString;
  [[nodiscard]] auto objective() const -> QString;
  [[nodiscard]] auto objective_state() const -> QString;
  [[nodiscard]] auto progress() const -> qreal { return m_progress; }
  [[nodiscard]] auto progress_text() const -> QString { return m_progress_text; }
  [[nodiscard]] auto hint() const -> QString { return m_hint; }
  [[nodiscard]] auto step_complete() const -> bool { return m_step_complete; }
  [[nodiscard]] auto holds_mission_clock() const -> bool;
  [[nodiscard]] auto steps() const -> QVariantList;

  [[nodiscard]] static auto step_id_name(TutorialStepId id) -> QString;
  [[nodiscard]] static auto step_title(TutorialStepId id) -> QString;
  [[nodiscard]] static auto step_body(TutorialStepId id) -> QString;
  [[nodiscard]] static auto step_objective(TutorialStepId id) -> QString;

signals:
  void start_requested();
  void state_changed();
  void step_changed();
  void step_completed(int index);
  void tutorial_finished();

private:
  struct Baseline {
    int enemy_troops_defeated = 0;
    int harvested_wood = 0;
    int harvested_stone = 0;
    int harvested_iron = 0;
    int home_count = 0;
    int soldier_count = 0;
    int waves_cleared = 0;
    bool aura_seen_ready = false;
    bool captured = false;
  };

  void enter_step(std::size_t index, bool from_replay);
  void mark_step_complete();
  void go_to_next_step();
  [[nodiscard]] auto evaluate(const TutorialObservation& observation,
                              qreal& progress,
                              QString& progress_text) const -> bool;
  [[nodiscard]] auto hint_for(const TutorialObservation& observation) const -> QString;
  void publish(qreal progress, const QString& progress_text, const QString& hint);

  bool m_active = false;
  bool m_finished = false;
  bool m_visible = true;
  std::size_t m_index = 0;
  bool m_step_complete = false;
  float m_complete_timer = 0.0F;
  bool m_objectives_opened = false;
  Baseline m_baseline;
  std::vector<bool> m_done;
  qreal m_progress = -1.0;
  QString m_progress_text;
  QString m_hint;
};

} // namespace App::Core
