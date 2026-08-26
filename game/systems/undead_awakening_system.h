#pragma once

#include <QHash>
#include <QJsonArray>
#include <QString>
#include <QVector3D>

#include <cstdint>
#include <memory>
#include <vector>

#include "game/core/system.h"
#include "game/map/undead_shrine_placement.h"
#include "game/systems/undead_zone_query.h"
#include "map/map_definition.h"

namespace Engine::Core {
using EntityID = std::uint64_t;
class World;
} // namespace Engine::Core

namespace Game::Map {
class TerrainService;
}

namespace Game::Units {
class UnitFactoryRegistry;
}

namespace Game::Systems {

class GlobalStatsRegistry;
class NationRegistry;
class OwnerRegistry;
class PlayerResourceRegistry;

class UndeadAwakeningSystem : public Engine::Core::System, public UndeadZoneQuery {
public:
  struct Services {
    Game::Map::TerrainService& terrain;
    OwnerRegistry& owners;
    NationRegistry& nations;
    GlobalStatsRegistry& stats;
    PlayerResourceRegistry& economy;
  };

  explicit UndeadAwakeningSystem(Services services);
  ~UndeadAwakeningSystem() override;

  void configure(const Game::Map::MapDefinition& map_definition);
  void restore_state(const QJsonArray& state);
  [[nodiscard]] auto serialize_state() const -> QJsonArray;

  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto has_zone(const QString& zone_id) const -> bool override;
  [[nodiscard]] auto is_zone_cleared(const QString& zone_id) const -> bool override;
  [[nodiscard]] auto is_shrine_purified(const QString& zone_id) const -> bool override;
  [[nodiscard]] auto completed_wave_count(const QString& zone_id) const -> int override;

  [[nodiscard]] auto
  anchor_entity(const QString& zone_id) const -> Engine::Core::EntityID;

  [[nodiscard]] auto has_shrine(const QString& zone_id) const -> bool;
  [[nodiscard]] auto shrine_world_position(const QString& zone_id) const -> QVector3D;
  [[nodiscard]] auto shrine_prop_id(const QString& zone_id) const -> std::uint64_t;

  [[nodiscard]] auto zones_without_shrine() const -> std::vector<QString>;

  struct ShrineMarker {
    QString zone_id;
    QVector3D world_position;
    bool awakened = false;
    bool cleared = false;
  };

  [[nodiscard]] auto shrine_markers() const -> std::vector<ShrineMarker>;

private:
  struct RuntimeZone {
    Game::Map::UndeadZone definition;
    QVector3D center_world;
    QVector3D anchor_world;
    QVector3D shrine_world;
    std::uint64_t anchor_world_prop_id = 0;
    std::uint64_t shrine_world_prop_id = 0;
    bool shrine_placed = false;
    Engine::Core::EntityID anchor_entity_id = 0;
    bool anchor_pending = false;
    bool awakened = false;
    bool garrison_broken = false;
    bool announced_awakening = false;
    bool announced_defeat = false;
    int next_wave_index = 0;
    int completed_waves = 0;
    float respawn_delay_remaining = 0.0F;
    float current_wave_elapsed = 0.0F;
    std::vector<Engine::Core::EntityID> active_spawn_ids;
  };

  void ensure_factory_registry();
  void ensure_zone_owner_registered(const RuntimeZone& zone) const;
  void place_zone_shrine(const Game::Map::MapDefinition& map_definition,
                         RuntimeZone& zone,
                         Game::Map::UndeadShrineExclusions& exclusions) const;
  void ensure_anchor_structure(Engine::Core::World& world, RuntimeZone& zone);
  void refresh_active_spawns(Engine::Core::World& world, RuntimeZone& zone) const;
  void refresh_anchor_structure(Engine::Core::World& world, RuntimeZone& zone);
  void break_garrison(Engine::Core::World& world, RuntimeZone& zone, bool captured);
  void pay_clear_reward(Engine::Core::World& world,
                        const RuntimeZone& zone,
                        bool captured) const;
  void refresh_capture_lock(Engine::Core::World& world, const RuntimeZone& zone) const;
  void awaken_zone(Engine::Core::World& world, RuntimeZone& zone);
  void try_spawn_next_wave(Engine::Core::World& world, RuntimeZone& zone);
  void announce_wave(const RuntimeZone& zone) const;
  [[nodiscard]] auto should_awaken_zone(Engine::Core::World& world,
                                        const RuntimeZone& zone) const -> bool;
  [[nodiscard]] auto can_spawn_wave(const RuntimeZone& zone) const -> bool;
  [[nodiscard]] auto spawn_position_for_index(const RuntimeZone& zone,
                                              int spawn_index,
                                              int spawn_count) const -> QVector3D;
  [[nodiscard]] auto find_zone(const QString& zone_id) const -> const RuntimeZone*;
  [[nodiscard]] auto find_zone_mutable(const QString& zone_id) -> RuntimeZone*;

  Services m_services;
  std::vector<RuntimeZone> m_zones;
  QHash<QString, int> m_zone_index;
  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory_registry;
  bool m_allow_mission_start_trigger = false;
};

} // namespace Game::Systems
