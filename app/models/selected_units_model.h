#pragma once

#include <QAbstractListModel>
#include <QHash>
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

class SelectionActivityDwell {
public:
  static constexpr int k_confirmations = 2;

  void settle(std::vector<SelectionGroup>& groups);

  void forget_missing(const std::vector<SelectionGroup>& groups);

  void clear() { m_held.clear(); }

private:
  struct Held {
    QString activity;
    QString activity_state;
    QString candidate_activity;
    QString candidate_activity_state;
    int candidate_seen = 0;
  };

  QHash<QString, Held> m_held;
};

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

  void reset_activity_dwell() { m_activity_dwell.clear(); }

public slots:
  void refresh();

private:
  const App::Core::ClientContext& m_context;
  std::vector<Engine::Core::EntityID> m_ids;
  mutable App::Models::SelectionActivityDwell m_activity_dwell;
};
