#include "selected_units_model.h"
#include "../game/core/component.h"
#include "../game/core/world.h"
#include "../game/systems/selection_system.h"
#include "game_engine.h"
#include <algorithm>

SelectedUnitsModel::SelectedUnitsModel(GameEngine *engine, QObject *parent)
    : QAbstractListModel(parent), m_engine(engine) {}

int SelectedUnitsModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return static_cast<int>(m_ids.size());
}

QVariant SelectedUnitsModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 ||
      index.row() >= static_cast<int>(m_ids.size()))
    return {};
  auto id = m_ids[index.row()];
  if (!m_engine)
    return {};
  QString name;
  int hp = 0, maxHp = 0;
  bool isB = false, alive = false;
  if (role == UnitIdRole)
    return QVariant::fromValue<int>(static_cast<int>(id));
  if (!m_engine->getUnitInfo(id, name, hp, maxHp, isB, alive))
    return {};
  if (role == NameRole)
    return name;
  if (role == HealthRole)
    return hp;
  if (role == MaxHealthRole)
    return maxHp;
  if (role == HealthRatioRole)
    return (maxHp > 0 ? static_cast<double>(std::clamp(hp, 0, maxHp)) /
                            static_cast<double>(maxHp)
                      : 0.0);
  return {};
}

QHash<int, QByteArray> SelectedUnitsModel::roleNames() const {
  return {{UnitIdRole, "unitId"},
          {NameRole, "name"},
          {HealthRole, "health"},
          {MaxHealthRole, "maxHealth"},
          {HealthRatioRole, "healthRatio"}};
}

void SelectedUnitsModel::refresh() {
  if (!m_engine)
    return;
  std::vector<Engine::Core::EntityID> ids;
  m_engine->getSelectedUnitIds(ids);

  if (ids.size() == m_ids.size() &&
      std::equal(ids.begin(), ids.end(), m_ids.begin())) {
    if (!m_ids.empty()) {
      QModelIndex first = index(0, 0);
      QModelIndex last = index(static_cast<int>(m_ids.size()) - 1, 0);
      emit dataChanged(first, last,
                       {HealthRole, MaxHealthRole, HealthRatioRole});
    }
    return;
  }

  beginResetModel();

  m_ids.clear();
  for (auto id : ids) {
    QString nm;
    int hp = 0, maxHp = 0;
    bool isB = false, alive = false;
    if (!m_engine->getUnitInfo(id, nm, hp, maxHp, isB, alive))
      continue;
    if (isB)
      continue;
    if (!alive)
      continue;
    m_ids.push_back(id);
  }
  endResetModel();
}
