#include "app/mission/mission_runtime.h"

#include <QVariant>

#include <algorithm>
#include <cmath>
#include <utility>

#include "app/viewmodels/mission_view_model.h"
#include "app/viewmodels/wave_view_model.h"
#include "app/world/minimap_manager.h"
#include "game/audio/audio_cues.h"
#include "game/core/component_gameplay.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/mission/campaign_manager.h"
#include "game/session/session_context.h"
#include "game/systems/player_feedback.h"
#include "game/systems/victory_service.h"
#include "game/units/spawn_type.h"
#include "game/util/asset_text.h"

namespace App::Mission {

namespace {

constexpr float k_stage_poll_seconds = 0.25F;
constexpr float k_announcement_spacing_seconds = 4.5F;

auto treasury_anchor(Engine::Core::World& world,
                     int owner_id) -> Engine::Core::EntityID {
  for (auto [id, building, unit] :
       world.view<Engine::Core::BuildingComponent, Engine::Core::UnitComponent>()) {
    (void)building;
    if (unit.owner_id == owner_id &&
        unit.spawn_type == Game::Units::SpawnType::Barracks && unit.health > 0 &&
        !world.has<Engine::Core::PendingRemovalComponent>(id)) {
      return id;
    }
  }
  return Engine::Core::NULL_ENTITY;
}

auto has_minimap(const MissionBinding& binding) -> bool {
  return binding.minimap != nullptr && binding.minimap->has_minimap();
}

} // namespace

MissionRuntime::MissionRuntime(App::ViewModels::MissionViewModel* mission_view_model,
                               App::ViewModels::WaveViewModel* wave_view_model,
                               Announce announce)
    : m_mission_view_model(mission_view_model)
    , m_wave_view_model(wave_view_model)
    , m_announce(std::move(announce))
    , m_setup(std::make_unique<Game::Mission::MissionSetupCoordinator>()) {
}

auto MissionRuntime::resolve_difficulty(const CampaignManager* campaign,
                                        const QVariantList& player_configs)
    -> Game::Mission::MatchDifficulty {
  Game::Mission::MatchDifficulty difficulty;
  if (campaign != nullptr && campaign->current_mission_context().has_mission()) {
    difficulty.set_baseline(campaign->current_mission_context().difficulty);
    return difficulty;
  }

  for (const QVariant& config_value : player_configs) {
    const QVariantMap config = config_value.toMap();
    if (config.value(QStringLiteral("isHuman"), false).toBool()) {
      continue;
    }
    const QString id = config.value(QStringLiteral("difficulty")).toString();
    if (id.isEmpty()) {
      continue;
    }
    difficulty.set_owner(config.value(QStringLiteral("player_id"), -1).toInt(), id);
  }
  return difficulty;
}

void MissionRuntime::reset() {
  m_waves.reset();
  m_stages.clear();
  m_stage_poll_accumulator = 0.0F;
  if (m_wave_view_model != nullptr) {
    m_wave_view_model->clear();
  }
  if (m_mission_view_model != nullptr) {
    m_mission_view_model->clear();
  }
}

auto MissionRuntime::wave_binding(const MissionBinding& binding)
    -> Game::Mission::MissionWaveBinding {
  return {.world = binding.world,
          .level = binding.level,
          .campaign = binding.campaign,
          .victory_service = binding.victory_service,
          .local_owner_id = binding.local_owner_id};
}

auto MissionRuntime::stage_facts() -> Game::Mission::StageWorldFacts {
  return {.elapsed_seconds = m_waves.elapsed(),
          .cleared_wave_count = m_waves.director().cleared_wave_count()};
}

auto MissionRuntime::bind_setup(const MissionBinding& binding, int& selected_player_id)
    -> std::optional<Game::Mission::MissionSetupEffects> {
  if (binding.world == nullptr || binding.campaign == nullptr) {
    return std::nullopt;
  }

  std::vector<Game::Mission::PendingMissionWave> waves;
  auto effects = m_setup->apply_mission_setup({*binding.world,
                                               *binding.campaign,
                                               *binding.level,
                                               selected_player_id,
                                               binding.local_owner_id,
                                               waves,
                                               &m_difficulty});
  std::vector<Game::Mission::PendingMissionEvent> events;
  if (binding.campaign->current_mission_definition().has_value()) {
    events = Game::Mission::build_pending_mission_events(
        *binding.campaign->current_mission_definition());
  }
  m_waves.bind_after_setup(wave_binding(binding), std::move(waves), std::move(events));
  return effects;
}

void MissionRuntime::apply_skirmish_commander_setup(
    const MissionBinding& binding, const QVariantList& player_configs) {
  if (binding.world == nullptr) {
    return;
  }

  const auto effects = m_setup->apply_skirmish_commander_setup(
      {*binding.world, binding.campaign, *binding.level, binding.local_owner_id},
      player_configs);
  for (const auto& announcement : effects.mission_announcements) {
    queue_announcement(announcement);
  }
}

auto MissionRuntime::configure_stages(const MissionBinding& binding) -> bool {
  m_stages.clear();
  m_stage_poll_accumulator = 0.0F;

  if (binding.campaign == nullptr ||
      !binding.campaign->current_mission_definition().has_value()) {
    publish_stages(binding);
    return false;
  }

  const auto& mission = *binding.campaign->current_mission_definition();
  m_stages.configure(mission,
                     binding.local_owner_id,
                     Game::Mission::make_mission_position_to_world(*binding.level));

  if (binding.session != nullptr && m_stages.has_stages()) {
    m_stages.update(*binding.session, stage_facts());
  }
  publish_stages(binding);
  return true;
}

void MissionRuntime::restore_stages(const MissionBinding& binding,
                                    const QJsonObject& stage_state) {
  m_stages.restore(stage_state);
  publish_stages(binding);
}

void MissionRuntime::restore_waves(const MissionBinding& binding,
                                   const QJsonObject& wave_state) {
  m_waves.restore(wave_binding(binding), wave_state);
}

auto MissionRuntime::advance_waves(const MissionBinding& binding,
                                   float dt,
                                   bool tutorial_holds_clock,
                                   const std::function<void()>& on_reward_granted)
    -> WaveFrameResult {
  WaveFrameResult result;
  const auto effects = m_waves.advance(wave_binding(binding), dt, tutorial_holds_clock);

  for (const auto& announcement : effects.announcements) {
    queue_announcement(announcement);
  }
  for (const auto& beat : effects.incoming_waves) {
    Engine::Core::EventManager::instance().publish(
        Engine::Core::MissionWaveIncomingEvent(
            beat.owner_id, beat.phase_index, beat.phase_count, beat.final_wave));
  }
  for (const auto& beat : effects.cleared_waves) {
    Engine::Core::EventManager::instance().publish(
        Engine::Core::MissionWaveClearedEvent(
            beat.owner_id, beat.phase_index, beat.phase_count, beat.final_wave));
  }
  for (const auto& cue : effects.audio_cues) {
    Game::Audio::play_cue(cue.toStdString());
  }
  if (effects.reward_granted) {
    Game::Systems::grant_resources(
        binding.local_owner_id,
        treasury_anchor(*binding.world, binding.local_owner_id),
        effects.reward);
    result.reward_granted = true;
    on_reward_granted();
  }
  if (effects.wave_status_changed) {
    publish_wave_status(binding);
  }
  result.owner_info_changed = effects.owner_info_changed;
  return result;
}

void MissionRuntime::advance_stages(const MissionBinding& binding, float dt) {
  if (!m_stages.has_stages() || binding.session == nullptr) {
    return;
  }

  m_stage_poll_accumulator += dt;
  if (m_stage_poll_accumulator < k_stage_poll_seconds) {
    return;
  }
  m_stage_poll_accumulator = 0.0F;

  if (m_stages.update(*binding.session, stage_facts())) {
    publish_stages(binding);
  }
}

void MissionRuntime::publish_deadline(const MissionBinding& binding) {
  if (m_mission_view_model == nullptr) {
    return;
  }
  const float remaining = binding.victory_service != nullptr
                              ? binding.victory_service->seconds_until_deadline()
                              : -1.0F;
  m_mission_view_model->set_seconds_until_deadline(
      remaining < 0.0F ? -1.0 : std::floor(static_cast<double>(remaining)));
}

void MissionRuntime::publish_optional_objectives(const MissionBinding& binding) {
  QVariantList optional;
  if (binding.victory_service != nullptr) {
    for (const auto& objective : binding.victory_service->optional_objectives()) {
      QVariantMap entry;
      entry["index"] = objective.source_index;
      entry["detail"] = objective.detail;
      entry["progress"] = objective.progress;
      entry["required"] = objective.required;
      entry["fraction"] = objective.fraction;
      entry["complete"] = objective.complete;
      optional.append(entry);
    }
  }
  m_mission_view_model->set_optional(optional);
}

void MissionRuntime::publish_stages(const MissionBinding& binding) {
  if (m_mission_view_model == nullptr) {
    return;
  }
  publish_optional_objectives(binding);
  if (!m_stages.has_stages()) {
    publish_victory_objectives(binding);
    return;
  }

  const bool minimap_ready = has_minimap(binding);

  QVariantList stages;
  int index = 0;
  for (const auto& status : m_stages.stages()) {
    QVariantMap entry;
    entry["id"] = status.id;
    entry["index"] = index;
    entry["type"] = status.type;
    entry["title"] = Game::Util::tr_asset(Game::Util::k_missions_context, status.title);
    entry["description"] =
        Game::Util::tr_asset(Game::Util::k_missions_context, status.description);
    entry["hint"] = Game::Util::tr_asset(Game::Util::k_missions_context, status.hint);
    entry["detail"] = status.detail;
    entry["compact_detail"] = status.compact_detail;
    entry["fraction"] = status.fraction >= 0.0 ? status.fraction
                                               : static_cast<double>(status.progress) /
                                                     std::max(1, status.required);
    entry["progress"] = status.progress;
    entry["required"] = status.required;
    entry["complete"] = status.complete;
    entry["active"] = status.active;
    entry["has_target"] = status.has_target;
    entry["target_structure_present"] = status.target_structure_present;
    entry["target_structure_is_local"] = status.target_structure_is_local;
    if (status.has_target) {
      entry["world_x"] = status.target.x();
      entry["world_z"] = status.target.z();
      if (minimap_ready) {
        float nx = 0.0F;
        float ny = 0.0F;
        (void)binding.minimap->world_to_normalized(
            status.target.x(), status.target.z(), nx, ny);
        entry["nx"] = std::clamp(nx, 0.0F, 1.0F);
        entry["ny"] = std::clamp(ny, 0.0F, 1.0F);
      }
    }
    stages.append(entry);
    ++index;
  }

  m_mission_view_model->set_stages(stages);
}

void MissionRuntime::publish_victory_objectives(const MissionBinding& binding) {
  if (binding.victory_service == nullptr) {
    m_mission_view_model->clear();
    return;
  }

  const auto objectives = binding.victory_service->objectives();
  QVariantList stages;
  int index = 0;
  for (const auto& objective : objectives) {
    if (objective.description.isEmpty()) {
      ++index;
      continue;
    }
    QVariantMap entry;
    entry["id"] = objective.id;
    entry["index"] = index;
    entry["type"] = QStringLiteral("victory_condition");
    entry["title"] =
        Game::Util::tr_asset(Game::Util::k_missions_context, objective.description);
    entry["description"] = entry["title"];
    entry["hint"] = QString();
    entry["detail"] = objective.detail;
    entry["compact_detail"] = objective.compact_detail;
    entry["fraction"] = objective.fraction;
    entry["progress"] = objective.progress;
    entry["required"] = objective.required;
    entry["complete"] = objective.complete;
    entry["has_target"] = false;
    entry["target_structure_present"] = false;
    entry["target_structure_is_local"] = false;
    stages.append(entry);
    ++index;
  }

  if (stages.isEmpty()) {
    m_mission_view_model->clear();
    return;
  }

  m_mission_view_model->set_stages(stages, true);
}

void MissionRuntime::publish_wave_status(const MissionBinding& binding) {
  if (m_wave_view_model == nullptr) {
    return;
  }

  QVariantMap status = m_waves.status();
  QVariantList alerts = status.value("alerts").toList();
  if (!alerts.isEmpty() && has_minimap(binding)) {
    QVariantList normalized;
    for (const auto& value : alerts) {
      QVariantMap alert = value.toMap();
      float nx = 0.0F;
      float ny = 0.0F;
      (void)binding.minimap->world_to_normalized(
          alert.value("x").toFloat(), alert.value("z").toFloat(), nx, ny);
      alert["nx"] = std::clamp(nx, 0.0F, 1.0F);
      alert["ny"] = std::clamp(ny, 0.0F, 1.0F);
      normalized.append(alert);
    }
    status["alerts"] = normalized;
  }

  m_wave_view_model->set_status(status);
}

void MissionRuntime::queue_announcement(const QString& text) {
  if (text.isEmpty()) {
    return;
  }
  if (m_announcement_cooldown <= 0.0F && m_pending_announcements.isEmpty()) {
    m_announcement_cooldown = k_announcement_spacing_seconds;
    m_announce(text);
    return;
  }
  if (m_pending_announcements.contains(text)) {
    return;
  }
  m_pending_announcements.append(text);
}

void MissionRuntime::flush_announcements(float dt) {
  m_announcement_cooldown =
      std::max(0.0F, m_announcement_cooldown - std::max(dt, 0.0F));
  if (m_announcement_cooldown > 0.0F || m_pending_announcements.isEmpty()) {
    return;
  }
  const QString text = m_pending_announcements.takeFirst();
  m_announcement_cooldown = k_announcement_spacing_seconds;
  m_announce(text);
}

} // namespace App::Mission
