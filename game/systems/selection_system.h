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
  void update(Engine::Core::World *world, float deltaTime) override;

  void selectUnit(Engine::Core::EntityID unit_id);
  void deselectUnit(Engine::Core::EntityID unit_id);
  void clearSelection();
  void selectUnitsInArea(float x1, float y1, float x2, float y2);

  [[nodiscard]] auto
  getSelectedUnits() const -> const std::vector<Engine::Core::EntityID> & {
    return m_selectedUnits;
  }

private:
  std::vector<Engine::Core::EntityID> m_selectedUnits;
  static auto isUnitInArea(Engine::Core::Entity *entity, float x1, float y1,
                           float x2, float y2) -> bool;
};

class SelectionController : public QObject {
  Q_OBJECT
public:
  SelectionController(Engine::Core::World *world,
                      SelectionSystem *selection_system,
                      PickingService *pickingService,
                      QObject *parent = nullptr);

  void onClickSelect(qreal sx, qreal sy, bool additive, int viewportWidth,
                     int viewportHeight, void *camera, int localOwnerId);
  void onAreaSelected(qreal x1, qreal y1, qreal x2, qreal y2, bool additive,
                      int viewportWidth, int viewportHeight, void *camera,
                      int localOwnerId);
  void onRightClickClearSelection();
  void selectAllPlayerTroops(int localOwnerId);

  [[nodiscard]] auto hasUnitsSelected() const -> bool;
  void getSelectedUnitIds(std::vector<Engine::Core::EntityID> &out) const;
  [[nodiscard]] auto hasSelectedType(const QString &type) const -> bool;

signals:
  void selectionChanged();
  void selectionModelRefreshRequested();

private:
  Engine::Core::World *m_world;
  SelectionSystem *m_selection_system;
  PickingService *m_pickingService;

  void syncSelectionFlags();
};

} // namespace Game::Systems