#include "cursed_gold_vein_system.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>

#include <cmath>

#include "core/component.h"
#include "core/entity.h"
#include "core/event_manager.h"
#include "core/world.h"
#include "core/world_spatial_index.h"
#include "game/map/terrain_service.h"
#include "game/systems/combat_system/damage_processor.h"
#include "game/systems/economy_feedback.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/resource_types.h"
#include "game/visuals/building_asset_key.h"
#include "units/factory.h"
#include "units/spawn_type.h"
#include "units/unit.h"

namespace Game::Systems {

CursedGoldVeinSystem::CursedGoldVeinSystem(Services services)
    : m_services(services) {
}

CursedGoldVeinSystem::~CursedGoldVeinSystem() = default;

void CursedGoldVeinSystem::ensure_factory_registry() {
  if (m_factory_registry) {
    return;
  }
  m_factory_registry = std::make_shared<Game::Units::UnitFactoryRegistry>();
  Game::Units::register_built_in_units(*m_factory_registry);
}

void CursedGoldVeinSystem::configure(const Game::Map::MapDefinition& map_definition) {
  (void)map_definition;
  m_veins.clear();
  ensure_factory_registry();

  const auto& terrain = m_services.terrain;
  for (const auto& prop : terrain.world_props()) {
    if (prop.type != Game::Map::WorldProp::Type::CursedGoldVein) {
      continue;
    }
    RuntimeVein vein;
    vein.prop_id = prop.id;
    vein.world_position = terrain.world_prop_footprint_world_position(
        prop, Game::Map::world_prop_ground_bounding_radius(prop.type, prop.scale));
    m_veins.push_back(vein);
  }
}

void CursedGoldVeinSystem::restore_state(const QJsonArray& state) {
  for (const auto value : state) {
    const QJsonObject obj = value.toObject();
    const auto prop_id = static_cast<std::uint64_t>(obj.value("prop_id").toDouble(0.0));
    for (auto& vein : m_veins) {
      if (vein.prop_id != prop_id) {
        continue;
      }
      vein.anchor_entity_id = static_cast<Engine::Core::EntityID>(
          obj.value("anchor_entity_id").toDouble(0.0));
      vein.anchor_pending = vein.anchor_entity_id == 0;
      vein.destroyed = obj.value("destroyed").toBool(false);
      vein.owner_id = obj.value("owner_id").toInt(Game::Core::NEUTRAL_OWNER_ID);
      vein.tick_elapsed = static_cast<float>(obj.value("tick_elapsed").toDouble(0.0));
      vein.ticks_paid = obj.value("ticks_paid").toInt(0);
      break;
    }
  }
}

auto CursedGoldVeinSystem::serialize_state() const -> QJsonArray {
  QJsonArray out;
  for (const auto& vein : m_veins) {
    QJsonObject obj;
    obj["prop_id"] = static_cast<double>(vein.prop_id);
    obj["anchor_entity_id"] = static_cast<double>(vein.anchor_entity_id);
    obj["destroyed"] = vein.destroyed;
    obj["owner_id"] = vein.owner_id;
    obj["tick_elapsed"] = static_cast<double>(vein.tick_elapsed);
    obj["ticks_paid"] = vein.ticks_paid;
    out.append(obj);
  }
  return out;
}

void CursedGoldVeinSystem::ensure_anchor(Engine::Core::World& world,
                                         RuntimeVein& vein) {
  if (!vein.anchor_pending || m_factory_registry == nullptr) {
    return;
  }
  vein.anchor_pending = false;

  Game::Units::SpawnParams params;
  params.position = vein.world_position;
  params.player_id = Game::Core::NEUTRAL_OWNER_ID;
  params.spawn_type = Game::Units::SpawnType::Barracks;
  params.ai_controlled = false;
  params.is_initial_spawn = true;
  params.enables_production = false;
  params.max_population = 0;

  auto anchor =
      m_factory_registry->create(Game::Units::SpawnType::Barracks, world, params);
  if (!anchor) {
    return;
  }
  vein.anchor_entity_id = anchor->id();

  auto* renderable =
      world.try_get<Engine::Core::RenderableComponent>(vein.anchor_entity_id);
  if (renderable != nullptr) {
    renderable->renderer_id =
        std::string(Game::Visuals::k_cursed_gold_vein_flag_asset_key);
  }
}

void CursedGoldVeinSystem::refresh_anchor(Engine::Core::World& world,
                                          RuntimeVein& vein) {
  if (vein.anchor_entity_id == 0 || vein.destroyed) {
    return;
  }
  auto* unit = world.try_get<Engine::Core::UnitComponent>(vein.anchor_entity_id);
  if (unit == nullptr || unit->health <= 0) {
    // A razed claim is inert for the rest of the match: the gold is buried again.
    vein.destroyed = true;
    vein.owner_id = Game::Core::NEUTRAL_OWNER_ID;
    return;
  }

  // Capture hands a barracks a production line; a vein must never train anything.
  world.remove<Engine::Core::ProductionComponent>(vein.anchor_entity_id);

  if (unit->owner_id != vein.owner_id) {
    vein.owner_id = unit->owner_id;
    vein.tick_elapsed = 0.0F;
    if (!Game::Core::is_neutral_owner(vein.owner_id)) {
      announce_claim(vein);
    }
  }
}

void CursedGoldVeinSystem::announce_claim(const RuntimeVein& vein) const {
  if (vein.owner_id != m_services.owners.get_local_player_id()) {
    return;
  }
  Engine::Core::EventManager::instance().publish(
      Engine::Core::MissionAnnouncementEvent(QCoreApplication::translate(
          "CursedGoldVeinSystem",
          "The cursed vein is yours. It bleeds gold - and the men who guard it.")));
}

void CursedGoldVeinSystem::apply_tick(Engine::Core::World& world, RuntimeVein& vein) {
  ++vein.ticks_paid;

  m_services.economy.add(
      vein.owner_id, ResourceType::Gold, k_cursed_gold_vein_gold_per_tick);
  publish_resource_feedback(vein.owner_id,
                            vein.anchor_entity_id,
                            ResourceType::Gold,
                            k_cursed_gold_vein_gold_per_tick);

  // The curse: every one of the owner's troops standing near the vein loses
  // manpower. Buildings and other owners' troops are untouched.
  std::vector<Engine::Core::EntityID> victims;
  auto& index = world.spatial_index();
  index.refresh(world);
  index.for_each_in_radius(
      vein.world_position.x(),
      vein.world_position.z(),
      k_cursed_gold_vein_curse_radius,
      [&](const Engine::Core::WorldSpatialIndex::Entry& entry) {
        if (!entry.is(Engine::Core::WorldSpatialIndex::k_alive) ||
            entry.is(Engine::Core::WorldSpatialIndex::k_building) ||
            entry.is(Engine::Core::WorldSpatialIndex::k_wildlife)) {
          return;
        }
        if (entry.owner_id != vein.owner_id) {
          return;
        }
        victims.push_back(entry.id);
      });

  for (Engine::Core::EntityID const victim_id : victims) {
    auto* unit = world.try_get<Engine::Core::UnitComponent>(victim_id);
    if (unit == nullptr || unit->health <= 0 ||
        !Game::Units::is_troop_spawn(unit->spawn_type)) {
      continue;
    }
    Combat::deal_damage(
        &world, world.get_entity(victim_id), k_cursed_gold_vein_curse_damage, 0);
  }
}

void CursedGoldVeinSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  for (auto& vein : m_veins) {
    ensure_anchor(*world, vein);
    refresh_anchor(*world, vein);

    if (vein.destroyed || Game::Core::is_neutral_owner(vein.owner_id)) {
      vein.tick_elapsed = 0.0F;
      continue;
    }

    vein.tick_elapsed += delta_time;
    while (vein.tick_elapsed >= k_cursed_gold_vein_tick_seconds) {
      vein.tick_elapsed -= k_cursed_gold_vein_tick_seconds;
      apply_tick(*world, vein);
    }
  }
}

auto CursedGoldVeinSystem::anchor_entity(std::size_t index) const
    -> Engine::Core::EntityID {
  return index < m_veins.size() ? m_veins[index].anchor_entity_id : 0;
}

auto CursedGoldVeinSystem::vein_world_position(std::size_t index) const -> QVector3D {
  return index < m_veins.size() ? m_veins[index].world_position : QVector3D{};
}

auto CursedGoldVeinSystem::vein_owner(std::size_t index) const -> int {
  return index < m_veins.size() ? m_veins[index].owner_id
                                : Game::Core::NEUTRAL_OWNER_ID;
}

auto CursedGoldVeinSystem::vein_markers() const -> std::vector<VeinMarker> {
  std::vector<VeinMarker> markers;
  markers.reserve(m_veins.size());
  for (const auto& vein : m_veins) {
    markers.push_back({vein.world_position, vein.owner_id, vein.destroyed});
  }
  return markers;
}

} // namespace Game::Systems
