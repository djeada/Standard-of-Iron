#include "game/mission/tutorial_director.h"

#include <QJsonArray>
#include <QVariantMap>

#include <algorithm>
#include <array>
#include <optional>

namespace Game::Mission {

namespace {

constexpr float k_step_complete_hold_seconds = 4.0F;
constexpr int k_save_version = 1;

constexpr std::array k_steps = {
    TutorialStepId::SelectTroops,
    TutorialStepId::MoveTroops,
    TutorialStepId::AttackScouts,
    TutorialStepId::GatherWood,
    TutorialStepId::GatherStoneAndIron,
    TutorialStepId::BuildHome,
    TutorialStepId::RecruitSoldier,
    TutorialStepId::AssembleArmy,
    TutorialStepId::DefendCamp,
    TutorialStepId::Stances,
    TutorialStepId::Commander,
    TutorialStepId::Camera,
    TutorialStepId::GameSpeed,
    TutorialStepId::Objectives,
    TutorialStepId::Assault,
};

auto ratio(int done, int target) -> qreal {
  if (target <= 0) {
    return 1.0;
  }
  return std::clamp(static_cast<qreal>(done) / static_cast<qreal>(target), 0.0, 1.0);
}

auto count_text(int done, int target) -> QString {
  return QStringLiteral("%1 / %2").arg(std::min(done, target)).arg(target);
}

} // namespace

TutorialDirector::TutorialDirector(QObject* parent)
    : QObject(parent)
    , m_done(k_steps.size(), false) {
}

auto TutorialDirector::step_count() -> int {
  return static_cast<int>(k_steps.size());
}

auto TutorialDirector::step() const -> TutorialStepId {
  return k_steps[std::min(m_index, k_steps.size() - 1)];
}

auto TutorialDirector::step_id() const -> QString {
  return step_id_name(step());
}

auto TutorialDirector::title() const -> QString {
  return step_title(step());
}

auto TutorialDirector::body() const -> QString {
  return step_body(step());
}

auto TutorialDirector::objective() const -> QString {
  return step_objective(step());
}

auto TutorialDirector::objective_state() const -> QString {
  return m_step_complete ? QStringLiteral("complete") : QStringLiteral("active");
}

auto TutorialDirector::holds_mission_clock() const -> bool {
  if (!m_active || m_finished) {
    return false;
  }
  return m_index <
         static_cast<std::size_t>(std::distance(
             k_steps.begin(),
             std::find(k_steps.begin(), k_steps.end(), TutorialStepId::DefendCamp)));
}

auto TutorialDirector::steps() const -> QVariantList {
  QVariantList list;
  for (std::size_t i = 0; i < k_steps.size(); ++i) {
    QVariantMap entry;
    entry["id"] = step_id_name(k_steps[i]);
    entry["title"] = step_title(k_steps[i]);
    entry["objective"] = step_objective(k_steps[i]);
    QString state = QStringLiteral("pending");
    if (m_done[i]) {
      state = QStringLiteral("complete");
    } else if (m_active && i == m_index) {
      state = m_step_complete ? QStringLiteral("complete") : QStringLiteral("active");
    }
    entry["state"] = state;
    entry["current"] = m_active && i == m_index;
    list.append(entry);
  }
  return list;
}

void TutorialDirector::begin() {
  m_active = true;
  m_finished = false;
  m_visible = true;
  m_objectives_opened = false;
  std::fill(m_done.begin(), m_done.end(), false);
  enter_step(0, false);
  emit state_changed();
}

void TutorialDirector::end() {
  if (!m_active && !m_finished) {
    return;
  }
  m_active = false;
  m_finished = false;
  m_step_complete = false;
  m_index = 0;
  std::fill(m_done.begin(), m_done.end(), false);
  publish(-1.0, {}, {}, {});
  emit state_changed();
  emit step_changed();
}

void TutorialDirector::stop() {
  if (!m_active) {
    return;
  }
  m_active = false;
  m_step_complete = false;
  publish(-1.0, {}, {}, {});
  emit state_changed();
}

void TutorialDirector::start() {
  emit start_requested();
}

void TutorialDirector::restart() {
  begin();
}

void TutorialDirector::set_visible(bool visible) {
  if (m_visible == visible) {
    return;
  }
  m_visible = visible;
  emit state_changed();
}

void TutorialDirector::note_objectives_opened() {
  m_objectives_opened = true;
}

void TutorialDirector::enter_step(std::size_t index, bool from_replay) {
  m_index = std::min(index, k_steps.size() - 1);
  m_step_complete = false;
  m_complete_timer = 0.0F;
  m_baseline = Baseline{};
  if (from_replay) {
    m_done[m_index] = false;
  }
  if (step() == TutorialStepId::Objectives) {
    m_objectives_opened = false;
  }
  m_progress = -1.0;
  m_progress_text.clear();
  m_hint.clear();
  m_focus = TutorialFocus{};
  if (!m_focus_points.isEmpty()) {
    m_focus_points.clear();
    emit focus_points_changed();
  }
  emit step_changed();
  emit state_changed();
}

void TutorialDirector::mark_step_complete() {
  if (m_step_complete) {
    return;
  }
  m_step_complete = true;
  m_complete_timer = 0.0F;
  m_done[m_index] = true;
  emit step_completed(static_cast<int>(m_index));
  emit state_changed();
}

void TutorialDirector::go_to_next_step() {
  if (m_index + 1 >= k_steps.size()) {
    m_done[m_index] = true;
    m_finished = true;
    m_active = false;
    m_step_complete = true;
    emit tutorial_finished();
    emit state_changed();
    return;
  }
  enter_step(m_index + 1, false);
}

void TutorialDirector::skip_step() {
  if (!m_active) {
    return;
  }
  m_done[m_index] = true;
  go_to_next_step();
}

void TutorialDirector::replay_step() {
  if (!m_active) {
    return;
  }
  enter_step(m_index, true);
}

void TutorialDirector::continue_step() {
  if (!m_active || !m_step_complete) {
    return;
  }
  go_to_next_step();
}

auto TutorialDirector::serialize() const -> QJsonObject {
  if (!m_active && !m_finished) {
    return {};
  }
  QJsonObject state;
  state["version"] = k_save_version;
  state["finished"] = m_finished;
  state["visible"] = m_visible;
  state["step"] = step_id_name(step());
  state["step_complete"] = m_step_complete;
  state["objectives_opened"] = m_objectives_opened;
  QJsonArray done;
  for (std::size_t i = 0; i < k_steps.size(); ++i) {
    if (m_done[i]) {
      done.append(step_id_name(k_steps[i]));
    }
  }
  state["done"] = done;
  if (m_baseline.captured) {
    QJsonObject baseline;
    baseline["enemy_units_defeated"] = m_baseline.enemy_units_defeated;
    baseline["harvested_wood"] = m_baseline.harvested_wood;
    baseline["harvested_stone"] = m_baseline.harvested_stone;
    baseline["harvested_iron"] = m_baseline.harvested_iron;
    baseline["home_count"] = m_baseline.home_count;
    baseline["soldier_count"] = m_baseline.soldier_count;
    baseline["waves_cleared"] = m_baseline.waves_cleared;
    state["baseline"] = baseline;
  }
  return state;
}

void TutorialDirector::restore(const QJsonObject& state, int waves_cleared) {
  if (!m_active) {
    return;
  }
  const auto index_of = [](const QString& id) -> std::optional<std::size_t> {
    for (std::size_t i = 0; i < k_steps.size(); ++i) {
      if (step_id_name(k_steps[i]) == id) {
        return i;
      }
    }
    return std::nullopt;
  };

  if (state.isEmpty() || state.value("version").toInt(0) > k_save_version) {
    if (waves_cleared > 0) {
      const auto defend = index_of(step_id_name(TutorialStepId::DefendCamp));
      std::fill(m_done.begin(),
                m_done.begin() + static_cast<std::ptrdiff_t>(*defend) + 1,
                true);
      enter_step(*defend + 1, false);
    }
    return;
  }

  if (state.value("finished").toBool()) {
    std::fill(m_done.begin(), m_done.end(), true);
    m_index = k_steps.size() - 1;
    m_finished = true;
    m_active = false;
    m_step_complete = true;
    publish(-1.0, {}, {}, {});
    emit step_changed();
    emit state_changed();
    return;
  }

  std::fill(m_done.begin(), m_done.end(), false);
  for (const QJsonValue id : state.value("done").toArray()) {
    if (const auto index = index_of(id.toString())) {
      m_done[*index] = true;
    }
  }
  const auto index = index_of(state.value("step").toString()).value_or(0);
  enter_step(index, false);
  m_visible = state.value("visible").toBool(true);
  m_objectives_opened = state.value("objectives_opened").toBool(false);

  const QJsonObject baseline = state.value("baseline").toObject();
  if (!baseline.isEmpty()) {
    m_baseline.captured = true;

    m_baseline.enemy_units_defeated = baseline.value("enemy_units_defeated").toInt();
    m_baseline.harvested_wood = baseline.value("harvested_wood").toInt();
    m_baseline.harvested_stone = baseline.value("harvested_stone").toInt();
    m_baseline.harvested_iron = baseline.value("harvested_iron").toInt();
    m_baseline.home_count = baseline.value("home_count").toInt();
    m_baseline.soldier_count = baseline.value("soldier_count").toInt();
    m_baseline.waves_cleared = baseline.value("waves_cleared").toInt();
  }
  if (state.value("step_complete").toBool()) {
    mark_step_complete();
  }
  emit state_changed();
}

void TutorialDirector::publish(qreal progress,
                               const QString& progress_text,
                               const QString& hint,
                               const TutorialFocus& focus) {
  const bool changed = !qFuzzyCompare(1.0 + m_progress, 1.0 + progress) ||
                       m_progress_text != progress_text || m_hint != hint ||
                       !(m_focus == focus);
  const bool target_changed = m_focus.target != focus.target;
  m_progress = progress;
  m_progress_text = progress_text;
  m_hint = hint;
  m_focus = focus;
  if (target_changed && !m_focus_points.isEmpty()) {
    m_focus_points.clear();
    emit focus_points_changed();
  }
  if (changed) {
    emit state_changed();
  }
}

void TutorialDirector::set_focus_points(const QVariantList& points) {
  if (m_focus_points == points) {
    return;
  }
  m_focus_points = points;
  emit focus_points_changed();
}

auto TutorialDirector::focus_target() const -> QString {
  return focus_target_name(m_focus.target);
}

void TutorialDirector::advance(const TutorialObservation& observation, float real_dt) {
  if (!m_active || !observation.mission_running) {
    return;
  }
  if (observation.defeat) {
    stop();
    return;
  }

  if (!m_baseline.captured) {
    m_baseline.captured = true;
    m_baseline.enemy_units_defeated = observation.enemy_units_defeated;
    m_baseline.harvested_wood = observation.harvested_wood;
    m_baseline.harvested_stone = observation.harvested_stone;
    m_baseline.harvested_iron = observation.harvested_iron;
    m_baseline.home_count = observation.home_count;
    m_baseline.soldier_count = observation.soldier_count;
    m_baseline.waves_cleared = observation.waves_cleared;
  }

  if (m_step_complete) {
    m_complete_timer += std::max(0.0F, real_dt);
    if (m_complete_timer >= k_step_complete_hold_seconds) {
      go_to_next_step();
    }
    return;
  }

  qreal progress = -1.0;
  QString progress_text;
  const bool completed = evaluate(observation, progress, progress_text);
  if (completed) {
    publish(1.0, progress_text, {}, {});
    mark_step_complete();
    if (step() == TutorialStepId::Assault) {

      go_to_next_step();
    }
    return;
  }
  publish(progress, progress_text, hint_for(observation), focus_for(observation));
}

auto TutorialDirector::evaluate(const TutorialObservation& o,
                                qreal& progress,
                                QString& progress_text) const -> bool {
  switch (step()) {
  case TutorialStepId::SelectTroops:
    return o.selected_troop_count > 0;

  case TutorialStepId::MoveTroops:
    return o.move_order_accepted;

  case TutorialStepId::AttackScouts: {
    const int killed = o.enemy_units_defeated - m_baseline.enemy_units_defeated;
    progress = ratio(killed, k_tutorial_scout_count);
    progress_text = count_text(killed, k_tutorial_scout_count);
    return killed >= k_tutorial_scout_count;
  }

  case TutorialStepId::GatherWood: {
    const int wood = o.harvested_wood - m_baseline.harvested_wood;
    progress = ratio(wood, k_tutorial_wood_target);
    progress_text = count_text(wood, k_tutorial_wood_target);
    return wood >= k_tutorial_wood_target;
  }

  case TutorialStepId::GatherStoneAndIron: {
    const int stone = o.harvested_stone - m_baseline.harvested_stone;
    const int iron = o.harvested_iron - m_baseline.harvested_iron;
    progress =
        (ratio(stone, k_tutorial_stone_target) + ratio(iron, k_tutorial_iron_target)) *
        0.5;
    progress_text = tr("stone %1 / %2 · iron %3 / %4")
                        .arg(std::min(stone, k_tutorial_stone_target))
                        .arg(k_tutorial_stone_target)
                        .arg(std::min(iron, k_tutorial_iron_target))
                        .arg(k_tutorial_iron_target);
    return stone >= k_tutorial_stone_target && iron >= k_tutorial_iron_target;
  }

  case TutorialStepId::BuildHome:
    progress = o.construction_preview_active ? 0.5 : 0.0;
    progress_text.clear();
    return o.home_count > m_baseline.home_count;

  case TutorialStepId::RecruitSoldier:
    progress = o.production_in_progress ? 0.5 : 0.0;
    progress_text.clear();
    return o.soldier_count > m_baseline.soldier_count;

  case TutorialStepId::AssembleArmy:
    progress = ratio(o.soldier_count, k_tutorial_army_size);
    progress_text = count_text(o.soldier_count, k_tutorial_army_size);
    return o.soldier_count >= k_tutorial_army_size;

  case TutorialStepId::DefendCamp:
    progress = o.wave_live ? 0.5 : (o.wave_pending ? 0.1 : 0.0);
    progress_text.clear();
    return o.waves_cleared > m_baseline.waves_cleared;

  case TutorialStepId::Stances:
    return o.hold_order_accepted || o.guard_order_accepted || o.patrol_order_accepted;

  case TutorialStepId::Commander:
    return o.aura_active;

  case TutorialStepId::Camera:
    return o.camera_used;

  case TutorialStepId::GameSpeed:
    return o.speed_changed;

  case TutorialStepId::Objectives:
    return m_objectives_opened || o.objectives_opened;

  case TutorialStepId::Assault:
    return o.victory;
  }
  return false;
}

} // namespace Game::Mission
