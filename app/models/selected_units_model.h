#pragma once

#include <QAbstractListModel>
#include <QVariantList>

#include <vector>

#include "../../game/core/entity.h"

class GameEngine;

namespace App::Models {

struct SelectionGroup {
  QString type_key;
  QString name;
  QString nation;
  int count = 0;
  int wounded_count = 0;

  double health = 0.0;
  double stamina = 1.0;
};

[[nodiscard]] auto
group_selection_by_type(const QVariantList& units) -> std::vector<SelectionGroup>;

[[nodiscard]] auto
selection_groups_to_variant(const std::vector<SelectionGroup>& groups) -> QVariantList;

} // namespace App::Models

class SelectedUnitsModel : public QAbstractListModel {
  Q_OBJECT
public:
  enum Roles {
    UnitIdRole = Qt::UserRole + 1,
    UnitTypeRole,
    NameRole,
    HealthRole,
    max_healthRole,
    HealthRatioRole,
    NationRole,
    StaminaRatioRole,
    IsRunningRole,
    CanRunRole
  };

  explicit SelectedUnitsModel(GameEngine* engine, QObject* parent = nullptr);

  [[nodiscard]] auto
  rowCount(const QModelIndex& parent = QModelIndex()) const -> int override;
  [[nodiscard]] auto data(const QModelIndex& index,
                          int role = Qt::DisplayRole) const -> QVariant override;
  [[nodiscard]] auto roleNames() const -> QHash<int, QByteArray> override;

  Q_INVOKABLE [[nodiscard]] QVariantList grouped_by_type() const;

public slots:
  void refresh();

private:
  GameEngine* m_engine = nullptr;
  std::vector<Engine::Core::EntityID> m_ids;
};
