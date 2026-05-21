#pragma once

#include <QHash>
#include <QJsonArray>
#include <QString>
#include <QVector3D>

#include <cstdint>
#include <memory>
#include <vector>

#include "game/core/system.h"
#include "game/systems/undead_zone_query.h"
#include "map/map_definition.h"

namespace Engine::Core {
using EntityID = unsigned int;
class World;
} // namespace Engine::Core

namespace Game::Units {
class UnitFactoryRegistry;
}

namespace Game::Systems {

class UndeadAwakeningSystem : public Engine::Core::System, public UndeadZoneQuery {
public:
  UndeadAwakeningSystem();
  ~UndeadAwakeningSystem() override;

  void configure(const Game::Map::MapDefinition& map_definition);
  void restore_state(const QJsonArray& state);
  [[nodiscard]] auto serialize_state() const -> QJsonArray;

  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto has_zone(const QString& zone_id) const -> bool override;
  [[nodiscard]] auto is_zone_cleared(const QString& zone_id) const -> bool override;
  [[nodiscard]] auto is_shrine_purified(const QString& zone_id) const -> bool override;
  [[nodiscard]] auto completed_wave_count(const QString& zone_id) const -> int override;

private:
  struct RuntimeZone {
    Game::Map::UndeadZone definition;
    QVector3D center_world;
    QVector3D anchor_world;
    std::uint64_t anchor_world_prop_id = 0;
    bool awakened = false;
    int next_wave_index = 0;
    int completed_waves = 0;
    float respawn_delay_remaining = 0.0F;
    std::vector<Engine::Core::EntityID> active_spawn_ids;
  };

  void ensure_factory_registry();
  void ensure_zone_owner_registered(const RuntimeZone& zone) const;
  void refresh_active_spawns(Engine::Core::World& world, RuntimeZone& zone) const;
  void awaken_zone(Engine::Core::World& world, RuntimeZone& zone);
  void try_spawn_next_wave(Engine::Core::World& world, RuntimeZone& zone);
  [[nodiscard]] auto should_awaken_zone(Engine::Core::World& world,
                                        const RuntimeZone& zone) const -> bool;
  [[nodiscard]] auto can_spawn_wave(const RuntimeZone& zone) const -> bool;
  [[nodiscard]] auto spawn_position_for_index(const RuntimeZone& zone,
                                              int spawn_index) const -> QVector3D;
  [[nodiscard]] auto find_zone(const QString& zone_id) const -> const RuntimeZone*;
  [[nodiscard]] auto find_zone_mutable(const QString& zone_id) -> RuntimeZone*;

  std::vector<RuntimeZone> m_zones;
  QHash<QString, int> m_zone_index;
  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory_registry;
  bool m_allow_mission_start_trigger = false;
};

} // namespace Game::Systems
