#pragma once

#include <QImage>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace App::Core {
struct ClientContext;
class ClientHost;
struct MatchLaunch;
} // namespace App::Core

namespace App::ViewModels {

class MapList {
public:
  void begin_loading() { m_maps.clear(); }
  void end_loading() {}
  void append(const QVariantMap& map) { m_maps.append(map); }
  void replace(const QVariantList& maps) { m_maps = maps; }

  [[nodiscard]] auto maps() const -> const QVariantList& { return m_maps; }
  [[nodiscard]] auto empty() const -> bool { return m_maps.isEmpty(); }

private:
  QVariantList m_maps;
};

class MatchSetupViewModel : public QObject {
  Q_OBJECT

  Q_PROPERTY(QVariantList maps READ maps NOTIFY maps_changed)
  Q_PROPERTY(bool maps_loading READ maps_loading NOTIFY maps_loading_changed)
  Q_PROPERTY(QVariantList nations READ nations CONSTANT)
  Q_PROPERTY(QVariantList campaigns READ campaigns NOTIFY campaigns_changed)
  Q_PROPERTY(QVariantList missions READ missions NOTIFY missions_changed)
  Q_PROPERTY(bool campaign_completed READ campaign_completed NOTIFY campaigns_changed)
  Q_PROPERTY(bool is_mission_match READ is_mission_match NOTIFY current_mission_changed)
  Q_PROPERTY(int starting_gold READ starting_gold WRITE set_starting_gold NOTIFY
                 starting_gold_changed)

public:
  MatchSetupViewModel(const App::Core::ClientContext& context,
                      App::Core::ClientHost& host,
                      QObject* parent = nullptr);

  Q_INVOKABLE void start_loading_maps();
  Q_INVOKABLE [[nodiscard]] QVariantList maps() const { return m_maps.maps(); }
  [[nodiscard]] auto maps_loading() const -> bool { return m_maps_loading; }
  [[nodiscard]] auto nations() const -> QVariantList;
  Q_INVOKABLE [[nodiscard]] QVariantList
  commanders_for_nation(const QString& nation_id) const;
  Q_INVOKABLE [[nodiscard]] QImage
  map_preview(const QString& map_path, const QVariantList& player_configs) const;

  Q_INVOKABLE [[nodiscard]] QVariantList map_bases(const QString& map_path) const;

  [[nodiscard]] auto starting_gold() const -> int;
  void set_starting_gold(int gold);

  Q_INVOKABLE void load_campaigns();
  Q_INVOKABLE void load_missions();
  [[nodiscard]] auto missions() const -> QVariantList;
  [[nodiscard]] auto campaigns() const -> QVariantList;
  [[nodiscard]] auto campaign_completed() const -> bool;

  [[nodiscard]] auto is_mission_match() const -> bool;
  Q_INVOKABLE void mark_current_mission_completed();
  Q_INVOKABLE [[nodiscard]] QVariantMap current_mission_objectives() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap
  mission_definition(const QString& mission_id) const;

  Q_INVOKABLE void start_skirmish(const QString& map_path,
                                  const QVariantList& player_configs = QVariantList());
  Q_INVOKABLE bool start_observed_skirmish(const QString& map_path);
  [[nodiscard]] auto
  build_observer_player_configs(const QString& map_path) const -> QVariantList;
  Q_INVOKABLE void start_campaign_mission(const QString& mission_path);
  Q_INVOKABLE void start_mission_file(const QString& file_path);
  void start_tutorial();

  void set_maps(const QVariantList& maps);
  void append_map(const QVariantMap& map);
  void set_maps_loading(bool loading);
  void notify_current_mission_changed() { emit current_mission_changed(); }
  void notify_campaigns_changed() { emit campaigns_changed(); }

signals:
  void maps_changed();
  void maps_loading_changed();
  void campaigns_changed();
  void missions_changed();
  void current_mission_changed();
  void starting_gold_changed();

  void launch_requested(const App::Core::MatchLaunch& launch);

  void failed(const QString& message);

private:
  void launch_current_mission(const QString& kind, const QString& reference);

  const App::Core::ClientContext& m_context;
  App::Core::ClientHost& m_host;
  MapList m_maps;
  QVariantList m_missions;
  bool m_maps_loading = false;
};

} // namespace App::ViewModels
