#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>
#include <optional>

#include "game/strategic/strategic_assets.h"
#include "game/strategic/strategic_geometry.h"
#include "game/strategic/strategic_graph.h"
#include "game/strategic/strategic_supply.h"
#include "game/strategic/strategic_validation.h"
#include "game/strategic/strategic_world.h"

namespace MapEditor {

class WorldDocument : public QObject {
  Q_OBJECT

public:
  explicit WorldDocument(QObject* parent = nullptr);

  auto load(const QString& path, QString* error = nullptr) -> bool;
  void adopt(Game::Strategic::WorldDefinition definition,
             std::shared_ptr<Game::Strategic::RegionGeometry> geometry,
             Game::Strategic::ValidationContext context,
             const QString& path = {});
  auto save(QString* error = nullptr) -> bool;
  auto save_as(const QString& path, QString* error = nullptr) -> bool;

  [[nodiscard]] auto path() const -> const QString& { return m_path; }
  [[nodiscard]] auto root() const -> const QString& { return m_root; }
  [[nodiscard]] auto is_modified() const -> bool { return m_modified; }
  [[nodiscard]] auto definition() const -> const Game::Strategic::WorldDefinition& {
    return m_def;
  }
  [[nodiscard]] auto geometry() const -> const Game::Strategic::RegionGeometry* {
    return m_geometry.get();
  }
  [[nodiscard]] auto report() const -> const Game::Strategic::ValidationReport& {
    return m_report;
  }
  [[nodiscard]] auto state() const -> const Game::Strategic::WorldState& {
    return m_state;
  }
  [[nodiscard]] auto previous_state() const -> const Game::Strategic::WorldState& {
    return m_previous_state;
  }
  [[nodiscard]] auto supply() const -> const QVector<Game::Strategic::SupplyReport>& {
    return m_supply;
  }
  [[nodiscard]] auto
  supply_for(const QString& army) const -> const Game::Strategic::SupplyReport*;

  [[nodiscard]] auto timeline_index() const -> int { return m_timeline; }
  [[nodiscard]] auto at_start() const -> bool { return m_timeline == 0; }
  void set_timeline_index(int events_applied);
  [[nodiscard]] auto current_event() const -> const Game::Strategic::WorldEvent*;

  [[nodiscard]] auto selection() const -> const QString& { return m_selection; }
  void select(const QString& id);

  [[nodiscard]] auto can_undo() const -> bool { return !m_undo.isEmpty(); }
  [[nodiscard]] auto can_redo() const -> bool { return !m_redo.isEmpty(); }
  void undo();
  void redo();

  auto move_site(const QString& id, const Game::Strategic::LonLat& position) -> bool;
  auto add_site(const Game::Strategic::LonLat& position,
                Game::Strategic::SiteKind kind) -> QString;
  auto add_route(const QString& from,
                 const QString& to,
                 Game::Strategic::RouteKind kind) -> QString;
  auto add_waypoint(const QString& route,
                    const Game::Strategic::LonLat& position) -> bool;
  auto clear_waypoints(const QString& route) -> bool;
  auto move_waypoint(const QString& route,
                     int index,
                     const Game::Strategic::LonLat& position) -> bool;
  auto remove_waypoint(const QString& route, int index) -> bool;
  auto set_initial_route_status(const QString& route,
                                Game::Strategic::RouteStatus status) -> bool;
  auto remove(const QString& id) -> bool;
  auto update_site(const Game::Strategic::Site& site) -> bool;
  auto update_route(const Game::Strategic::Route& route) -> bool;
  auto update_region(const Game::Strategic::Region& region) -> bool;
  auto set_initial_controller(const QString& id, const QString& faction) -> bool;
  auto set_initial_army_location(const QString& army, const QString& site) -> bool;
  auto complete_network() -> int;

  [[nodiscard]] auto
  region_for(const Game::Strategic::LonLat& position) const -> QString;
  [[nodiscard]] auto nearest_site(const Game::Strategic::LonLat& position,
                                  const QStringList& exclude = {}) const -> QString;

signals:
  void changed();
  void selection_changed(const QString& id);
  void timeline_changed(int index);
  void modified_changed(bool modified);
  void loaded();

private:
  void checkpoint();
  void commit();
  void refresh_state();
  void revalidate();
  void set_modified(bool modified);

  Game::Strategic::WorldDefinition m_def;
  std::shared_ptr<Game::Strategic::RegionGeometry> m_geometry;
  Game::Strategic::ValidationContext m_context;
  Game::Strategic::ValidationReport m_report;
  Game::Strategic::WorldState m_state;
  Game::Strategic::WorldState m_previous_state;
  QVector<Game::Strategic::SupplyReport> m_supply;
  QVector<Game::Strategic::WorldDefinition> m_undo;
  QVector<Game::Strategic::WorldDefinition> m_redo;
  QString m_path;
  QString m_root;
  QString m_selection;
  int m_timeline = 0;
  bool m_modified = false;
};

} // namespace MapEditor
