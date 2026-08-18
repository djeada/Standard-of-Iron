#include "app/models/selected_units_model.h"

#include <algorithm>
#include <vector>

#include "app/core/client_context.h"
#include "app/world/unit_queries.h"
#include "game/render_bridge/selection_controller.h"

SelectedUnitsModel::SelectedUnitsModel(const App::Core::ClientContext& context,
                                       QObject* parent)
    : QAbstractListModel(parent)
    , m_context(context) {
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
  const auto id = m_ids[index.row()];

  if (role == UnitIdRole) {
    return QVariant::fromValue<qulonglong>(static_cast<qulonglong>(id));
  }
  if (role == UnitTypeRole) {
    QString type_key;
    if (App::World::unit_type_key(m_context.world, id, type_key)) {
      return type_key;
    }
    return {};
  }
  if (role == ActivityRole || role == ActivityStateRole) {
    const auto activity = App::World::unit_activity(m_context.world, id);
    const auto text = role == ActivityRole
                          ? Game::Systems::activity_kind_id(activity.kind)
                          : Game::Systems::activity_state_id(activity.state);
    return QString::fromUtf8(text.data(), static_cast<int>(text.size()));
  }

  App::World::UnitDescription unit;
  if (!App::World::describe_unit(m_context.world, id, unit)) {
    return {};
  }
  if (role == NameRole) {
    return unit.name;
  }
  if (role == HealthRole) {
    return unit.health;
  }
  if (role == max_healthRole) {
    return unit.max_health;
  }
  if (role == HealthRatioRole) {
    return unit.max_health > 0
               ? static_cast<double>(std::clamp(unit.health, 0, unit.max_health)) /
                     static_cast<double>(unit.max_health)
               : 0.0;
  }
  if (role == NationRole) {
    return unit.nation;
  }
  if (role == StaminaRatioRole || role == IsRunningRole || role == CanRunRole) {
    App::World::UnitStamina stamina;
    (void)App::World::describe_unit_stamina(m_context.world, id, stamina);
    if (role == StaminaRatioRole) {
      return static_cast<double>(stamina.ratio);
    }
    if (role == IsRunningRole) {
      return stamina.is_running;
    }
    return stamina.can_run;
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

auto SelectedUnitsModel::grouped_by_type() const -> QVariantList {
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
  if (m_context.selection == nullptr) {
    return;
  }
  std::vector<Engine::Core::EntityID> ids;
  m_context.selection->get_selected_unit_ids(ids);

  if (ids.size() == m_ids.size() && std::equal(ids.begin(), ids.end(), m_ids.begin())) {
    if (!m_ids.empty()) {
      const QModelIndex first = index(0, 0);
      const QModelIndex last = index(static_cast<int>(m_ids.size()) - 1, 0);
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
  for (const auto id : ids) {
    App::World::UnitDescription unit;
    if (!App::World::describe_unit(m_context.world, id, unit)) {
      continue;
    }
    if (unit.is_building || !unit.alive) {
      continue;
    }
    m_ids.push_back(id);
  }
  endResetModel();
}
