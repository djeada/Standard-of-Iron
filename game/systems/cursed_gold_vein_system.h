#pragma once

#include <QJsonArray>
#include <QVector3D>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "game/core/ownership_constants.h"
#include "game/core/system.h"
#include "map/map_definition.h"

namespace Engine::Core {
using EntityID = std::uint64_t;
class World;
class Entity;
} // namespace Engine::Core

namespace Game::Map {
class TerrainService;
}

namespace Game::Units {
class UnitFactoryRegistry;
}

namespace Game::Systems {

class OwnerRegistry;
class PlayerResourceRegistry;

inline constexpr float k_cursed_gold_vein_tick_seconds = 6.0F;
inline constexpr int k_cursed_gold_vein_gold_per_tick = 25;
inline constexpr int k_cursed_gold_vein_curse_damage = 10;
inline constexpr float k_cursed_gold_vein_curse_radius = 9.0F;

class CursedGoldVeinSystem : public Engine::Core::System {
public:
  struct Services {
    Game::Map::TerrainService& terrain;
    OwnerRegistry& owners;
    PlayerResourceRegistry& economy;
  };

  explicit CursedGoldVeinSystem(Services services);
  ~CursedGoldVeinSystem() override;

  void configure(const Game::Map::MapDefinition& map_definition);
  void restore_state(const QJsonArray& state);
  [[nodiscard]] auto serialize_state() const -> QJsonArray;

  void update(Engine::Core::World* world, float delta_time) override;

  struct VeinMarker {
    QVector3D world_position;
    int owner_id = Game::Core::NEUTRAL_OWNER_ID;
    bool destroyed = false;
  };

  [[nodiscard]] auto vein_count() const -> std::size_t { return m_veins.size(); }
  [[nodiscard]] auto anchor_entity(std::size_t index) const -> Engine::Core::EntityID;
  [[nodiscard]] auto vein_world_position(std::size_t index) const -> QVector3D;
  [[nodiscard]] auto vein_owner(std::size_t index) const -> int;
  [[nodiscard]] auto vein_markers() const -> std::vector<VeinMarker>;

private:
  struct RuntimeVein {
    std::uint64_t prop_id = 0;
    QVector3D world_position;
    Engine::Core::EntityID anchor_entity_id = 0;
    bool anchor_pending = true;
    bool destroyed = false;
    int owner_id = Game::Core::NEUTRAL_OWNER_ID;
    float tick_elapsed = 0.0F;
    int ticks_paid = 0;
  };

  void ensure_factory_registry();
  void ensure_anchor(Engine::Core::World& world, RuntimeVein& vein);
  void refresh_anchor(Engine::Core::World& world, RuntimeVein& vein);
  void apply_tick(Engine::Core::World& world, RuntimeVein& vein);
  void announce_claim(const RuntimeVein& vein) const;

  Services m_services;
  std::vector<RuntimeVein> m_veins;
  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory_registry;
};

} // namespace Game::Systems
