#pragma once

#include <QAbstractListModel>
#include <QVariantList>

#include <vector>

#include "game/core/entity.h"

namespace App::Core {
struct ClientContext;
}

namespace App::Models {

struct SelectionGroup {
  QString type_key;
  QString name;
  QString nation;
  int count = 0;
  int wounded_count = 0;
  int soldiers = 0;
  int max_soldiers = 0;

  double health = 0.0;
  double stamina = 1.0;
  bool can_run = false;

  QString activity = QStringLiteral("idle");
  QString activity_state = QStringLiteral("active");
  int activity_count = 0;
  bool mixed_activity = false;
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
    SoldiersRole,
    MaxSoldiersRole,
    NationRole,
    StaminaRatioRole,
    IsRunningRole,
    CanRunRole,
    ActivityRole,
    ActivityStateRole
  };

  explicit SelectedUnitsModel(const App::Core::ClientContext& context,
                              QObject* parent = nullptr);

  [[nodiscard]] auto
  rowCount(const QModelIndex& parent = QModelIndex()) const -> int override;
  [[nodiscard]] auto data(const QModelIndex& index,
                          int role = Qt::DisplayRole) const -> QVariant override;
  [[nodiscard]] auto roleNames() const -> QHash<int, QByteArray> override;

  Q_INVOKABLE [[nodiscard]] QVariantList grouped_by_type() const;

public slots:
  void refresh();

private:
  const App::Core::ClientContext& m_context;
  std::vector<Engine::Core::EntityID> m_ids;
};
