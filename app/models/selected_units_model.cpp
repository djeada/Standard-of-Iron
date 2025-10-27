#include "selected_units_model.h"
#include "../core/game_engine.h"
#include <algorithm>
#include <qabstractitemmodel.h>
#include <qhash.h>
#include <qhashfunctions.h>
#include <qobject.h>
#include <qstringview.h>
#include <qtmetamacros.h>
#include <qvariant.h>
#include <vector>

SelectedUnitsModel::SelectedUnitsModel(GameEngine *engine, QObject *parent)
    : QAbstractListModel(parent), m_engine(engine) {}

auto SelectedUnitsModel::rowCount(const QModelIndex &parent) const -> int {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(m_ids.size());
}

auto SelectedUnitsModel::data(const QModelIndex &index,
                              int role) const -> QVariant {
  if (!index.isValid() || index.row() < 0 ||
      index.row() >= static_cast<int>(m_ids.size())) {
    return {};
  }
  auto id = m_ids[index.row()];
  if (m_engine == nullptr) {
    return {};
  }
  QString name;
  int hp = 0;
  int max_hp = 0;
  bool isB = false;
  bool alive = false;
  if (role == UnitIdRole) {
    return QVariant::fromValue<int>(static_cast<int>(id));
  }
  if (!m_engine->getUnitInfo(id, name, hp, max_hp, isB, alive)) {
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
  return {};
}

auto SelectedUnitsModel::roleNames() const -> QHash<int, QByteArray> {
  return {{UnitIdRole, "unit_id"},
          {NameRole, "name"},
          {HealthRole, "health"},
          {max_healthRole, "max_health"},
          {HealthRatioRole, "health_ratio"}};
}

void SelectedUnitsModel::refresh() {
  if (m_engine == nullptr) {
    return;
  }
  std::vector<Engine::Core::EntityID> ids;
  m_engine->getSelectedUnitIds(ids);

  if (ids.size() == m_ids.size() &&
      std::equal(ids.begin(), ids.end(), m_ids.begin())) {
    if (!m_ids.empty()) {
      QModelIndex const first = index(0, 0);
      QModelIndex const last = index(static_cast<int>(m_ids.size()) - 1, 0);
      emit dataChanged(first, last,
                       {HealthRole, max_healthRole, HealthRatioRole});
    }
    return;
  }

  beginResetModel();

  m_ids.clear();
  for (auto id : ids) {
    QString nm;
    int hp = 0;
    int max_hp = 0;
    bool isB = false;
    bool alive = false;
    if (!m_engine->getUnitInfo(id, nm, hp, max_hp, isB, alive)) {
      continue;
    }
    if (isB) {
      continue;
    }
    if (!alive) {
      continue;
    }
    m_ids.push_back(id);
  }
  endResetModel();
}
