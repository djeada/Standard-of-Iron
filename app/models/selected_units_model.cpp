#include "app/models/selected_units_model.h"

#include <algorithm>
#include <vector>

#include "app/core/client_context.h"
#include "app/world/unit_queries.h"
#include "game/render_bridge/selection_controller.h"

namespace {

auto health_ratio_of(const App::World::UnitDescription& unit) -> double {
  if (unit.max_health <= 0) {
    return 0.0;
  }
  return static_cast<double>(std::clamp(unit.health, 0, unit.max_health)) /
         static_cast<double>(unit.max_health);
}

auto activity_text(const Game::Systems::UnitActivity& activity, bool state) -> QString {
  const auto text = state ? Game::Systems::activity_state_id(activity.state)
                          : Game::Systems::activity_kind_id(activity.kind);
  return QString::fromUtf8(text.data(), static_cast<int>(text.size()));
}

} // namespace

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
    return activity_text(App::World::unit_activity(m_context.world, id),
                         role == ActivityStateRole);
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
  if (role == SoldiersRole) {
    return unit.soldiers;
  }
  if (role == MaxSoldiersRole) {
    return unit.max_soldiers;
  }
  if (role == HealthRatioRole) {
    return health_ratio_of(unit);
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
          {SoldiersRole, "soldiers"},
          {MaxSoldiersRole, "max_soldiers"},
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

  for (const auto id : m_ids) {
    App::World::UnitDescription described;
    if (!App::World::describe_unit(m_context.world, id, described)) {
      continue;
    }
    QString type_key;
    (void)App::World::unit_type_key(m_context.world, id, type_key);
    App::World::UnitStamina stamina;
    (void)App::World::describe_unit_stamina(m_context.world, id, stamina);
    const auto activity = App::World::unit_activity(m_context.world, id);

    QVariantMap unit;
    unit[QStringLiteral("unit_type")] = type_key;
    unit[QStringLiteral("name")] = described.name;
    unit[QStringLiteral("nation")] = described.nation;
    unit[QStringLiteral("health_ratio")] = health_ratio_of(described);
    unit[QStringLiteral("soldiers")] = described.soldiers;
    unit[QStringLiteral("max_soldiers")] = described.max_soldiers;
    unit[QStringLiteral("stamina_ratio")] = static_cast<double>(stamina.ratio);
    unit[QStringLiteral("can_run")] = stamina.can_run;
    unit[QStringLiteral("activity")] = activity_text(activity, false);
    unit[QStringLiteral("activity_state")] = activity_text(activity, true);
    units.append(unit);
  }
  return App::Models::selection_groups_to_variant(
      App::Models::group_selection_by_type(units));
}

void SelectedUnitsModel::refresh() {
  if (m_context.selection == nullptr) {
    return;
  }
  std::vector<Engine::Core::EntityID> selected;
  m_context.selection->get_selected_unit_ids(selected);

  std::vector<Engine::Core::EntityID> living;
  living.reserve(selected.size());
  for (const auto id : selected) {
    App::World::UnitDescription unit;
    if (!App::World::describe_unit(m_context.world, id, unit)) {
      continue;
    }
    if (unit.is_building || !unit.alive) {
      continue;
    }
    living.push_back(id);
  }

  if (living == m_ids) {
    if (!m_ids.empty()) {
      const QModelIndex first = index(0, 0);
      const QModelIndex last = index(static_cast<int>(m_ids.size()) - 1, 0);
      emit dataChanged(first,
                       last,
                       {HealthRole,
                        max_healthRole,
                        HealthRatioRole,
                        SoldiersRole,
                        MaxSoldiersRole,
                        StaminaRatioRole,
                        IsRunningRole,
                        CanRunRole,
                        ActivityRole,
                        ActivityStateRole});
    }
    return;
  }

  beginResetModel();
  m_ids = std::move(living);
  endResetModel();
}
