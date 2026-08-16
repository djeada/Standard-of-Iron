#pragma once

#include <QImage>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace App::Core {
struct ClientContext;
struct MatchLaunch;
} // namespace App::Core

namespace App::ViewModels {

class MatchSetupViewModel : public QObject {
  Q_OBJECT

  Q_PROPERTY(QVariantList maps READ maps NOTIFY maps_changed)
  Q_PROPERTY(bool maps_loading READ maps_loading NOTIFY maps_loading_changed)
  Q_PROPERTY(QVariantList nations READ nations CONSTANT)
  Q_PROPERTY(QVariantList campaigns READ campaigns NOTIFY campaigns_changed)
  Q_PROPERTY(bool campaign_completed READ campaign_completed NOTIFY campaigns_changed)
  Q_PROPERTY(
      bool is_campaign_mission READ is_campaign_mission NOTIFY current_mission_changed)

public:
  explicit MatchSetupViewModel(const App::Core::ClientContext& context,
                               QObject* parent = nullptr);

  Q_INVOKABLE void start_loading_maps();
  Q_INVOKABLE [[nodiscard]] QVariantList maps() const { return m_maps; }
  [[nodiscard]] auto maps_loading() const -> bool { return m_maps_loading; }
  [[nodiscard]] auto nations() const -> QVariantList;
  Q_INVOKABLE [[nodiscard]] QVariantList
  commanders_for_nation(const QString& nation_id) const;
  Q_INVOKABLE [[nodiscard]] QImage
  map_preview(const QString& map_path, const QVariantList& player_configs) const;

  Q_INVOKABLE void load_campaigns();
  [[nodiscard]] auto campaigns() const -> QVariantList;
  [[nodiscard]] auto campaign_completed() const -> bool;
  [[nodiscard]] auto is_campaign_mission() const -> bool;
  Q_INVOKABLE void mark_current_mission_completed();
  Q_INVOKABLE [[nodiscard]] QVariantMap current_mission_objectives() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap
  mission_definition(const QString& mission_id) const;

  Q_INVOKABLE void start_skirmish(const QString& map_path,
                                  const QVariantList& player_configs = QVariantList());
  Q_INVOKABLE void start_campaign_mission(const QString& mission_path);
  Q_INVOKABLE void start_mission_file(const QString& file_path);
  void start_tutorial();

  void set_maps(const QVariantList& maps);
  void set_maps_loading(bool loading);
  void notify_current_mission_changed() { emit current_mission_changed(); }
  void notify_campaigns_changed() { emit campaigns_changed(); }

signals:
  void maps_changed();
  void maps_loading_changed();
  void campaigns_changed();
  void current_mission_changed();

  void launch_requested(const App::Core::MatchLaunch& launch);

  void failed(const QString& message);

private:
  void launch_current_mission(const QString& kind, const QString& reference);

  const App::Core::ClientContext& m_context;
  QVariantList m_maps;
  bool m_maps_loading = false;
};

} // namespace App::ViewModels
