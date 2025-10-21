#pragma once

#include "../models/cursor_manager.h"
#include "../models/cursor_mode.h"
#include "../models/hover_tracker.h"
#include "../utils/engine_view_helpers.h"
#include "../utils/movement_utils.h"
#include "../utils/selection_utils.h"
#include "game/audio/AudioEventHandler.h"
#include "game/core/event_manager.h"
#include "game/systems/game_state_serializer.h"
#include <QJsonObject>
#include <QList>
#include <QMatrix4x4>
#include <QObject>
#include <QPointF>
#include <QStringList>
#include <QVariant>
#include <QVector3D>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace Engine {
namespace Core {
class World;
using EntityID = unsigned int;
struct MovementComponent;
struct TransformComponent;
struct RenderableComponent;
} // namespace Core
} // namespace Engine

namespace Render {
namespace GL {
class Renderer;
class Camera;
class ResourceManager;
class GroundRenderer;
class TerrainRenderer;
class BiomeRenderer;
class RiverRenderer;
class RiverbankRenderer;
class BridgeRenderer;
class FogRenderer;
class StoneRenderer;
class PlantRenderer;
class PineRenderer;
struct IRenderPass;
} // namespace GL
} // namespace Render

namespace Game {
namespace Systems {
class SelectionSystem;
class SelectionController;
class ArrowSystem;
class PickingService;
class VictoryService;
class CameraService;
class SaveLoadService;
} // namespace Systems
namespace Map {
class MapCatalog;
}
} // namespace Game

namespace App {
namespace Controllers {
class CommandController;
}
} // namespace App

class QQuickWindow;

class GameEngine : public QObject {
  Q_OBJECT
public:
  GameEngine();
  ~GameEngine();

  Q_PROPERTY(QObject *selectedUnitsModel READ selectedUnitsModel NOTIFY
                 selectedUnitsChanged)
  Q_PROPERTY(bool paused READ paused WRITE setPaused)
  Q_PROPERTY(float timeScale READ timeScale WRITE setGameSpeed)
  Q_PROPERTY(QString victoryState READ victoryState NOTIFY victoryStateChanged)
  Q_PROPERTY(QString cursorMode READ cursorMode WRITE setCursorMode NOTIFY
                 cursorModeChanged)
  Q_PROPERTY(qreal globalCursorX READ globalCursorX NOTIFY globalCursorChanged)
  Q_PROPERTY(qreal globalCursorY READ globalCursorY NOTIFY globalCursorChanged)
  Q_PROPERTY(
      bool hasUnitsSelected READ hasUnitsSelected NOTIFY selectedUnitsChanged)
  Q_PROPERTY(
      int playerTroopCount READ playerTroopCount NOTIFY troopCountChanged)
  Q_PROPERTY(
      int maxTroopsPerPlayer READ maxTroopsPerPlayer NOTIFY troopCountChanged)
  Q_PROPERTY(
      QVariantList availableMaps READ availableMaps NOTIFY availableMapsChanged)
  Q_PROPERTY(bool mapsLoading READ mapsLoading NOTIFY mapsLoadingChanged)
  Q_PROPERTY(int enemyTroopsDefeated READ enemyTroopsDefeated NOTIFY
                 enemyTroopsDefeatedChanged)
  Q_PROPERTY(QVariantList ownerInfo READ getOwnerInfo NOTIFY ownerInfoChanged)
  Q_PROPERTY(int selectedPlayerId READ selectedPlayerId WRITE
                 setSelectedPlayerId NOTIFY selectedPlayerIdChanged)
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

  Q_INVOKABLE void onMapClicked(qreal sx, qreal sy);
  Q_INVOKABLE void onRightClick(qreal sx, qreal sy);
  Q_INVOKABLE void onClickSelect(qreal sx, qreal sy, bool additive = false);
  Q_INVOKABLE void onAreaSelected(qreal x1, qreal y1, qreal x2, qreal y2,
                                  bool additive = false);
  Q_INVOKABLE void selectAllTroops();
  Q_INVOKABLE void setHoverAtScreen(qreal sx, qreal sy);
  Q_INVOKABLE void onAttackClick(qreal sx, qreal sy);
  Q_INVOKABLE void onStopCommand();
  Q_INVOKABLE void onHoldCommand();
  Q_INVOKABLE bool anySelectedInHoldMode() const;
  Q_INVOKABLE void onPatrolClick(qreal sx, qreal sy);

  Q_INVOKABLE void cameraMove(float dx, float dz);
  Q_INVOKABLE void cameraElevate(float dy);
  Q_INVOKABLE void resetCamera();
  Q_INVOKABLE void cameraZoom(float delta);
  Q_INVOKABLE float cameraDistance() const;
  Q_INVOKABLE void cameraYaw(float degrees);
  Q_INVOKABLE void cameraOrbit(float yawDeg, float pitchDeg);
  Q_INVOKABLE void cameraOrbitDirection(int direction, bool shift);
  Q_INVOKABLE void cameraFollowSelection(bool enable);
  Q_INVOKABLE void cameraSetFollowLerp(float alpha);
  Q_INVOKABLE void startLoadingMaps();

  Q_INVOKABLE void setPaused(bool paused) { m_runtime.paused = paused; }
  Q_INVOKABLE void setGameSpeed(float speed) {
    m_runtime.timeScale = std::max(0.0f, speed);
  }
  bool paused() const { return m_runtime.paused; }
  float timeScale() const { return m_runtime.timeScale; }
  QString victoryState() const { return m_runtime.victoryState; }
  QString cursorMode() const;
  void setCursorMode(CursorMode mode);
  void setCursorMode(const QString &mode);
  qreal globalCursorX() const;
  qreal globalCursorY() const;
  bool hasUnitsSelected() const;
  int playerTroopCount() const;
  int maxTroopsPerPlayer() const { return m_level.maxTroopsPerPlayer; }
  int enemyTroopsDefeated() const;

  Q_INVOKABLE QVariantMap getPlayerStats(int ownerId) const;

  int selectedPlayerId() const { return m_selectedPlayerId; }
  void setSelectedPlayerId(int id) {
    if (m_selectedPlayerId != id) {
      m_selectedPlayerId = id;
      emit selectedPlayerIdChanged();
    }
  }
  QString lastError() const { return m_runtime.lastError; }
  Q_INVOKABLE void clearError() {
    if (!m_runtime.lastError.isEmpty()) {
      m_runtime.lastError = "";
      emit lastErrorChanged();
    }
  }

  Q_INVOKABLE bool hasSelectedType(const QString &type) const;
  Q_INVOKABLE void recruitNearSelected(const QString &unitType);
  Q_INVOKABLE QVariantMap getSelectedProductionState() const;
  Q_INVOKABLE QString getSelectedUnitsCommandMode() const;
  Q_INVOKABLE void setRallyAtScreen(qreal sx, qreal sy);
  Q_INVOKABLE QVariantList availableMaps() const;
  bool mapsLoading() const { return m_mapsLoading; }
  Q_INVOKABLE void
  startSkirmish(const QString &mapPath,
                const QVariantList &playerConfigs = QVariantList());
  Q_INVOKABLE void openSettings();
  Q_INVOKABLE void loadSave();
  Q_INVOKABLE void saveGame(const QString &filename = "savegame.json");
  Q_INVOKABLE void saveGameToSlot(const QString &slotName);
  Q_INVOKABLE void loadGameFromSlot(const QString &slotName);
  Q_INVOKABLE QVariantList getSaveSlots() const;
  Q_INVOKABLE void refreshSaveSlots();
  Q_INVOKABLE bool deleteSaveSlot(const QString &slotName);
  Q_INVOKABLE void exitGame();
  Q_INVOKABLE QVariantList getOwnerInfo() const;

  void setWindow(QQuickWindow *w) { m_window = w; }

  void ensureInitialized();
  void update(float dt);
  void render(int pixelWidth, int pixelHeight);

  void getSelectedUnitIds(std::vector<Engine::Core::EntityID> &out) const;
  bool getUnitInfo(Engine::Core::EntityID id, QString &name, int &health,
                   int &maxHealth, bool &isBuilding, bool &alive) const;

  bool hasPatrolPreviewWaypoint() const;
  QVector3D getPatrolPreviewWaypoint() const;

private:
  struct RuntimeState {
    bool initialized = false;
    bool paused = false;
    bool loading = false;
    float timeScale = 1.0f;
    int localOwnerId = 1;
    QString victoryState = "";
    CursorMode cursorMode{CursorMode::Normal};
    QString lastError = "";
    Qt::CursorShape currentCursor = Qt::ArrowCursor;
    int lastTroopCount = 0;
    std::uint64_t visibilityVersion = 0;
    float visibilityUpdateAccumulator = 0.0f;
    qreal lastCursorX = -1.0;
    qreal lastCursorY = -1.0;
    int selectionRefreshCounter = 0;
  };
  struct EntityCache {
    int playerTroopCount = 0;
    bool playerBarracksAlive = false;
    bool enemyBarracksAlive = false;
    int enemyBarracksCount = 0;

    void reset() {
      playerTroopCount = 0;
      playerBarracksAlive = false;
      enemyBarracksAlive = false;
      enemyBarracksCount = 0;
    }
  };
  struct ViewportState {
    int width = 0;
    int height = 0;
  };

  bool screenToGround(const QPointF &screenPt, QVector3D &outWorld);
  bool worldToScreen(const QVector3D &world, QPointF &outScreen) const;
  void syncSelectionFlags();
  void resetMovement(Engine::Core::Entity *entity);
  QObject *selectedUnitsModel();
  void onUnitSpawned(const Engine::Core::UnitSpawnedEvent &event);
  void onUnitDied(const Engine::Core::UnitDiedEvent &event);
  void rebuildEntityCache();
  void rebuildRegistriesAfterLoad();
  void rebuildBuildingCollisions();
  void restoreEnvironmentFromMetadata(const QJsonObject &metadata);
  void updateCursor(Qt::CursorShape newCursor);
  void setError(const QString &errorMessage);
  bool loadFromSlot(const QString &slot);
  bool saveToSlot(const QString &slot, const QString &title);
  Game::Systems::RuntimeSnapshot toRuntimeSnapshot() const;
  void applyRuntimeSnapshot(const Game::Systems::RuntimeSnapshot &snapshot);
  QByteArray captureScreenshot() const;

  std::unique_ptr<Engine::Core::World> m_world;
  std::unique_ptr<Render::GL::Renderer> m_renderer;
  std::unique_ptr<Render::GL::Camera> m_camera;
  std::shared_ptr<Render::GL::ResourceManager> m_resources;
  std::unique_ptr<Render::GL::GroundRenderer> m_ground;
  std::unique_ptr<Render::GL::TerrainRenderer> m_terrain;
  std::unique_ptr<Render::GL::BiomeRenderer> m_biome;
  std::unique_ptr<Render::GL::RiverRenderer> m_river;
  std::unique_ptr<Render::GL::RiverbankRenderer> m_riverbank;
  std::unique_ptr<Render::GL::BridgeRenderer> m_bridge;
  std::unique_ptr<Render::GL::FogRenderer> m_fog;
  std::unique_ptr<Render::GL::StoneRenderer> m_stone;
  std::unique_ptr<Render::GL::PlantRenderer> m_plant;
  std::unique_ptr<Render::GL::PineRenderer> m_pine;
  std::vector<Render::GL::IRenderPass *> m_passes;
  std::unique_ptr<Game::Systems::PickingService> m_pickingService;
  std::unique_ptr<Game::Systems::VictoryService> m_victoryService;
  std::unique_ptr<Game::Systems::SaveLoadService> m_saveLoadService;
  std::unique_ptr<CursorManager> m_cursorManager;
  std::unique_ptr<HoverTracker> m_hoverTracker;
  std::unique_ptr<Game::Systems::CameraService> m_cameraService;
  std::unique_ptr<Game::Systems::SelectionController> m_selectionController;
  std::unique_ptr<App::Controllers::CommandController> m_commandController;
  std::unique_ptr<Game::Map::MapCatalog> m_mapCatalog;
  std::unique_ptr<Game::Audio::AudioEventHandler> m_audioEventHandler;
  QQuickWindow *m_window = nullptr;
  RuntimeState m_runtime;
  ViewportState m_viewport;
  bool m_followSelectionEnabled = false;
  Game::Systems::LevelSnapshot m_level;
  QObject *m_selectedUnitsModel = nullptr;
  int m_enemyTroopsDefeated = 0;
  int m_selectedPlayerId = 1;
  QVariantList m_availableMaps;
  bool m_mapsLoading = false;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>
      m_unitDiedSubscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::UnitSpawnedEvent>
      m_unitSpawnedSubscription;
  EntityCache m_entityCache;
  Engine::Core::AmbientState m_currentAmbientState = Engine::Core::AmbientState::PEACEFUL;
  float m_ambientCheckTimer = 0.0f;

  void updateAmbientState(float dt);
  bool isPlayerInCombat() const;
  void loadAudioResources();
signals:
  void selectedUnitsChanged();
  void selectedUnitsDataChanged();
  void enemyTroopsDefeatedChanged();
  void victoryStateChanged();
  void cursorModeChanged();
  void globalCursorChanged();
  void troopCountChanged();
  void availableMapsChanged();
  void ownerInfoChanged();
  void selectedPlayerIdChanged();
  void lastErrorChanged();
  void mapsLoadingChanged();
  void saveSlotsChanged();
};
