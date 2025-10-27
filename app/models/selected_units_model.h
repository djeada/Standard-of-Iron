#pragma once

#include "../../game/core/entity.h"
#include <QAbstractListModel>
#include <vector>

class GameEngine;

class SelectedUnitsModel : public QAbstractListModel {
  Q_OBJECT
public:
  enum Roles {
    UnitIdRole = Qt::UserRole + 1,
    NameRole,
    HealthRole,
    max_healthRole,
    HealthRatioRole
  };

  explicit SelectedUnitsModel(GameEngine *engine, QObject *parent = nullptr);

  [[nodiscard]] auto
  rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  [[nodiscard]] auto
  data(const QModelIndex &index,
       int role = Qt::DisplayRole) const -> QVariant override;
  [[nodiscard]] auto roleNames() const -> QHash<int, QByteArray> override;

  void refresh();

private:
  GameEngine *m_engine = nullptr;
  std::vector<Engine::Core::EntityID> m_ids;
};
