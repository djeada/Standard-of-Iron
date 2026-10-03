#pragma once

#include <QObject>
#include <QString>
#include <QVector3D>

#include <cstdint>
#include <vector>

#include "game/systems/nation_id.h"
#include "game/systems/wall_plan_service.h"

namespace Engine::Core {
class World;
using EntityID = std::uint64_t;
} // namespace Engine::Core

namespace App::Economy {

struct PreviewOwner {
  int owner_id = 0;
  Game::Systems::NationID nation_id{Game::Systems::NationID::RomanRepublic};
};

class ConstructionPreview : public QObject {
  Q_OBJECT

public:
  explicit ConstructionPreview(Engine::Core::World* world, QObject* parent = nullptr);

  void clear_entities();
  void hand_to_construction_site(const QString& item_type);
  void show_structure(const QString& item_type,
                      const QVector3D& world_position,
                      float rotation_y,
                      const PreviewOwner& owner);
  void show_wall_plan(const std::vector<Game::Systems::PlannedWallSegment>& segments,
                      bool gate,
                      const PreviewOwner& owner,
                      bool ladder = false);

  void set_active(bool active);
  void set_valid(bool valid);
  void set_ruling(bool valid, const QString& reason);
  void set_summary(int segment_count, int valid_segment_count, int total_cost);
  void clear_summary() { set_summary(0, 0, 0); }

  [[nodiscard]] auto active() const -> bool { return m_active; }
  [[nodiscard]] auto valid() const -> bool { return m_valid; }
  [[nodiscard]] auto reason() const -> const QString& { return m_reason; }
  [[nodiscard]] auto segment_count() const -> int { return m_segment_count; }
  [[nodiscard]] auto valid_segment_count() const -> int {
    return m_valid_segment_count;
  }
  [[nodiscard]] auto total_cost() const -> int { return m_total_cost; }
  [[nodiscard]] auto entity_count() const -> std::size_t { return m_entity_ids.size(); }

signals:
  void active_changed();
  void valid_changed();
  void reason_changed();
  void summary_changed();

private:
  void set_reason(const QString& reason);

  Engine::Core::World* m_world;
  std::vector<Engine::Core::EntityID> m_entity_ids;
  bool m_active = false;
  bool m_valid = false;
  QString m_reason;
  int m_segment_count = 0;
  int m_valid_segment_count = 0;
  int m_total_cost = 0;
};

} // namespace App::Economy
