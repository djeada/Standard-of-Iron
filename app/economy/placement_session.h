#pragma once

#include <QString>
#include <QVector3D>

#include <cstdint>
#include <vector>

#include "game/systems/nation_id.h"

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace App::Economy {

enum class PlacementKind : std::uint8_t {
  Structure,
  Wall,
  Gate,
  Ladder,
  Harvest
};

enum class PlacementPhase : std::uint8_t {
  Idle,
  Aiming,
  Previewing,
  DraggingWall
};

enum class PlacementEvent : std::uint8_t {
  Start,
  PointerMotion,
  PointerPress,
  PointerRelease,
  Rotate,
  Confirm,
  Cancel,
  MatchReset
};

[[nodiscard]] auto placement_kind_for(const QString& item_type) -> PlacementKind;
[[nodiscard]] auto is_previewable_structure_item(const QString& item_type) -> bool;
[[nodiscard]] auto item_supports_preview_rotation(const QString& item_type) -> bool;

[[nodiscard]] auto placement_phase(bool placing,
                                   bool wall_drag_active,
                                   bool preview_active) -> PlacementPhase;

[[nodiscard]] auto placement_allows(PlacementPhase phase,
                                    PlacementKind kind,
                                    PlacementEvent event) -> bool;

class PlacementSession {
public:
  void
  begin_direct(const QString& building_type, int owner_id, Game::Systems::NationID);
  void begin_with_builders(const QString& item_type,
                           std::vector<Engine::Core::EntityID> builders,
                           const QVector3D& center,
                           Engine::Core::World* world);
  void end();
  void forget_pending_building() { m_pending_building_type.clear(); }

  [[nodiscard]] auto active() const -> bool { return m_active; }
  [[nodiscard]] auto direct() const -> bool { return m_direct; }
  [[nodiscard]] auto pending_building_type() const -> const QString& {
    return m_pending_building_type;
  }
  [[nodiscard]] auto construction_type() const -> const QString& {
    return m_construction_type;
  }
  [[nodiscard]] auto kind() const -> PlacementKind {
    return placement_kind_for(m_construction_type);
  }
  [[nodiscard]] auto is_wall() const -> bool;
  [[nodiscard]] auto is_gate() const -> bool;
  [[nodiscard]] auto is_ladder() const -> bool;
  [[nodiscard]] auto builders() const -> const std::vector<Engine::Core::EntityID>& {
    return m_builders;
  }

  [[nodiscard]] auto position() const -> const QVector3D& { return m_position; }
  void set_position(const QVector3D& position) { m_position = position; }
  [[nodiscard]] auto rotation_y() const -> float { return m_rotation_y; }
  void set_rotation_y(float degrees) { m_rotation_y = degrees; }
  [[nodiscard]] auto effective_rotation_y() const -> float;

  [[nodiscard]] auto harvest_target_id() const -> std::uint64_t {
    return m_harvest_target_id;
  }
  [[nodiscard]] auto food_target_id() const -> Engine::Core::EntityID {
    return m_food_target_id;
  }
  void set_targets(std::uint64_t harvest_target, Engine::Core::EntityID food_target);
  void clear_targets() { set_targets(0, 0); }

  void set_owner_id(int owner_id) { m_owner_id = owner_id; }
  [[nodiscard]] auto owner_id(Engine::Core::World* world) const -> int;
  [[nodiscard]] auto
  nation_id(Engine::Core::World* world) const -> Game::Systems::NationID;

private:
  QString m_pending_building_type;
  QString m_construction_type;
  std::vector<Engine::Core::EntityID> m_builders;
  QVector3D m_position;
  bool m_active = false;
  bool m_direct = false;
  int m_owner_id = 0;
  Game::Systems::NationID m_nation_id{Game::Systems::NationID::RomanRepublic};
  float m_rotation_y = 0.0F;
  std::uint64_t m_harvest_target_id = 0;
  Engine::Core::EntityID m_food_target_id = 0;
};

} // namespace App::Economy
