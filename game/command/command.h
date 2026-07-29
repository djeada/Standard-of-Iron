#pragma once

#include <QVector3D>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "../core/entity.h"
#include "../systems/order_service.h"
#include "../units/troop_type.h"

namespace Game::Command {

enum class Source : std::uint8_t {
  LocalPlayer,
  AI,
  Replay,
  Script
};

struct Move {
  std::vector<Engine::Core::EntityID> units;

  std::vector<QVector3D> targets;

  std::vector<float> facing_angles;
  Game::Systems::MoveOrderKind kind = Game::Systems::MoveOrderKind::PlayerMove;
  bool preserve_formation_mode = false;
};

struct AttackTarget {
  std::vector<Engine::Core::EntityID> units;
  Engine::Core::EntityID target = Engine::Core::NULL_ENTITY;
  bool should_chase = true;
};

struct Stop {
  std::vector<Engine::Core::EntityID> units;
};

struct SetHold {
  std::vector<Engine::Core::EntityID> units;
  bool active = true;
};

struct SetGuard {
  std::vector<Engine::Core::EntityID> units;
  bool active = true;
  QVector3D anchor;
  bool has_anchor = false;
};

struct SetRunMode {
  std::vector<Engine::Core::EntityID> units;
  bool active = true;
};

struct Patrol {
  std::vector<Engine::Core::EntityID> units;
  QVector3D first_waypoint;
  QVector3D second_waypoint;
};

struct SetRallyPoint {
  Engine::Core::EntityID building = Engine::Core::NULL_ENTITY;
  QVector3D position;
};

struct Produce {
  Engine::Core::EntityID building = Engine::Core::NULL_ENTITY;
  Game::Units::TroopType product = Game::Units::TroopType::Archer;
};

using Payload = std::variant<Move,
                             AttackTarget,
                             Stop,
                             SetHold,
                             SetGuard,
                             SetRunMode,
                             Patrol,
                             SetRallyPoint,
                             Produce>;

struct Command {
  Source source = Source::LocalPlayer;

  int owner_id = 0;

  std::uint64_t submitted_tick = 0;
  Payload payload;
};

[[nodiscard]] auto source_name(Source source) -> const char*;

[[nodiscard]] auto payload_name(const Payload& payload) -> const char*;

[[nodiscard]] auto
affected_units(const Payload& payload) -> const std::vector<Engine::Core::EntityID>*;

} // namespace Game::Command
