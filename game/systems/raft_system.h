#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector3D>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "game/core/system.h"
#include "map/map_definition.h"

namespace Engine::Core {
using EntityID = std::uint64_t;
class World;
} // namespace Engine::Core

namespace Game::Map {
class TerrainService;
}

namespace Game::Systems {

inline constexpr float k_raft_min_river_width = 6.0F;

inline constexpr float k_raft_deck_half_length = 1.9F;

inline constexpr float k_raft_deck_half_width = 2.7F;

inline constexpr float k_raft_call_radius = 16.0F;

inline constexpr float k_raft_board_reach = 4.5F;

inline constexpr float k_raft_board_seconds = 1.4F;

inline constexpr float k_raft_approach_timeout = 40.0F;

inline constexpr float k_raft_deck_height = 0.32F;

class RaftSystem : public Engine::Core::System {
public:
  struct Services {
    Game::Map::TerrainService& terrain;
  };

  enum class Phase : std::uint8_t {
    Docked,
    Boarding,
    Crossing,
    Landing
  };

  struct RaftView {
    QString id;
    QVector3D position;
    QVector3D across;
    std::array<QVector3D, 2> landings;
    std::array<QVector3D, 2> docks;
    int side = 0;
    Phase phase = Phase::Docked;
    Engine::Core::EntityID passenger = 0;
    std::vector<Engine::Core::EntityID> queue;
  };

  struct Reach {
    std::size_t raft = 0;
    int side = 0;
  };

  explicit RaftSystem(Services services);
  ~RaftSystem() override;

  void configure(const Game::Map::MapDefinition& map_definition);
  void restore_state(const QJsonObject& state);
  [[nodiscard]] auto serialize_state() const -> QJsonObject;

  void update(Engine::Core::World* world, float delta_time) override;

  [[nodiscard]] auto
  raft_in_reach(Engine::Core::World& world,
                Engine::Core::EntityID unit) const -> std::optional<Reach>;

  auto order_crossing(Engine::Core::World& world, Engine::Core::EntityID unit) -> bool;

  [[nodiscard]] auto is_waiting_or_aboard(Engine::Core::EntityID unit) const -> bool;

  [[nodiscard]] auto raft_count() const -> std::size_t { return m_rafts.size(); }
  [[nodiscard]] auto raft(std::size_t index) const -> RaftView;

  [[nodiscard]] static auto river_admits_raft(float river_width) -> bool {
    return river_width >= k_raft_min_river_width;
  }

private:
  struct Request {
    Engine::Core::EntityID unit = 0;
    int side = 0;
    float waited = 0.0F;
    float reorder_cooldown = 0.0F;
  };

  struct Raft {
    QString id;
    QVector3D across;
    std::array<QVector3D, 2> docks;
    std::array<QVector3D, 2> landings;
    float speed = 1.6F;
    int side = 0;
    Phase phase = Phase::Docked;
    float elapsed = 0.0F;
    Engine::Core::EntityID passenger = 0;
    int saved_files_override = 0;
    int landed = 0;
    std::vector<Request> queue;
  };

  auto place(const Game::Map::RaftCrossing& definition, const QVector3D& at) -> bool;
  [[nodiscard]] auto
  find_landing(const QVector3D& water_edge,
               const QVector3D& outward) const -> std::optional<QVector3D>;
  [[nodiscard]] auto crossing_seconds(const Raft& raft) const -> float;
  [[nodiscard]] auto deck_position(const Raft& raft) const -> QVector3D;
  [[nodiscard]] auto passenger_position(const Raft& raft) const -> QVector3D;

  void advance(Engine::Core::World& world, Raft& raft, float dt);
  void prune_queue(Engine::Core::World& world, Raft& raft, float dt);
  void board(Engine::Core::World& world, Raft& raft, const Request& request);
  void hold_passenger(Engine::Core::World& world, Raft& raft);
  void release_passenger(Engine::Core::World& world, Raft& raft, bool landed);
  void publish_render_views(Engine::Core::World& world) const;
  void track_reach(Engine::Core::World& world);

  Services m_services;
  std::vector<Raft> m_rafts;
  std::vector<Engine::Core::EntityID> m_in_reach;
};

} // namespace Game::Systems
