#pragma once

#include "../core/entity.h"
#include "../core/system.h"
#include <QObject>
#include <functional>
#include <vector>

namespace Engine::Core {
class Entity;
class World;
} // namespace Engine::Core

namespace Game::Systems {

class PickingService;

class SelectionSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float delta_time) override;

  void select_unit(Engine::Core::EntityID unit_id);
  void deselect_unit(Engine::Core::EntityID unit_id);
  void clear_selection();
  void select_units_in_area(float x1, float y1, float x2, float y2);

  [[nodiscard]] auto
  get_selected_units() const -> const std::vector<Engine::Core::EntityID> & {
    return m_selected_units;
  }

private:
  std::vector<Engine::Core::EntityID> m_selected_units;
  static auto is_unit_in_area(Engine::Core::Entity *entity, float x1, float y1,
                              float x2, float y2) -> bool;
};

class SelectionController : public QObject {
  Q_OBJECT
public:
  SelectionController(Engine::Core::World *world,
                      SelectionSystem *selection_system,
                      PickingService *picking_service,
                      QObject *parent = nullptr);

  void on_click_select(qreal sx, qreal sy, bool additive, int viewport_width,
                       int viewport_height, void *camera, int local_owner_id);
  void on_area_selected(qreal x1, qreal y1, qreal x2, qreal y2, bool additive,
                        int viewport_width, int viewport_height, void *camera,
                        int local_owner_id);
  void on_right_click_clear_selection();
  void select_all_player_troops(int local_owner_id);
  void select_single_unit(Engine::Core::EntityID id, int local_owner_id);

  [[nodiscard]] auto has_units_selected() const -> bool;
  void get_selected_unit_ids(std::vector<Engine::Core::EntityID> &out) const;
  [[nodiscard]] auto has_selected_type(const QString &type) const -> bool;

signals:
  void selection_changed();
  void selection_model_refresh_requested();

private:
  Engine::Core::World *m_world;
  SelectionSystem *m_selection_system;
  PickingService *m_picking_service;

  void sync_selection_flags();
};

} // namespace Game::Systems
