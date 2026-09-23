#include "app/viewmodels/production_view_model.h"

#include <QColor>
#include <QCoreApplication>
#include <QStringView>

#include <optional>
#include <utility>
#include <vector>

#include "app/core/client_context.h"
#include "app/economy/production_manager.h"
#include "app/economy/production_readouts.h"
#include "app/orders/command_controller.h"
#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component_core.h"
#include "game/core/presentation_coverage.h"
#include "game/core/world.h"
#include "game/render_bridge/selection_controller.h"
#include "game/session/session_context.h"
#include "game/systems/marketplace_system.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/resource_types.h"
#include "game/units/spawn_type.h"

namespace App::ViewModels {
namespace {

auto trade_resource_from_key(QStringView key)
    -> std::optional<Game::Systems::ResourceType> {
  if (key == QLatin1String("food")) {
    return Game::Systems::ResourceType::Food;
  }
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
  if (key == QLatin1String("food")) {
    return QCoreApplication::translate("ProductionViewModel", "food");
  }
  if (key == QLatin1String("wood")) {
    return QCoreApplication::translate("ProductionViewModel", "wood");
  }
  if (key == QLatin1String("stone")) {
    return QCoreApplication::translate("ProductionViewModel", "stone");
  }
  if (key == QLatin1String("iron")) {
    return QCoreApplication::translate("ProductionViewModel", "iron");
  }
  if (key == QLatin1String("gold")) {
    return QCoreApplication::translate("ProductionViewModel", "gold");
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

void ProductionViewModel::publish_frame() {
  App::Core::SelectionReadout readout;

  if (m_context.selection != nullptr && m_context.world != nullptr) {
    std::vector<Engine::Core::EntityID> selected;
    m_context.selection->get_selected_unit_ids(selected);
    for (const auto id : selected) {
      const auto* unit = m_context.world->try_get<Engine::Core::UnitComponent>(id);
      if (unit == nullptr) {
        continue;
      }
      readout.selected_types.insert(
          QString::fromStdString(Game::Units::spawn_typeToString(unit->spawn_type)));
    }
  }

  const int owner = m_context.local_owner_id;
  readout.barracks = App::Economy::selected_barracks_state(m_context.world, owner);
  readout.home = App::Economy::selected_home_state(m_context.world, owner);
  readout.temple = App::Economy::selected_temple_state(m_context.world, owner);
  readout.builder = App::Economy::selected_builder_state(m_context.world);
  readout.marketplace =
      App::Economy::selected_marketplace_state(m_context.world, owner);
  readout.farm = App::Economy::selected_farm_state(m_context.world, owner);

  m_readout.publish(std::move(readout));
}

auto ProductionViewModel::has_selected_type(const QString& type) const -> bool {
  const auto readout = m_readout.read();
  return readout && readout->selected_types.contains(type);
}

void ProductionViewModel::recruit_near_selected(const QString& unit_type) {
  Engine::Core::note_coverage(Engine::Core::CoverageEvent::ProductionOrder);
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  if (m_context.commands != nullptr) {
    m_context.commands->recruit_near_selected(unit_type, m_context.local_owner_id);
  }
}

auto ProductionViewModel::selected_state() const -> QVariantMap {
  const auto readout = m_readout.read();
  return readout ? readout->barracks : QVariantMap{};
}

auto ProductionViewModel::selected_home_state() const -> QVariantMap {
  const auto readout = m_readout.read();
  return readout ? readout->home : QVariantMap{};
}

auto ProductionViewModel::selected_temple_state() const -> QVariantMap {
  const auto readout = m_readout.read();
  return readout ? readout->temple : QVariantMap{};
}

auto ProductionViewModel::selected_builder_state() const -> QVariantMap {
  const auto readout = m_readout.read();
  return readout ? readout->builder : QVariantMap{};
}

auto ProductionViewModel::selected_marketplace_state() const -> QVariantMap {
  const auto readout = m_readout.read();
  return readout ? readout->marketplace : QVariantMap{};
}

auto ProductionViewModel::selected_farm_state() const -> QVariantMap {
  const auto readout = m_readout.read();
  return readout ? readout->farm : QVariantMap{};
}

auto ProductionViewModel::unit_info(const QString& unit_type,
                                    const QString& nation_id) const -> QVariantMap {
  const auto frame_lock = m_host.lock_frame();
  return App::Economy::unit_production_info(
      m_context.session->nations(), unit_type, nation_id);
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
  const auto frame_lock = m_host.lock_frame();
  if (m_context.world == nullptr || m_context.session == nullptr) {
    return false;
  }

  if (!selected_marketplace_state().value("has_marketplace").toBool()) {
    emit refused(tr("Select your marketplace to trade."));
    return false;
  }

  const auto resource_type = trade_resource_from_key(resource_key);
  if (!resource_type.has_value()) {
    emit refused(tr("The marketplace does not trade that."));
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

auto ProductionViewModel::marketplace_allies() const -> QVariantList {
  const auto frame_lock = m_host.lock_frame();
  QVariantList allies;
  if (m_context.session == nullptr) {
    return allies;
  }
  const auto& owners = m_context.session->owners();
  for (const auto& owner : owners.get_all_owners()) {
    if (owner.owner_id == m_context.local_owner_id || owner.owner_id <= 0 ||
        !owners.are_allies(m_context.local_owner_id, owner.owner_id)) {
      continue;
    }
    QVariantMap entry;
    entry["owner_id"] = owner.owner_id;
    entry["name"] = QString::fromStdString(owner.name);
    entry["is_ai"] = owner.type == Game::Systems::OwnerType::AI;
    entry["color"] = QColor::fromRgbF(owner.color[0], owner.color[1], owner.color[2]);
    allies.append(entry);
  }
  return allies;
}

auto ProductionViewModel::send_to_ally(int ally_owner,
                                       const QString& resource_key,
                                       int amount) -> bool {
  return ally_tribute(ally_owner, resource_key, amount, false);
}

auto ProductionViewModel::request_from_ally(int ally_owner,
                                            const QString& resource_key,
                                            int amount) -> bool {
  return ally_tribute(ally_owner, resource_key, amount, true);
}

auto ProductionViewModel::ally_tribute(int ally_owner,
                                       const QString& resource_key,
                                       int amount,
                                       bool request) -> bool {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  if (m_context.world == nullptr || m_context.session == nullptr) {
    return false;
  }
  Game::Systems::ResourceType resource{};
  if (!Game::Systems::resource_type_from_key(resource_key, resource)) {
    emit refused(tr("Allies can only exchange gold, food, wood, stone or iron."));
    return false;
  }
  if (!m_context.session->owners().are_allies(m_context.local_owner_id, ally_owner) ||
      ally_owner == m_context.local_owner_id) {
    emit refused(tr("Choose an ally to trade with."));
    return false;
  }
  if (!Game::Systems::MarketplaceSystem::owner_has_marketplace(
          *m_context.world, m_context.local_owner_id)) {
    emit refused(tr("You need a marketplace to deal with your allies."));
    return false;
  }
  const int clamped = std::clamp(amount, 1, Game::Systems::k_max_ally_tribute);
  if (!request &&
      m_context.session->economy().get(m_context.local_owner_id, resource) < clamped) {
    emit refused(tr("Not enough %1 to send.").arg(trade_resource_label(resource_key)));
    return false;
  }
  Game::Command::submit(*m_context.world,
                        Game::Command::Source::LocalPlayer,
                        m_context.local_owner_id,
                        Game::Command::AllyTribute{.ally_owner = ally_owner,
                                                   .resource = resource,
                                                   .amount = clamped,
                                                   .request = request});
  emit player_state_stale();
  return true;
}

void ProductionViewModel::set_rally_at_screen(qreal sx, qreal sy) {
  m_host.ensure_initialized();
  const auto frame_lock = m_host.lock_frame();
  if (m_context.production != nullptr && m_context.viewport != nullptr) {
    m_context.production->set_rally_at_screen(
        sx, sy, m_context.local_owner_id, *m_context.viewport);
  }
}

} // namespace App::ViewModels
