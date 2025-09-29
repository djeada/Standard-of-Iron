#include "selected_units_model.h"
#include "game_engine.h"
#include "../game/systems/selection_system.h"
#include "../game/core/world.h"
#include "../game/core/component.h"
#include <algorithm>

SelectedUnitsModel::SelectedUnitsModel(GameEngine* engine, QObject* parent)
    : QAbstractListModel(parent), m_engine(engine) {
}

int SelectedUnitsModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_ids.size());
}

QVariant SelectedUnitsModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_ids.size())) return {};
    auto id = m_ids[index.row()];
    if (!m_engine) return {};
    auto* world = reinterpret_cast<Engine::Core::World*>(m_engine->property("_worldPtr").value<void*>());
    if (!world) return {};
    auto* e = world->getEntity(id);
    if (!e) return {};
    if (role == UnitIdRole) return QVariant::fromValue<int>(static_cast<int>(id));
    if (role == NameRole) {
        if (auto* u = e->getComponent<Engine::Core::UnitComponent>()) return QString::fromStdString(u->unitType);
        return QStringLiteral("Unit");
    }
    if (role == HealthRole) {
        if (auto* u = e->getComponent<Engine::Core::UnitComponent>()) return u->health;
        return 0;
    }
    if (role == MaxHealthRole) {
        if (auto* u = e->getComponent<Engine::Core::UnitComponent>()) return u->maxHealth;
        return 0;
    }
    if (role == HealthRatioRole) {
        if (auto* u = e->getComponent<Engine::Core::UnitComponent>()) {
            if (u->maxHealth > 0) {
                return static_cast<double>(std::clamp(u->health, 0, u->maxHealth)) / static_cast<double>(u->maxHealth);
            }
        }
        return 0.0;
    }
    return {};
}

QHash<int, QByteArray> SelectedUnitsModel::roleNames() const {
    return {
        { UnitIdRole, "unitId" },
        { NameRole, "name" },
        { HealthRole, "health" },
        { MaxHealthRole, "maxHealth" },
        { HealthRatioRole, "healthRatio" }
    };
}

void SelectedUnitsModel::refresh() {
    if (!m_engine) return;
    auto* selSys = reinterpret_cast<Game::Systems::SelectionSystem*>(m_engine->property("_selPtr").value<void*>());
    if (!selSys) return;
    const auto& ids = selSys->getSelectedUnits();

    // If the selected IDs are unchanged, emit dataChanged to refresh health ratios without resetting the list
    if (ids.size() == m_ids.size() && std::equal(ids.begin(), ids.end(), m_ids.begin())) {
        if (!m_ids.empty()) {
            QModelIndex first = index(0, 0);
            QModelIndex last = index(static_cast<int>(m_ids.size()) - 1, 0);
            emit dataChanged(first, last, { HealthRole, MaxHealthRole, HealthRatioRole });
        }
        return;
    }

    beginResetModel();
    // Filter out entities that are dead (health <= 0) if we can access the world
    m_ids.clear();
    auto* world = reinterpret_cast<Engine::Core::World*>(m_engine->property("_worldPtr").value<void*>());
    if (!world) {
        m_ids.assign(ids.begin(), ids.end());
    } else {
        for (auto id : ids) {
            if (auto* e = world->getEntity(id)) {
                if (auto* u = e->getComponent<Engine::Core::UnitComponent>()) {
                    if (u->health > 0) m_ids.push_back(id);
                }
            }
        }
    }
    endResetModel();
}
