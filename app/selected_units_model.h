#pragma once

#include <QAbstractListModel>
#include <vector>
#include "../game/core/entity.h"

class GameEngine;

class SelectedUnitsModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { UnitIdRole = Qt::UserRole + 1, NameRole, HealthRole, MaxHealthRole, HealthRatioRole };

    explicit SelectedUnitsModel(GameEngine* engine, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

public slots:
    void refresh();

private:
    GameEngine* m_engine = nullptr;
    std::vector<Engine::Core::EntityID> m_ids;
};
