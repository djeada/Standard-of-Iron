#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariantMap>
#include <QVector3D>

#include <cstddef>
#include <vector>

#include "game/mission/mission_setup_coordinator.h"
#include "game/systems/convoy_query.h"

namespace Game::Mission {

class MissionConvoy final : public Game::Systems::ConvoyQuery {
public:
  void bind(const ConvoyDefinition& definition,
            Engine::Core::World& world,
            const Game::Systems::LevelSnapshot& level,
            int owner_id);
  void reset();
  void spawn_wagons();
  void assign_escort(Engine::Core::EntityID escort_id) const;
  void advance(float dt, float elapsed, std::vector<PendingMissionWave>& waves);
  void update_wave_checkpoints(std::vector<PendingMissionWave>& waves) const;

  [[nodiscard]] auto active() const -> bool { return !m_route.empty(); }
  [[nodiscard]] auto delivered_wagons() const -> int override;
  [[nodiscard]] auto available_wagons() const -> int override;
  [[nodiscard]] auto
  status(float elapsed,
         const std::vector<PendingMissionWave>& waves) const -> QVariantMap;
  [[nodiscard]] auto serialize() const -> QJsonObject;
  void restore(const QJsonObject& state);

private:
  struct Waypoint {
    QVector3D world;
    QString id;
    QString name;
    float distance = 0.0F;
    float reached_at = -1.0F;
  };

  struct Wagon {
    QString name;
    Engine::Core::EntityID id = 0;
    int last_health = 0;
    float last_hit_at = -10.0F;
    bool delivered = false;
  };

  [[nodiscard]] auto position_at(float distance) const -> QVector3D;
  [[nodiscard]] auto last_travel_distance() const -> float;
  [[nodiscard]] auto wagon_health(const Wagon& wagon) const -> int;
  [[nodiscard]] auto
  nearest_wagon(const QVector3D& from) const -> Engine::Core::EntityID;
  void place_wagon(Engine::Core::EntityID id, float distance);
  void reassign_stranded_escorts() const;

  Engine::Core::World* m_world = nullptr;
  int m_owner_id = 1;
  std::vector<Waypoint> m_route;
  std::vector<Wagon> m_wagons;
  float m_speed = 0.0F;
  float m_spacing = 0.0F;
  int m_max_health = 0;
  float m_departure_distance = 0.0F;
  float m_distance = 0.0F;
  std::size_t m_next_point = 0;
};

} // namespace Game::Mission
