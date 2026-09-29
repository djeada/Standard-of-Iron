#pragma once

#include <QImage>
#include <QObject>
#include <QString>
#include <QTimer>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

#include "app/persistence/save_load_coordinator.h"
#include "app/persistence/save_orchestrator.h"
#include "game/systems/persistence/save_format.h"

namespace Engine::Core {
class World;
}

namespace Game::Systems {
class SaveLoadService;
}

namespace App::ViewModels {
class SaveSlotsViewModel;
}

namespace App::Core {

struct SaveSlotHooks {
  std::function<bool()> simulation_running;
  std::function<SaveToSlotEffects(
      const QString& slot, Game::Systems::Save::SlotKind kind, int autosave_retention)>
      capture;
  std::function<std::unique_lock<std::recursive_mutex>()> lock_frame;
  std::function<void(const QString& message)> report_error;
  std::function<void()> leave_commander_mode;
  std::function<bool()> autosave_allowed;
};

class SaveSlotController {
public:
  SaveSlotController(Game::Systems::SaveLoadService* service,
                     App::ViewModels::SaveSlotsViewModel* slot_model,
                     Engine::Core::World* world,
                     SaveSlotHooks hooks,
                     QObject* context);

  void connect_service_signals();
  void connect_view_model();
  void restart_autosave_timer();
  void stop_autosave_timer() { m_autosave_timer.stop(); }
  void shutdown();

  void save_game_to_slot(const QString& slot_name);
  void quicksave();
  void autosave();
  void cancel_active_save();

  void drain_pending_capture();
  [[nodiscard]] auto last_capture_us() const -> std::uint64_t {
    return m_orchestrator.last_capture_us();
  }

  void finish_request(const QString& slot_name, const SaveToSlotEffects& effects);

  [[nodiscard]] auto consume_screenshot_request() -> bool {
    return m_screenshot_requested.exchange(false, std::memory_order_acq_rel);
  }
  void attach_screenshot(const QImage& image);

private:
  void begin_save(const QString& slot_name,
                  Game::Systems::Save::SlotKind kind,
                  int autosave_retention);
  void clear_progress();
  [[nodiscard]] auto queue_capture(const QString& slot_name,
                                   Game::Systems::Save::SlotKind kind,
                                   int autosave_retention) -> bool;

  Game::Systems::SaveLoadService* m_service;
  App::ViewModels::SaveSlotsViewModel* m_slots;
  Engine::Core::World* m_world;
  SaveSlotHooks m_hooks;
  QObject* m_context;
  SaveOrchestrator m_orchestrator;
  quint64 m_active_job = 0;
  std::atomic_bool m_screenshot_requested{false};
  QString m_screenshot_target_slot;
  QString m_progress_slot;
  QTimer m_autosave_timer;
};

} // namespace App::Core
