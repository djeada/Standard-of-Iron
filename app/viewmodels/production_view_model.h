#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include "app/core/frame_snapshot.h"
#include "game/command/command.h"

namespace App::Core {
struct ClientContext;
class ClientHost;
} // namespace App::Core

namespace App::ViewModels {

class ProductionViewModel : public QObject {
  Q_OBJECT

public:
  ProductionViewModel(const App::Core::ClientContext& context,
                      App::Core::ClientHost& host,
                      QObject* parent = nullptr);

  Q_INVOKABLE [[nodiscard]] bool has_selected_type(const QString& type) const;

  void publish_frame();
  Q_INVOKABLE void recruit_near_selected(const QString& unit_type);

  Q_INVOKABLE [[nodiscard]] QVariantMap selected_state() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap selected_home_state() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap selected_temple_state() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap selected_builder_state() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap selected_marketplace_state() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap selected_farm_state() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap unit_info(const QString& unit_type,
                                                  const QString& nation_id) const;

  Q_INVOKABLE bool marketplace_buy(const QString& resource_key);
  Q_INVOKABLE bool marketplace_sell(const QString& resource_key);
  Q_INVOKABLE [[nodiscard]] QVariantList marketplace_allies() const;
  Q_INVOKABLE bool
  send_to_ally(int ally_owner, const QString& resource_key, int amount);
  Q_INVOKABLE bool
  request_from_ally(int ally_owner, const QString& resource_key, int amount);

  Q_INVOKABLE [[nodiscard]] qulonglong selected_building_id() const;
  Q_INVOKABLE [[nodiscard]] QVariantMap ally_call_state(qulonglong entity) const;
  Q_INVOKABLE bool call_allies(qulonglong entity);

  Q_INVOKABLE void set_rally_at_screen(qreal sx, qreal sy);

signals:

  void refused(const QString& message);

  void player_state_stale();

private:
  App::Core::Published<App::Core::SelectionReadout> m_readout;

  auto ally_tribute(int ally_owner,
                    const QString& resource_key,
                    int amount,
                    bool request) -> bool;
  [[nodiscard]] auto trade(const QString& resource_key,
                           Game::Command::TradeDirection direction) -> bool;

  const App::Core::ClientContext& m_context;
  App::Core::ClientHost& m_host;
  QElapsedTimer m_last_ally_call;
};

} // namespace App::ViewModels
