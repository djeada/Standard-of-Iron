#include "app/viewmodels/mission_view_model.h"

#include <QVariantMap>
#include <QVector3D>

#include "app/core/client_context.h"
#include "app/viewmodels/camera_view_model.h"
#include "scene/camera.h"

namespace App::ViewModels {

namespace {

auto stage_holds_an_enemy_settlement(const QVariantMap& stage) -> bool {
  return stage.value("target_structure_present").toBool() &&
         !stage.value("target_structure_is_local").toBool();
}

} // namespace

MissionViewModel::MissionViewModel(const App::Core::ClientContext& context,
                                   App::Core::ClientHost& host,
                                   CameraViewModel& camera,
                                   QObject* parent)
    : QObject(parent)
    , m_context(context)
    , m_host(host)
    , m_camera(camera) {
}

void MissionViewModel::set_stages(const QVariantList& stages,
                                  bool mirrors_victory_conditions) {
  m_stages = stages;
  m_stages_mirror_victory_conditions = mirrors_victory_conditions;
  m_markers.clear();
  m_active_index = -1;

  bool settlements_left_to_take = false;
  for (int i = 0; i < m_stages.size(); ++i) {
    const QVariantMap stage = m_stages.at(i).toMap();
    const bool complete = stage.value("complete").toBool();
    if (m_active_index < 0 && !complete) {
      m_active_index = i;
    }
    if (!complete && stage_holds_an_enemy_settlement(stage)) {
      settlements_left_to_take = true;
    }
  }

  for (int i = 0; i < m_stages.size(); ++i) {
    const QVariantMap stage = m_stages.at(i).toMap();
    if (!stage.value("has_target").toBool() || !stage.contains("nx")) {
      continue;
    }
    if (stage.value("target_structure_is_local").toBool()) {
      continue;
    }

    const bool is_active = i == m_active_index;
    const bool is_a_settlement_still_to_take =
        settlements_left_to_take && stage_holds_an_enemy_settlement(stage);
    if (!is_active && !is_a_settlement_still_to_take) {
      continue;
    }

    QVariantMap marker;
    marker["nx"] = stage.value("nx");
    marker["ny"] = stage.value("ny");
    marker["title"] = stage.value("title");
    marker["active"] = is_active;
    m_markers.append(marker);
  }

  emit stages_changed();
}

void MissionViewModel::clear() {
  if (m_stages.isEmpty() && m_markers.isEmpty() && m_active_index < 0) {
    return;
  }
  m_stages.clear();
  m_markers.clear();
  m_active_index = -1;
  m_stages_mirror_victory_conditions = false;
  emit stages_changed();
}

auto MissionViewModel::active_stage() const -> QVariantMap {
  if (m_active_index < 0 || m_active_index >= m_stages.size()) {
    return {};
  }
  return m_stages.at(m_active_index).toMap();
}

auto MissionViewModel::active_title() const -> QString {
  return active_stage().value("title").toString();
}

auto MissionViewModel::active_hint() const -> QString {
  const QVariantMap stage = active_stage();
  const QString hint = stage.value("hint").toString();
  return hint.isEmpty() ? stage.value("description").toString() : hint;
}

auto MissionViewModel::active_progress() const -> int {
  return active_stage().value("progress").toInt();
}

auto MissionViewModel::active_required() const -> int {
  const int required = active_stage().value("required").toInt();
  return required > 0 ? required : 1;
}

auto MissionViewModel::active_has_target() const -> bool {
  return active_stage().value("has_target").toBool();
}

auto MissionViewModel::completed_count() const -> int {
  int completed = 0;
  for (const auto& value : m_stages) {
    if (value.toMap().value("complete").toBool()) {
      ++completed;
    }
  }
  return completed;
}

void MissionViewModel::focus_active_stage() {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  const QVariantMap stage = active_stage();
  if (!stage.value("has_target").toBool()) {
    return;
  }
  auto* camera = m_context.active_camera;
  if (camera == nullptr) {
    return;
  }

  const QVector3D target(
      stage.value("world_x").toFloat(), 0.0F, stage.value("world_z").toFloat());
  const QVector3D offset = camera->get_position() - camera->get_target();
  camera->look_at(target + offset, target, camera->get_up_vector());
  m_camera.set_following_selection(false);
}

} // namespace App::ViewModels
