#pragma once

#include <QJsonObject>
#include <QQuaternion>
#include <QString>
#include <QVector3D>

#include <cstddef>
#include <cstdint>
#include <vector>

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

namespace Game::Systems {

class OwnerRegistry;

// A boulder needs this much speed to hurt anyone; slower ones only roll on.
inline constexpr float k_rockfall_lethal_speed = 2.5F;
// How far past the boulder's own radius a troop's soldiers are spread.
inline constexpr float k_rockfall_troop_reach = 1.9F;
// The AI holds its rocks this long once the first enemy troop is in the zone,
// hoping more of the column follows it in.
inline constexpr float k_rockfall_ai_patience_seconds = 3.0F;
inline constexpr float k_rockfall_settled_linger_seconds = 8.0F;
inline constexpr float k_rockfall_max_boulder_age_seconds = 30.0F;

// Rockfall ambushes: defenders on the heights roll boulders down onto a pass.
// Boulders are physics-lite: they roll along the terrain gradient, hop off
// convex breaks, bounce on landing and come to rest on flat ground. Each one
// strikes a troop at most once, killing soldiers in proportion to its speed,
// flinging the dead and knocking the survivors down.
class RockfallSystem : public Engine::Core::System {
public:
  struct Services {
    Game::Map::TerrainService& terrain;
    OwnerRegistry& owners;
  };

  struct TrapView {
    QString id;
    QVector3D release_world;
    QVector3D target_world;
    Game::Map::RockfallTriggerMode trigger = Game::Map::RockfallTriggerMode::Zone;
    int owner_id = -1;
    int times_fired = 0;
    bool armed = true;
  };

  struct BoulderView {
    QVector3D position;
    QVector3D velocity;
    QQuaternion orientation;
    float radius = 0.0F;
    bool airborne = false;
    bool settled = false;
    int troops_struck = 0;
  };

  explicit RockfallSystem(Services services);
  ~RockfallSystem() override;

  void configure(const Game::Map::MapDefinition& map_definition);
  void restore_state(const QJsonObject& state);
  [[nodiscard]] auto serialize_state() const -> QJsonObject;

  void update(Engine::Core::World* world, float delta_time) override;

  // Mission scripts and tests release a trap by id. Returns false when the trap
  // is unknown, still rearming, or already spent.
  auto trigger(const QString& trap_id) -> bool;

  [[nodiscard]] auto trap_count() const -> std::size_t { return m_traps.size(); }
  [[nodiscard]] auto trap(std::size_t index) const -> TrapView;
  [[nodiscard]] auto boulders() const -> std::vector<BoulderView>;
  [[nodiscard]] auto total_troops_struck() const -> int { return m_total_strikes; }

private:
  struct RuntimeTrap {
    Game::Map::RockfallTrap definition;
    QVector3D release_world;
    QVector3D target_world;
    bool armed = true;
    int times_fired = 0;
    float rearm_remaining = 0.0F;
    int pending_releases = 0;
    int released_in_volley = 0;
    float release_timer = 0.0F;
    float ai_dwell = 0.0F;
  };

  struct Boulder {
    QVector3D position;
    QVector3D velocity;
    QQuaternion orientation;
    float radius = 0.55F;
    float age = 0.0F;
    float settled_age = 0.0F;
    float dust_timer = 0.0F;
    bool airborne = false;
    bool settled = false;
    int trap_index = -1;
    std::vector<Engine::Core::EntityID> struck;
  };

  struct Dust {
    QVector3D position;
    float scale = 1.0F;
    float age = 0.0F;
    float lifetime = 1.3F;
  };

  [[nodiscard]] auto to_world(const Game::Map::MapDefinition& map_definition,
                              float x,
                              float z) const -> QVector3D;
  void fire(RuntimeTrap& trap);
  void evaluate_auto_trigger(Engine::Core::World& world,
                             RuntimeTrap& trap,
                             float delta_time);
  [[nodiscard]] auto hostile_troops_in_zone(Engine::Core::World& world,
                                            const RuntimeTrap& trap) const -> int;
  [[nodiscard]] auto is_hostile(int trap_owner, int other_owner) const -> bool;
  void release_boulder(RuntimeTrap& trap, int trap_index);
  void step_boulder(Engine::Core::World& world, Boulder& boulder, float dt);
  void strike_troops(Engine::Core::World& world, Boulder& boulder);
  void strike(Engine::Core::World& world,
              Boulder& boulder,
              Engine::Core::Entity& target,
              float speed);
  void add_dust(const QVector3D& position, float scale);
  void publish_render_views(Engine::Core::World& world) const;

  Services m_services;
  std::vector<RuntimeTrap> m_traps;
  std::vector<Boulder> m_boulders;
  std::vector<Dust> m_dust;
  float m_half_extent_x = 0.0F;
  float m_half_extent_z = 0.0F;
  int m_total_strikes = 0;
  std::uint32_t m_release_sequence = 0;
};

} // namespace Game::Systems
