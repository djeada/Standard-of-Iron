#pragma once

#include "../core/entity.h"
#include "../core/system.h"
#include <QObject>
#include <functional>
#include <vector>

namespace Engine {
namespace Core {
class Entity;
class World;
}
} // namespace Engine

namespace Game::Systems {

class PickingService;

class SelectionSystem : public Engine::Core::System {
public:
  void update(Engine::Core::World *world, float deltaTime) override;

  void selectUnit(Engine::Core::EntityID unitId);
  void deselectUnit(Engine::Core::EntityID unitId);
  void clearSelection();
  void selectUnitsInArea(float x1, float y1, float x2, float y2);

  const std::vector<Engine::Core::EntityID> &getSelectedUnits() const {
    return m_selectedUnits;
  }

private:
  std::vector<Engine::Core::EntityID> m_selectedUnits;
  bool isUnitInArea(Engine::Core::Entity *entity, float x1, float y1, float x2,
                    float y2);
};

class SelectionController : public QObject {
  Q_OBJECT
public:
  SelectionController(Engine::Core::World *world,
                      SelectionSystem *selectionSystem,
                      PickingService *pickingService, QObject *parent = nullptr);

  void onClickSelect(qreal sx, qreal sy, bool additive, int viewportWidth,
                     int viewportHeight, void *camera, int localOwnerId);
  void onAreaSelected(qreal x1, qreal y1, qreal x2, qreal y2, bool additive,
                      int viewportWidth, int viewportHeight, void *camera,
                      int localOwnerId);
  void onRightClickClearSelection();

  bool hasUnitsSelected() const;
  void getSelectedUnitIds(std::vector<Engine::Core::EntityID> &out) const;
  bool hasSelectedType(const QString &type) const;

signals:
  void selectionChanged();
  void selectionModelRefreshRequested();

private:
  Engine::Core::World *m_world;
  SelectionSystem *m_selectionSystem;
  PickingService *m_pickingService;

  void syncSelectionFlags();
};

} // namespace Game::Systems