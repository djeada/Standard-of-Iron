#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector3D>

#include <functional>
#include <optional>
#include <vector>

#include "game/map/mission_definition.h"

namespace Game::Session {
class SessionContext;
}

namespace Game::Mission {

struct StageStatus {
  QString id;
  QString title;
  QString description;
  QString hint;
  QString type;
  int progress = 0;
  int required = 1;
  bool complete = false;
  bool active = false;
  bool has_target = false;

  bool target_structure_present = false;
  bool target_structure_is_local = false;
  QVector3D target;
  std::vector<QVector3D> route;
};

struct StageWorldFacts {
  float elapsed_seconds = 0.0F;
  int cleared_wave_count = 0;
};

using MissionPositionToWorld = std::function<QVector3D(const Position&)>;

class MissionStageTracker {
public:
  void configure(const MissionDefinition& mission,
                 int local_owner_id,
                 const MissionPositionToWorld& to_world);
  void clear();

  auto update(Game::Session::SessionContext& session,
              const StageWorldFacts& facts) -> bool;

  [[nodiscard]] auto has_stages() const -> bool { return !m_stages.empty(); }
  [[nodiscard]] auto stages() const -> const std::vector<StageStatus>& {
    return m_stages;
  }

  [[nodiscard]] auto active_index() const -> int { return m_active_index; }

  [[nodiscard]] auto active_target() const -> std::optional<QVector3D>;

  [[nodiscard]] auto serialize() const -> QJsonObject;
  void restore(const QJsonObject& state);

private:
  struct StageRule {
    MissionStage authored;
    QVector3D target;
    float target_radius = 0.0F;
    int baseline = 0;
    bool baseline_captured = false;
  };

  void refresh_active_index();

  std::vector<StageRule> m_rules;
  std::vector<StageStatus> m_stages;
  int m_local_owner_id = 1;
  int m_active_index = -1;
};

} // namespace Game::Mission
