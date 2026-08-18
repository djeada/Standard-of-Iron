#include "app/viewmodels/production_view_model.h"

#include <QCoreApplication>
#include <QStringView>

#include <optional>

#include "app/core/client_context.h"
#include "app/economy/production_manager.h"
#include "app/economy/production_readouts.h"
#include "app/orders/command_controller.h"
#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/render_bridge/selection_controller.h"
#include "game/session/session_context.h"
#include "game/systems/marketplace_system.h"
#include "game/systems/resource_types.h"

namespace App::ViewModels {
namespace {

auto trade_resource_from_key(QStringView key)
    -> std::optional<Game::Systems::ResourceType> {
  if (key == QLatin1String("wood")) {
    return Game::Systems::ResourceType::Wood;
  }
  if (key == QLatin1String("stone")) {
    return Game::Systems::ResourceType::Stone;
  }
  if (key == QLatin1String("iron")) {
    return Game::Systems::ResourceType::Iron;
  }
  return std::nullopt;
}

auto trade_resource_label(QStringView key) -> QString {
  if (key == QLatin1String("wood")) {
    return QCoreApplication::translate("ProductionViewModel", "wood");
  }
  if (key == QLatin1String("stone")) {
    return QCoreApplication::translate("ProductionViewModel", "stone");
  }
  if (key == QLatin1String("iron")) {
    return QCoreApplication::translate("ProductionViewModel", "iron");
  }
  return key.toString();
}

} // namespace

ProductionViewModel::ProductionViewModel(const App::Core::ClientContext& context,
                                         App::Core::ClientHost& host,
                                         QObject* parent)
    : QObject(parent)
    , m_context(context)
    , m_host(host) {
}

auto ProductionViewModel::has_selected_type(const QString& type) const -> bool {
  return m_context.selection != nullptr && m_context.selection->has_selected_type(type);
}

void ProductionViewModel::recruit_near_selected(const QString& unit_type) {
  m_host.ensure_initialized();
  if (m_context.commands != nullptr) {
    m_context.commands->recruit_near_selected(unit_type, m_context.local_owner_id);
  }
}

auto ProductionViewModel::selected_state() const -> QVariantMap {
  return App::Economy::selected_barracks_state(m_context.world,
                                               m_context.local_owner_id);
}

auto ProductionViewModel::selected_home_state() const -> QVariantMap {
  return App::Economy::selected_home_state(m_context.world, m_context.local_owner_id);
}

auto ProductionViewModel::selected_builder_state() const -> QVariantMap {
  return App::Economy::selected_builder_state(m_context.world);
}

auto ProductionViewModel::selected_marketplace_state() const -> QVariantMap {
  return App::Economy::selected_marketplace_state(m_context.world,
                                                  m_context.local_owner_id);
}

auto ProductionViewModel::selected_farm_state() const -> QVariantMap {
  return App::Economy::selected_farm_state(m_context.world, m_context.local_owner_id);
}

auto ProductionViewModel::unit_info(const QString& unit_type,
                                    const QString& nation_id) const -> QVariantMap {
  return App::Economy::unit_production_info(unit_type, nation_id);
}

auto ProductionViewModel::marketplace_buy(const QString& resource_key) -> bool {
  return trade(resource_key, Game::Command::TradeDirection::Buy);
}

auto ProductionViewModel::marketplace_sell(const QString& resource_key) -> bool {
  return trade(resource_key, Game::Command::TradeDirection::Sell);
}

auto ProductionViewModel::trade(const QString& resource_key,
                                Game::Command::TradeDirection direction) -> bool {
  m_host.ensure_initialized();
  if (m_context.world == nullptr || m_context.session == nullptr) {
    return false;
  }

  if (!selected_marketplace_state().value("has_marketplace").toBool()) {
    emit refused(tr("Select your marketplace to trade."));
    return false;
  }

  const auto resource_type = trade_resource_from_key(resource_key);
  if (!resource_type.has_value()) {
    emit refused(tr("Marketplace can trade only wood, stone, or iron."));
    return false;
  }

  const bool buying = direction == Game::Command::TradeDirection::Buy;
  auto& marketplace = m_context.session->marketplace();
  const bool allowed =
      buying ? marketplace.can_buy(
                   *m_context.world, m_context.local_owner_id, *resource_type)
             : marketplace.can_sell(
                   *m_context.world, m_context.local_owner_id, *resource_type);
  if (!allowed) {
    emit refused(
        buying
            ? tr("Not enough gold to buy %1.").arg(trade_resource_label(resource_key))
            : tr("Not enough %1 to sell.").arg(trade_resource_label(resource_key)));
    return false;
  }

  Game::Command::submit(
      *m_context.world,
      Game::Command::Source::LocalPlayer,
      m_context.local_owner_id,
      Game::Command::Trade{.resource = *resource_type, .direction = direction});

  emit player_state_stale();
  return true;
}

void ProductionViewModel::set_rally_at_screen(qreal sx, qreal sy) {
  m_host.ensure_initialized();
  if (m_context.production != nullptr && m_context.viewport != nullptr) {
    m_context.production->set_rally_at_screen(
        sx, sy, m_context.local_owner_id, *m_context.viewport);
  }
}

} // namespace App::ViewModels
