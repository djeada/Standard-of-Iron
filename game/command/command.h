#pragma once

#include <QVector3D>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "../core/component.h"
#include "../core/entity.h"
#include "../formation/army_formation_types.h"
#include "../systems/order_service.h"
#include "../systems/resource_types.h"
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

struct SetGateMode {
  std::vector<Engine::Core::EntityID> units;
  Engine::Core::GateComponent::ManualMode mode{
      Engine::Core::GateComponent::ManualMode::Automatic};
};

struct SetAutoGather {
  std::vector<Engine::Core::EntityID> units;
  bool active = true;

  std::string priority_product_type;
};

struct Produce {
  Engine::Core::EntityID building = Engine::Core::NULL_ENTITY;
  Game::Units::TroopType product = Game::Units::TroopType::Archer;
};

enum class TradeDirection : std::uint8_t {
  Buy,
  Sell
};
struct Trade {
  Game::Systems::ResourceType resource = Game::Systems::ResourceType::Wood;
  TradeDirection direction = TradeDirection::Buy;
};

enum class CommanderAbility : std::uint8_t {
  Aura,
  Rally,
  FlagRally
};
struct UseCommanderAbility {
  Engine::Core::EntityID commander = Engine::Core::NULL_ENTITY;
  CommanderAbility ability = CommanderAbility::Aura;
  QVector3D target;
};

struct SetFormationMode {
  std::vector<Engine::Core::EntityID> units;
  bool active = true;
};

struct DeployFormation {
  std::vector<Engine::Core::EntityID> units;
  QVector3D anchor;
  float facing = 0.0F;
  float frontage = 0.0F;
  float spacing = 1.0F;
  Game::Formation::ArmyFormationIntent intent =
      Game::Formation::ArmyFormationIntent::FactionDefault;
  Game::Formation::FormationDoctrineId doctrine;
  Game::Formation::ArmyFormationOptions options;
};

struct ReleaseFormation {
  std::vector<Engine::Core::EntityID> units;
};

struct StartConstruction {
  std::vector<Engine::Core::EntityID> units;
  std::string construction_type;
  QVector3D site;
  float rotation_y = 0.0F;
};

struct PlaceWallPlan {
  std::vector<Engine::Core::EntityID> units;
  bool gate = false;
  int anchor_x = 0;
  int anchor_z = 0;
  int target_x = 0;
  int target_z = 0;
  float rotation_y = 0.0F;
};

struct PlaceBuilding {
  std::string building_type;
  QVector3D position;
  float rotation_y = 0.0F;
};

struct DeliverCivilians {
  std::vector<Engine::Core::EntityID> units;
  Engine::Core::EntityID barracks = Engine::Core::NULL_ENTITY;
};

struct RepairStructure {
  std::vector<Engine::Core::EntityID> units;
  Engine::Core::EntityID structure = Engine::Core::NULL_ENTITY;
};

struct DismantleStructure {
  std::vector<Engine::Core::EntityID> units;
  Engine::Core::EntityID structure = Engine::Core::NULL_ENTITY;
};

struct StartHarvest {
  std::vector<Engine::Core::EntityID> units;
  std::string construction_type;
  Engine::Core::EntityID resource_target = Engine::Core::NULL_ENTITY;
  QVector3D site;
};

using Payload = std::variant<Move,
                             AttackTarget,
                             Stop,
                             SetHold,
                             SetGuard,
                             SetRunMode,
                             Patrol,
                             SetRallyPoint,
                             SetGateMode,
                             SetAutoGather,
                             Produce,
                             Trade,
                             UseCommanderAbility,
                             SetFormationMode,
                             DeployFormation,
                             ReleaseFormation,
                             StartConstruction,
                             StartHarvest,
                             DeliverCivilians,
                             RepairStructure,
                             DismantleStructure,
                             PlaceWallPlan,
                             PlaceBuilding>;

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
