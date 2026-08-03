#include "selected_units_model.h"

#include <qabstractitemmodel.h>
#include <qhash.h>
#include <qhashfunctions.h>
#include <qobject.h>
#include <qstringview.h>
#include <qtmetamacros.h>
#include <qvariant.h>

#include <algorithm>
#include <iterator>
#include <vector>

#include "../core/game_engine.h"

SelectedUnitsModel::SelectedUnitsModel(GameEngine* engine, QObject* parent)
    : QAbstractListModel(parent)
    , m_engine(engine) {
}

auto SelectedUnitsModel::rowCount(const QModelIndex& parent) const -> int {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(m_ids.size());
}

auto SelectedUnitsModel::data(const QModelIndex& index, int role) const -> QVariant {
  if (!index.isValid() || index.row() < 0 ||
      index.row() >= static_cast<int>(m_ids.size())) {
    return {};
  }
  auto id = m_ids[index.row()];
  if (m_engine == nullptr) {
    return {};
  }
  QString name;
  QString nation;
  int hp = 0;
  int max_hp = 0;
  bool is_b = false;
  bool alive = false;
  if (role == UnitIdRole) {

    return QVariant::fromValue<qulonglong>(static_cast<qulonglong>(id));
  }
  if (role == UnitTypeRole) {
    QString type_key;
    if (m_engine->get_unit_type_key(id, type_key)) {
      return type_key;
    }
    return {};
  }
  if (role == ActivityRole || role == ActivityStateRole) {
    const QVariantMap activity = m_engine->unit_activity(id);
    return role == ActivityRole ? activity.value(QStringLiteral("activity"))
                                : activity.value(QStringLiteral("state"));
  }
  if (!m_engine->get_unit_info(id, name, hp, max_hp, is_b, alive, nation)) {
    return {};
  }
  if (role == NameRole) {
    return name;
  }
  if (role == HealthRole) {
    return hp;
  }
  if (role == max_healthRole) {
    return max_hp;
  }
  if (role == HealthRatioRole) {
    return (max_hp > 0 ? static_cast<double>(std::clamp(hp, 0, max_hp)) /
                             static_cast<double>(max_hp)
                       : 0.0);
  }
  if (role == NationRole) {
    return nation;
  }
  if (role == StaminaRatioRole || role == IsRunningRole || role == CanRunRole) {
    float stamina_ratio = 1.0F;
    bool is_running = false;
    bool can_run = false;
    m_engine->get_unit_stamina_info(id, stamina_ratio, is_running, can_run);
    if (role == StaminaRatioRole) {
      return static_cast<double>(stamina_ratio);
    }
    if (role == IsRunningRole) {
      return is_running;
    }
    if (role == CanRunRole) {
      return can_run;
    }
  }
  return {};
}

auto SelectedUnitsModel::roleNames() const -> QHash<int, QByteArray> {
  return {{UnitIdRole, "unit_id"},
          {UnitTypeRole, "unit_type"},
          {NameRole, "name"},
          {HealthRole, "health"},
          {max_healthRole, "max_health"},
          {HealthRatioRole, "health_ratio"},
          {NationRole, "nation"},
          {StaminaRatioRole, "stamina_ratio"},
          {IsRunningRole, "is_running"},
          {CanRunRole, "can_run"},
          {ActivityRole, "activity"},
          {ActivityStateRole, "activity_state"}};
}

QVariantList SelectedUnitsModel::grouped_by_type() const {
  QVariantList units;
  units.reserve(static_cast<int>(m_ids.size()));
  for (int row = 0; row < rowCount(); ++row) {
    const QModelIndex model_index = index(row, 0);
    QVariantMap unit;
    unit[QStringLiteral("unit_type")] = data(model_index, UnitTypeRole);
    unit[QStringLiteral("name")] = data(model_index, NameRole);
    unit[QStringLiteral("nation")] = data(model_index, NationRole);
    unit[QStringLiteral("health_ratio")] = data(model_index, HealthRatioRole);
    unit[QStringLiteral("stamina_ratio")] = data(model_index, StaminaRatioRole);
    unit[QStringLiteral("can_run")] = data(model_index, CanRunRole);
    unit[QStringLiteral("activity")] = data(model_index, ActivityRole);
    unit[QStringLiteral("activity_state")] = data(model_index, ActivityStateRole);
    units.append(unit);
  }
  return App::Models::selection_groups_to_variant(
      App::Models::group_selection_by_type(units));
}

void SelectedUnitsModel::refresh() {
  if (m_engine == nullptr) {
    return;
  }
  std::vector<Engine::Core::EntityID> ids;
  m_engine->get_selected_unit_ids(ids);

  if (ids.size() == m_ids.size() && std::equal(ids.begin(), ids.end(), m_ids.begin())) {
    if (!m_ids.empty()) {
      QModelIndex const first = index(0, 0);
      QModelIndex const last = index(static_cast<int>(m_ids.size()) - 1, 0);
      emit dataChanged(first,
                       last,
                       {HealthRole,
                        max_healthRole,
                        HealthRatioRole,
                        StaminaRatioRole,
                        IsRunningRole,
                        CanRunRole,
                        ActivityRole,
                        ActivityStateRole});
    }
    return;
  }

  beginResetModel();

  m_ids.clear();
  for (auto id : ids) {
    QString nm;
    QString nation;
    int hp = 0;
    int max_hp = 0;
    bool is_b = false;
    bool alive = false;
    if (!m_engine->get_unit_info(id, nm, hp, max_hp, is_b, alive, nation)) {
      continue;
    }
    if (is_b) {
      continue;
    }
    if (!alive) {
      continue;
    }
    m_ids.push_back(id);
  }
  endResetModel();
}
