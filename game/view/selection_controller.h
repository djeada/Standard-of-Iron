#pragma once

#include <QObject>
#include <QString>

#include <functional>
#include <vector>

#include "../core/entity.h"
#include "../systems/selection_system.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {

class PickingService;

class SelectionController : public QObject {
  Q_OBJECT
public:
  SelectionController(Engine::Core::World* world,
                      SelectionSystem* selection_system,
                      PickingService* picking_service,
                      QObject* parent = nullptr);

  void on_click_select(qreal sx,
                       qreal sy,
                       bool additive,
                       int viewport_width,
                       int viewport_height,
                       void* camera,
                       int local_owner_id);
  void on_area_selected(qreal x1,
                        qreal y1,
                        qreal x2,
                        qreal y2,
                        bool additive,
                        int viewport_width,
                        int viewport_height,
                        void* camera,
                        int local_owner_id);
  void on_right_click_clear_selection();
  void select_all_player_troops(int local_owner_id);
  void select_single_unit(Engine::Core::EntityID id, int local_owner_id);
  [[nodiscard]] auto can_inspect(Engine::Core::EntityID entity_id,
                                 int local_owner_id) const -> bool;
  using InspectFilter = std::function<bool(Engine::Core::EntityID)>;
  void set_inspect_filter(InspectFilter filter) {
    m_inspect_filter = std::move(filter);
  }
  void select_selected_units_by_type(const QString& unit_type, int local_owner_id);

  [[nodiscard]] auto has_units_selected() const -> bool;
  void get_selected_unit_ids(std::vector<Engine::Core::EntityID>& out) const;
  [[nodiscard]] auto has_selected_type(const QString& type) const -> bool;

signals:
  void selection_changed();
  void selection_model_refresh_requested();

private:
  Engine::Core::World* m_world;
  SelectionSystem* m_selection_system;
  PickingService* m_picking_service;
  InspectFilter m_inspect_filter;

  void sync_selection_flags();
};

} // namespace Game::Systems
