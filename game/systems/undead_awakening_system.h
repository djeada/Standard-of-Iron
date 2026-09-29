#pragma once

#include <QHash>
#include <QJsonArray>
#include <QString>
#include <QVector3D>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "game/core/system.h"
#include "game/map/undead_shrine_placement.h"
#include "game/systems/undead_guardians.h"
#include "game/systems/undead_shrine.h"
#include "game/systems/undead_zone_music.h"
#include "game/systems/undead_zone_query.h"
#include "game/systems/undead_zone_runtime.h"
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

  void set_wave_multiplier(float multiplier);
  [[nodiscard]] auto wave_multiplier() const -> float { return m_wave_multiplier; }
  [[nodiscard]] auto wave_squad_count(const QString& zone_id,
                                      int wave_index) const -> int;
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

  [[nodiscard]] auto
  would_wake_a_zone(float world_x, float world_z, float body_radius) const -> bool;

  struct ShrineMarker {
    QString zone_id;
    QVector3D world_position;
    bool awakened = false;
    bool cleared = false;
  };

  [[nodiscard]] auto shrine_markers() const -> std::vector<ShrineMarker>;

private:
  void ensure_factory_registry();
  [[nodiscard]] auto
  build_zone(const Game::Map::MapDefinition& map_definition,
             const Game::Map::UndeadZone& zone_definition,
             Game::Map::UndeadShrineExclusions& shrine_exclusions) -> UndeadRuntimeZone;
  void match_anchor_prop(UndeadRuntimeZone& zone) const;
  void
  update_zone(Engine::Core::World& world, UndeadRuntimeZone& zone, float delta_time);
  void announce_zone_cleared(UndeadRuntimeZone& zone) const;
  void refresh_active_spawns(Engine::Core::World& world, UndeadRuntimeZone& zone) const;
  void awaken_zone(Engine::Core::World& world, UndeadRuntimeZone& zone, int woken_by);
  void spawn_wave_units(Engine::Core::World& world,
                        UndeadRuntimeZone& zone,
                        const Game::Map::UndeadWave& wave);
  void try_spawn_next_wave(Engine::Core::World& world, UndeadRuntimeZone& zone);
  void announce_wave(const UndeadRuntimeZone& zone) const;
  void begin_wave_interval(UndeadRuntimeZone& zone) const;
  void apply_wave_multiplier(UndeadRuntimeZone& zone) const;
  [[nodiscard]] auto
  should_awaken_zone(Engine::Core::World& world,
                     const UndeadRuntimeZone& zone) const -> std::optional<int>;
  [[nodiscard]] auto can_spawn_wave(const UndeadRuntimeZone& zone) const -> bool;
  [[nodiscard]] auto
  find_zone(const QString& zone_id) const -> const UndeadRuntimeZone*;
  [[nodiscard]] auto find_zone_mutable(const QString& zone_id) -> UndeadRuntimeZone*;

  Services m_services;
  UndeadShrine m_shrine;
  UndeadGuardians m_guardians;
  UndeadZoneMusic m_music;
  std::vector<UndeadRuntimeZone> m_zones;
  QHash<QString, int> m_zone_index;
  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory_registry;

  bool m_allow_mission_start_trigger = false;
  float m_wave_multiplier = 1.0F;
};

} // namespace Game::Systems
