#include "mission_stage_tracker.h"

#include <QJsonArray>

#include <algorithm>
#include <cmath>
#include <limits>

#include "game/core/component_commander.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "units/spawn_type.h"

namespace Game::Mission {

namespace {

constexpr float k_default_reach_radius = 6.0F;

constexpr float k_target_structure_match_radius = 30.0F;

struct StageTally {
  int local_owned_structures = 0;
  int local_captured_structures = 0;
  int enemy_structures = 0;
  int local_units_in_range = 0;
  float target_structure_distance_sq = std::numeric_limits<float>::max();
  bool target_structure_is_local = false;
};

auto matches_type(const std::vector<QString>& wanted, const QString& type) -> bool {
  return std::any_of(wanted.begin(), wanted.end(), [&type](const QString& candidate) {
    return candidate.compare(type, Qt::CaseInsensitive) == 0;
  });
}

} // namespace

void MissionStageTracker::configure(const MissionDefinition& mission,
                                    int local_owner_id,
                                    const MissionPositionToWorld& to_world) {
  clear();
  m_local_owner_id = local_owner_id;

  m_rules.reserve(mission.stages.size());
  m_stages.reserve(mission.stages.size());

  for (const auto& authored : mission.stages) {
    StageRule rule;
    rule.authored = authored;
    if (authored.target.has_value() && to_world) {
      rule.target = to_world(authored.target.value());
    }
    rule.target_radius = authored.target_radius.value_or(k_default_reach_radius);

    StageStatus status;
    status.id = authored.id;
    status.title = authored.title;
    status.description = authored.description;
    status.hint = authored.hint;
    status.type = authored.type.trimmed().toLower();
    status.required = std::max(1, authored.required_count);
    status.has_target = authored.target.has_value();
    status.target = rule.target;
    if (to_world) {
      status.route.reserve(authored.route.size());
      for (const auto& point : authored.route) {
        status.route.push_back(to_world(point));
      }
    }

    m_rules.push_back(std::move(rule));
    m_stages.push_back(std::move(status));
  }

  refresh_active_index();
}

void MissionStageTracker::clear() {
  m_rules.clear();
  m_stages.clear();
  m_active_index = -1;
}

void MissionStageTracker::refresh_active_index() {
  m_active_index = -1;
  for (std::size_t i = 0; i < m_stages.size(); ++i) {
    m_stages[i].active = false;
    if (m_active_index < 0 && !m_stages[i].complete) {
      m_active_index = static_cast<int>(i);
    }
  }
  if (m_active_index >= 0) {
    m_stages[static_cast<std::size_t>(m_active_index)].active = true;
  }
}

auto MissionStageTracker::active_target() const -> std::optional<QVector3D> {
  if (m_active_index < 0 || m_active_index >= static_cast<int>(m_stages.size())) {
    return std::nullopt;
  }
  const auto& stage = m_stages[static_cast<std::size_t>(m_active_index)];
  if (!stage.has_target) {
    return std::nullopt;
  }
  return stage.target;
}

auto MissionStageTracker::serialize() const -> QJsonObject {
  QJsonArray stages;
  for (const auto& status : m_stages) {
    QJsonObject entry;
    entry["id"] = status.id;
    entry["progress"] = status.progress;
    entry["complete"] = status.complete;
    stages.append(entry);
  }

  QJsonObject root;
  root["stages"] = stages;
  return root;
}

void MissionStageTracker::restore(const QJsonObject& state) {
  if (m_stages.empty()) {
    return;
  }

  const QJsonArray stages = state.value("stages").toArray();
  for (int i = 0; i < stages.size(); ++i) {
    const QJsonObject entry = stages.at(i).toObject();
    const QString id = entry.value("id").toString();

    auto match = std::find_if(
        m_stages.begin(), m_stages.end(), [&id](const StageStatus& status) {
          return !id.isEmpty() && status.id == id;
        });
    if (match == m_stages.end()) {
      if (i >= static_cast<int>(m_stages.size())) {
        continue;
      }
      match = m_stages.begin() + i;
    }

    match->progress = std::clamp(entry.value("progress").toInt(), 0, match->required);
    match->complete = entry.value("complete").toBool();
    if (match->complete) {
      match->progress = match->required;
    }
  }

  refresh_active_index();
}

auto MissionStageTracker::update(Game::Session::SessionContext& session,
                                 const StageWorldFacts& facts) -> bool {
  if (m_stages.empty()) {
    return false;
  }

  auto& world = session.world();
  auto& nation_registry = session.nations();
  auto& owner_registry = session.owners();
  const auto* local_nation = nation_registry.get_nation_for_player(m_local_owner_id);
  const Game::Systems::NationID local_nation_id =
      (local_nation != nullptr) ? local_nation->id
                                : nation_registry.default_nation_id();

  std::vector<StageTally> tallies(m_stages.size());
  int enemy_commanders = 0;

  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->health <= 0) {
      continue;
    }

    const bool is_local = unit->owner_id == m_local_owner_id;
    const bool is_enemy =
        !is_local && owner_registry.are_enemies(m_local_owner_id, unit->owner_id);
    const bool is_commander =
        entity->get_component<Engine::Core::CommanderComponent>() != nullptr;

    if (is_enemy && is_commander) {
      enemy_commanders += 1;
    }

    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    const QString unit_type =
        QString::fromStdString(Game::Units::spawn_typeToString(unit->spawn_type));

    bool captured_from_enemy = false;
    if (is_local) {
      const auto* building = entity->get_component<Engine::Core::BuildingComponent>();
      captured_from_enemy =
          building != nullptr && building->original_nation_id != local_nation_id;
    }

    for (std::size_t i = 0; i < m_rules.size(); ++i) {
      const auto& rule = m_rules[i];
      auto& tally = tallies[i];

      if (is_local && !is_commander && m_stages[i].has_target && transform != nullptr) {
        const float radius = rule.target_radius;
        const float dx = transform->position.x - rule.target.x();
        const float dz = transform->position.z - rule.target.z();
        if ((dx * dx) + (dz * dz) <= radius * radius) {
          tally.local_units_in_range += 1;
        }
      }

      if (rule.authored.structure_types.empty() ||
          !matches_type(rule.authored.structure_types, unit_type)) {
        continue;
      }

      if (is_local) {
        tally.local_owned_structures += 1;
        if (captured_from_enemy) {
          tally.local_captured_structures += 1;
        }
      } else if (is_enemy) {
        tally.enemy_structures += 1;
      }

      if (m_stages[i].has_target && transform != nullptr && (is_local || is_enemy)) {
        const float dx = transform->position.x - rule.target.x();
        const float dz = transform->position.z - rule.target.z();
        const float distance_sq = (dx * dx) + (dz * dz);
        if (distance_sq <=
                k_target_structure_match_radius * k_target_structure_match_radius &&
            distance_sq < tally.target_structure_distance_sq) {
          tally.target_structure_distance_sq = distance_sq;
          tally.target_structure_is_local = is_local;
        }
      }
    }
  }

  bool changed = false;

  for (std::size_t i = 0; i < m_stages.size(); ++i) {
    auto& rule = m_rules[i];
    auto& status = m_stages[i];
    const auto& tally = tallies[i];

    int progress = status.progress;
    int required = status.required;
    QString detail = status.detail;
    const QString& type = status.type;

    if (type == QStringLiteral("capture_structures")) {
      progress = tally.local_captured_structures;
    } else if (type == QStringLiteral("control_structures")) {
      progress = tally.local_owned_structures;
    } else if (type == QStringLiteral("destroy_structures")) {
      if (!rule.baseline_captured) {
        rule.baseline = tally.enemy_structures;
        rule.baseline_captured = true;
      }
      progress = std::max(0, rule.baseline - tally.enemy_structures);
    } else if (type == QStringLiteral("eliminate_commanders")) {
      if (!rule.baseline_captured) {
        rule.baseline = enemy_commanders;
        rule.baseline_captured = true;
      }
      if (rule.authored.required_count <= 1 && rule.baseline > 0) {
        required = rule.baseline;
      }
      progress = std::max(0, rule.baseline - enemy_commanders);
    } else if (type == QStringLiteral("reach_position")) {
      progress = tally.local_units_in_range > 0 ? required : 0;
    } else if (type == QStringLiteral("survive_time")) {
      const float duration = rule.authored.duration.value_or(0.0F);
      required = std::max(1, static_cast<int>(std::ceil(duration)));
      progress = static_cast<int>(std::floor(facts.elapsed_seconds));
    } else if (type == QStringLiteral("accumulate_resources")) {

      const auto harvested = session.economy().get_harvested_all(m_local_owner_id);
      const auto& wanted = rule.authored.resources;
      const auto resource_progress = Game::Systems::resource_tally(
          harvested, wanted.value_or(Game::Systems::ResourceAmounts{}));
      detail = resource_progress.text;
      required = std::max(1, resource_progress.kinds);
      progress = resource_progress.met;
    } else if (type == QStringLiteral("survive_waves")) {
      required = std::max(1, rule.authored.wave_count.value_or(1));
      progress = facts.cleared_wave_count;
    }

    progress = std::clamp(progress, 0, required);

    const bool complete = status.complete || progress >= required;
    if (complete) {
      progress = required;
    }

    if (progress != status.progress || complete != status.complete ||
        required != status.required || detail != status.detail) {
      status.progress = progress;
      status.required = required;
      status.complete = complete;
      status.detail = detail;
      changed = true;
    }

    const bool structure_present =
        tally.target_structure_distance_sq < std::numeric_limits<float>::max();
    const bool structure_is_local =
        structure_present && tally.target_structure_is_local;
    if (structure_present != status.target_structure_present ||
        structure_is_local != status.target_structure_is_local) {
      status.target_structure_present = structure_present;
      status.target_structure_is_local = structure_is_local;
      changed = true;
    }
  }

  if (changed) {
    refresh_active_index();
  }

  return changed;
}

} // namespace Game::Mission
