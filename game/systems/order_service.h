#pragma once

#include <cstdint>

namespace Engine::Core {
class Entity;
}

namespace Game::Systems {

enum class MoveOrderKind : std::uint8_t {
  PlayerMove,
  FormationMove,
  AttackChase,
  GuardReturn,
  RecoveryMove,
  ScriptedMove
};

class OrderService {
public:
  static void reset_movement(Engine::Core::Entity* entity);
  static void clear_civilian_delivery(Engine::Core::Entity* entity);
  static void clear_patrol(Engine::Core::Entity* entity);
  static void clear_attack_target(Engine::Core::Entity* entity);
  static void clear_player_order_intent(Engine::Core::Entity* entity);
  static void exit_hold_mode(Engine::Core::Entity* entity);
  static void set_guard_mode_active(Engine::Core::Entity* entity, bool active);
  static void set_formation_mode_active(Engine::Core::Entity* entity, bool active);
  static void prepare_for_move(Engine::Core::Entity* entity,
                               MoveOrderKind kind,
                               bool preserve_formation_mode);
  static void prepare_for_attack(Engine::Core::Entity* entity);
  static void apply_stop(Engine::Core::Entity* entity);
};

[[nodiscard]] auto move_order_kind_name(MoveOrderKind kind) -> const char*;

} // namespace Game::Systems
